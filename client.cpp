#include "./src/rpc_client.hpp"
#include "./src/msgpack_utils.hpp"
#include <iostream>

using namespace std;

int main() {
    cout << "main thread: " << this_thread::get_id() << endl;
    RpcClient rc("127.0.0.1", 9007);
    rc.async_connect();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    auto result = rc.call<int>("add", 1, 3);
    std::cout << result << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    rc.run();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    rc.stop();
    return 0;
}