# rpc

这是一个c++ header-only的远程过程调用项目，参考```rest_rpc```编写（感谢purecpp社区，后面会讲解进行的改进），支持客户端调用服务器端的函数（包括类成员函数）。

网络编程支持同步、异步两种方式，同步为从零实现，异步使用```boost::asio```网络库。

客户端支持同步和异步调用两种方式，同步会阻塞直到获取结果，异步将会获得```std::future<ReqResult>```，其声明和定义位于```rpc_client.hpp```，后续可自行管理。

传输协议使用```msgpack```（在本项目所需功能中，相比```json```更易用也更快）。

## 使用

确保本地环境有```g++、cmake、boost::asio```库，编译命令为```c++17```。

```common.hpp```下```log_dir```可以调整日志输出路径，```LOG_LEVEL```调整输出日志等级，为0时不进行任何日志输出（用于性能测试）。

同步内容（服务器、客户端、测试、性能测试demo）位于```./sync```下，异步位于```./async```下，先进入对应的目录

开启服务
```
cd build
cmake .. && make && ./server
```
测试
```
cd build
g++ -o test ../test.cpp -pthread && ./test
```
客户端可以参考```client.cpp```或测试程序编写

需要异步时，可参考以下代码，若调用函数返回值为```void```，请删除最后一行
```c++
auto future = async_call(rpc_name, args...);
auto status = future.wait_for(std::chrono::seconds(timeout));
if (status == std::future_status::timeout || status == std::future_status::deferred) {
    throw std::runtime_error("sync call timeout or deferred\n");
}
ReqResult result = future.get();
if (!result.check_valid()) {
    throw std::runtime_error("call error, exception: " + result.get_result<std::string>() + "\n");    
}
return result.get_result<T>();
```


## 分析

1. 怎么做到？
* 网络传输时不需要复杂协议，只需要有一个自定义head记录长度、id、类型等即可，数据存在后面的body中
* ```FunctionTraits```可以萃取出任意函数的返回值、参数、函数名
* 服务端利用```std::bind```或```lambda```，将函数转为统一格式（类成员和全局或静态）并存储在哈希表里
* 利用```msgpack```，在客户端时将参数转为字符串，在服务端将字符串转为```std::tuple```
* 利用模板元和不定参数展开技巧，将```std::tuple```展开为参数
* 调用，并得到结果（或异常）后返回
* 以add为例
  * 服务端在哈希表中添加add函数信息，```std::bind```后只需要提供参数字符串和一个字符串引用```result```存储结果
  * 客户端```call("add", 1, 2)```
  * ```msgpack```打包为字符串s
  * 服务端解析```s```，根据第一个参数```add```从哈希表中得到调用函数```func```
  * 解析```s```得到```std::tuple<std::string, int, int> t```
  * 利用```tuple_to_args_utils.hpp```，根据t得到一个结构体```TupleIndex<1, 2> i```
  * 暂不考虑```result```等等，```func(std::get<i::type>(t)...)```即可
  * 调用时利用萃取编写函数返回值，```SFINAE```（匹配失败不算编译错误，详情请查询```std::enable_if```）加上判断```std::is_void```就可以对返回值是否为```void```进行不同的逻辑编写
  * 如果有异常，返回```std::tuple<int, string>(1, std::exception::what)()) ```；否则返回```std::tuple<int, int>(0, 3)```
  * 客户端进行异常判断，如果无异常，根据预期返回值将```std::tuple```第二维转为目标类型```T```，此处```T```为```int```

2. 异步分析
* 这里仅先分析正确性
* ```boost::asio```提供```boost::asio::io_context```，作为```socket```读写工具，异步```callback```时会从run他的线程中挑选一个就绪的来执行```callback```。
* 前置：```socket```线程不安全（至少在```boost::asio```库下
* 不能有多个线程对同一个```socket```同时读/写
* 考虑一个场景，实现一个线程安全消费队列，读取后需要将内容写入到一个fd里，该fd线程不安全且无法加锁。
  * 写时加锁
  * 写入后进行读取，并判断队列元素是否大于1，如果=1则进行读入，否则解锁
  * 读入时不pop，读取完毕后解锁并写入fd，然后加锁pop元素，继续判断队列元素个数是否大于0，如果是则继续读取
  * 相当于把整个读取和写fd过程原子化，整个读取过程中队列元素个数并未减少，其他线程写入后队列元素大于1，无法写入
  * 同读同写也没问题，队列已线程安全
