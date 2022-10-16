#include "./src/rpc_client.hpp"

int main() {
    RpcClient rc("127.0.0.1", 9001);
    rc.async_connect();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    rc.write_queue_.push_back({rc.req_id_, std::make_shared<std::string>("aabbcc")});
    rc.write();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    rc.run();
    rc.stop();
    return 0;
}