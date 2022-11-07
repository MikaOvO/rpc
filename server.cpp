#include "./src/rpc_server.hpp"

#include <iostream>

using namespace std;

int add(int x, int y) {
    cout << "add " << x << " " << y << endl;
    return x + y;
}

int main() {
    RpcServer rs(9007, 3);
    rs.register_handler("add", add);
    rs.run();
    return 0;
}