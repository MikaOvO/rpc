#include "rpc_server.hpp"

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
    RpcServer rs(9008, 4);
    Foo foo("begin_");
    rs.register_handler("add", &add);
    rs.register_handler("cat", &Foo::cat, &foo);
    rs.run();
    return 0;
}