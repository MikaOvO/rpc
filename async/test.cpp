#include "../src/rpc_client.hpp"

#include <bits/stdc++.h>

using namespace std;

void check() {
    RpcClient rc("127.0.0.1", 9009);
    
    rc.connect();

    for (int i = 1; i <= rand() % 200 + 1; ++i) {
        int x = rand() % 10 + 1;
        int y = rand() % 10 + 1;
        int result = rc.call<int>("add", x, y);
        assert(x + y == result);
        if (i % 10 == 0) std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    for (int i = 1; i <= rand() % 200 + 1; ++i) {
        int x = rand() % 20 + 1;
        string tmp = "";
        for (int j = 0; j < x; ++j)  {
            int y = rand() % 26;
            tmp += (char)(y + 'a');
        }
        string result = rc.call<string>("cat", tmp);
        assert("begin_" + tmp == result);
        if (i % 10 == 0) std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    rc.run();
}


int main() {
    srand(4564);
    vector<thread> vec;
    for (int i = 0; i < 10; ++i) {
        vec.emplace_back(thread(check));
    }
    for (auto &t : vec) {
        t.join();
    }

    cout << "pass." << endl;

    // cout << "main thread: " << this_thread::get_id() << endl;
    // RpcClient rc("127.0.0.1", 9007);
    
    // rc.connect();
    
    // auto result = rc.call<string>("cat", "aaa");
    
    // std::cout << result << std::endl;
    
    // rc.run();

    return 0;
}