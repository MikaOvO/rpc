# rpc

这是一个c++ header-only的远程过程调用项目，参考rest_rpc编写（感谢purecpp社区，后面会讲解进行的改进），支持客户端调用服务器端的函数（包括类成员函数）。

网络编程支持同步（TODO）异步两种方式，同步为从零实现，异步使用boost::asio网络库。

客户端支持同步和异步调用两种方式，同步会阻塞直到获取结果，异步将会获得std::future\<ReqResult>，其声明和定义位于rpc_client.hpp，后续可自行管理。

传输协议使用msgpack（在本项目所需功能中，相比json更易用也更快）。

## 使用

确保本地环境有g++、cmake、boost::asio库，编译命令为c++17。

common.hpp下log_dir可以调整日志输出路径，LOG_LEVEL调整输出日志等级，为0时不进行任何日志输出（用于性能测试）。

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
客户端可以参考client.cpp或测试程序编写

需要异步时，可参考以下代码，若调用函数返回值为void，请删除最后一行
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
* FunctionTraits可以萃取出任意函数的返回值、参数、函数名
* 服务端利用std::bind，将函数转为统一格式（类成员和全局或静态）并存储在哈希表里
* 利用msgpack，在客户端时将参数转为字符串，在服务端将字符串转为tuple
* 利用模板元和不定参数展开技巧，将tuple展开为参数
* 调用，并得到结果（或异常）后返回
* 以add为例
  * 服务端在哈希表中添加add函数信息，bind后只需要提供参数字符串和一个字符串引用result存储结果
  * 客户端call("add", 1, 2)
  * msgpack打包为字符串s
  * 服务端解析s，根据第一个参数add从哈希表中得到调用函数func
  * 解析s得到std::tuple<std::string, int, int> t
  * 利用tuple_to_args_utils.hpp，根据t得到一个结构体TupleIndex<1, 2> i
  * 暂不考虑result等等，func(std::get\<i::type>(t)...)即可
  * 调用时利用萃取编写函数返回值，SFINAE（匹配失败不算编译错误，详情请查询std::enable_if）加上判断std::is_void就可以对返回值是否为void进行不同的逻辑编写
  * 如果有异常，返回std::tuple<int, string>(1, std::exception::what)()) ；否则返回std::tuple<int, int>(0, 3)
  * 客户端进行异常判断，如果无异常，根据预期返回值将tuple第二维转为目标类型T，此处T为int

2. 异步分析
* 这里仅先分析正确性
* 前置：socket线程不安全（至少在boost::asio库下
* boost::asio提供boost::asio::io_context，作为socket读写工具，异步callback时会从run他的线程中挑选一个就绪的来执行callback。
* 不能有多个线程对同一个socket同时读/写
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

## 改进

1. 简化萃取和模板元部分
*  为了将tuple展开为不定参数，需要模板元来生成序列<0, 1, 2...>，从而配合get取值
*  由于传来的参数附带rpc_name，所以展开到1即可，而非0
*  萃取的代码就不需要写rest_rpc中的2nd了，模板元的代码也不需要写那么多（比如实现循环等等）
*  返回值从```typename std::enable_if<std::is_void<typename std::result_of<F(std::weak_ptr<connection>, Args...)>::type>::value>::type```修改为```typename FunctionTraits<Func>::return_type```，原因在于我们的Arg包含了rpc_name，只是最后展开的时候从1开始
*  整体代码量减少，更好理解

2. 提升性能
* rest_rpc期望在调用过程时实现同步和异步两种方式，但代码只体现了一种
* 对每一个boost::asio::io_context，只有一个线程run
* 根据分析中的异步分析，不同线程写回结果是没有错误的
* 根据分析中的异步分析，多线程读是没有错误的，因为同一个socket head和body同时只有一个在监听，也不会有多个线程同时监听head
* 调用过程可以有多个线程完成（下一个方法也可以）
* 对每一个boost::asio::io_context，可以多run几个线程，防止某一个运行线程卡死在一个客户上太久（多个客户端请求可能对应一个io_context），因为运行逻辑的线程是run io_context的回调线程
* 添加common.hpp: THREAD_NUMBER_PER_CONTEXT，测试通过

3. 添加同步服务器、日志功能

TODO

## 性能

TODO

## 后记

改进中可能存在未测出的错误，或者您有更好的改进，欢迎联系mikaovo2000@gmail.com或提issue
