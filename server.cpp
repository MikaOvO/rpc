#include "./src/rpc_server.hpp"

int main() {
    RpcServer rs(9001, 3);
    rs.run();
    return 0;
}