* 客户端/服务端
  * 读
  * head callback读body，body callback读head
  * 读head时读body一定结束了，读body时读head一定结束了
  * 写
  * 其实就是上面说的场景，socket即为该fd，使用该方法即可

3. 同步分析
* 服务端主线程监听epoll事件，listen_fd为非阻塞，监听选项为非ONESHOT
* 监听到新客户端请求时，提交给线程池：创建一个```SyncConnection```处理资源，并添加```EPOLLIN | EPOLLONESHOT```事件监听
* 监听到读请求时，提交给线程池：利用状态机，先读head再读body，最多读一个请求后返回（重置epoll监听的事件），检测是否读完，读完则提交给线程池新任务：```router```完成过程调用逻辑。调用后write，如果未写完则添加```EPOLLOUT | EPOLLONESHOT```事件监听
* 监听到写请求时，提交给线程池：调用```write```，继续完成未写入的部分，写入完成后添加```EPOLLIN | EPOLLONESHOT```事件监听
* 客户端新线程阻塞读socket，阻塞写socket
* 同时至多只有一个线程读，至多只有一个线程写，并且均不是主线程
* 这里可以继续改进：如何对于同一个用户的多个请求并发，允许响应乱序返回？（实现比较困难，而且只在客户数 < 线程池线程数有用）

## 改进

1. 简化萃取和模板元部分
*  为了将```std::tuple```展开为不定参数，需要模板元来生成序列```<0, 1, 2...>```，从而配合get取值
*  由于传来的参数附带```rpc_name```，所以展开到1即可，而非0
*  萃取的代码就不需要写```rest_rpc```中的2nd了，模板元的代码也不需要写那么多（比如实现循环等等）
*  返回值从```typename std::enable_if<std::is_void<typename std::result_of<F(std::weak_ptr<connection>, Args...)>::type>::value>::type```修改为```typename FunctionTraits<Func>::return_type```，原因在于我们的Arg包含了```rpc_name```，只是最后展开的时候从1开始
*  整体代码量减少，更好理解

2. 提升性能
* ```rest_rpc```期望在调用过程时实现同步和异步两种方式，但代码只体现了一种
* 对每一个```boost::asio::io_context```，只有一个线程run
* 根据分析中的异步分析，不同线程写回结果是没有错误的
* 根据分析中的异步分析，多线程读是没有错误的，因为同一个socket head和body同时只有一个在监听，也不会有多个线程同时监听head
* 调用过程可以有多个线程完成（下一个方法也可以）
* 对每一个```boost::asio::io_context```，可以多run几个线程，防止某一个运行线程卡死在一个客户上太久（多个客户端请求可能对应一个```io_context```），因为运行逻辑的线程是run ```io_context```的回调线程
* 添加```common.hpp: THREAD_NUMBER_PER_CONTEXT```，测试通过

3. 添加同步服务器、日志功能

* 实现线程池```ThreadPool```，可提交任意非类成员函数
* 实现```Timer```，管理所有```socket_fd```的添加、删除（经过若干时间还未flush）、flush
* 利用```epoll``` io多路复用实现```reactor```模式


## 性能

环境：ubuntu20.04(wsl), 4核4G内存

测试时间仅测试请求和响应时间，不计算连接和回收资源时间

```add```：计算两数之和，返回结果

```slp```：计算两数之和，线程阻塞50ms，返回结果

```cat```：类成员函数，拼接类成员字符串和到来的字符串，返回结果

测试case：
* C1：50客户端，每个500次add + 500次cat
* C2：50客户端，每个500次slp + 500次cat

服务器
* S1：async 4个io_context，每个对应1个就绪线程
* S2：async 4个io_context，每个对应5个就绪线程
* S3：sync 线程池大小20，LT
* S4：sync 线程池大小20，ET 

| | S1 | S2 | S3 | S4 |
| ---- |  ----  | ----  | ----| ----|
| C1 | 0.681534s | 0.657563s | 0.596436s | 1.17298s |
| C2 | 326.822s | 65.4517s | 63.182s | 63.191s |

可以看出异步情况的改进确实有效果

## 后记

如果您发现未测出的错误，或者有更好的改进，欢迎联系mikaovo2000@gmail.com或提issue
