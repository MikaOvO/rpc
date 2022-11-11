# rpc

这是一个远程过程调用的c++ header-only项目，参考rest_rpc编写（感谢purecpp社区，后面会讲解进行的改进），支持客户端调用服务器端的函数（包括类成员函数）。

服务器支持同步（TODO）异步两种方式，同步为从零实现，异步使用boost::asio网络库。

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


## 分析

1. 怎么做到？
* 暂不考虑网络传输
* FunctionTraits可以萃取出任意函数的返回值、参数、函数名
* 服务端利用bind，将函数转为统一格式（类成员和全局或静态）并存储在哈希表里
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
  * 暂不考虑result等等，func(std::get<i>(t)...)即可
  * 调用时利用萃取编写函数返回值，SFINAE（匹配失败不算编译错误）加上判断is_void就可以对返回值是否为void进行不同的逻辑编写
  * 如果有异常，返回std::tuple<int, string>(1, std::exception::what)()) ；否则返回std::tuple<int, string>(0, "3")
  * 客户端进行异常判断，如果无异常，根据预期返回值将tuple第二维转为目标类型T，此处T为int