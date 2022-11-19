#include "sync_rpc_server.hpp"

#include <iostream>

using namespace std;

class Foo {
public:
    Foo(string &begin) : begin_(std::move(begin)) {

    }
    Foo(string &&begin) : begin_(std::move(begin)) {

    }
    string cat(string x) {
        cout << "cat " << x << endl;
        return begin_ + x;
    }
    string begin_; 
};

int add(int x, int y) {
    cout << "add " << x << " " << y << endl;
    return x + y;
}

int main() {
    SyncRpcServer rs(9009, 8, triger::LT_triger);
    Foo foo("begin_");
    rs.register_handler("add", &add);
    rs.register_handler("cat", &Foo::cat, &foo);
    rs.run();
    std::this_thread::sleep_for(std::chrono::seconds(1000));
    rs.stop();
    return 0;
}