#include "../src/sync_rpc_client.hpp"
#include <bits/stdc++.h>

using namespace std;

void check() {
    SyncRpcClient rc("127.0.0.1", 9009);
    
    rc.try_connect();

    int result = rc.call<int>("add", 1, 2);
    
    cout << result << endl;

    rc.stop();
}


int main() {
    vector<thread> vec;
    for (int i = 0; i < 1; ++i) {
        vec.emplace_back(thread(check));
    }
    for (auto &t : vec) {
        t.join();
    }
    // cout << "main thread: " << this_thread::get_id() << endl;
    // RpcClient rc("127.0.0.1", 9007);
    
    // rc.connect();
    
    // auto result = rc.call<string>("cat", "aaa");
    
    // std::cout << result << std::endl;
    
    // rc.run();

    return 0;
}