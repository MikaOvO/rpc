#include "../src/rpc_client.hpp"

#include <bits/stdc++.h>

using namespace std;

void check(int i, time_count &tc) {
    RpcClient rc("127.0.0.1", 9100);
    
    rc.connect();

    tc.add(i);

    for (int i = 1; i <= 500; ++i) {
        int x = rand() % 10 + 1;
        int y = rand() % 10 + 1;
        int result = rc.call<int>("slp", x, y);
        assert(x + y == result);
    }

    for (int i = 1; i <= 500; ++i) {
        int x = rand() % 20 + 1;
        string tmp = "";
        for (int j = 0; j < x; ++j)  {
            int y = rand() % 26;
            tmp += (char)(y + 'a');
        }
        string result = rc.call<string>("cat", tmp);
        assert("begin_" + tmp == result);
    }

    tc.call(i);

    rc.run();
}


int main() {
    time_count tc;

    srand(42);

    vector<thread> vec;
    for (int i = 0; i < 50; ++i) {
        vec.emplace_back(thread(check, i, std::ref(tc)));
    }
    for (auto &t : vec) {
        t.join();
    }

    return 0;
}