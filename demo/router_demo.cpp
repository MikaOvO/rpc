#include <bits/stdc++.h>
#include "../src/router.hpp"
#include "../src/msgpack_utils.hpp"
#include "../src/tuple_to_args_utils.hpp"
#include "../src/function_traits.hpp"

using namespace std;

template<typename T>
void func(T t) {

}

template<typename T>
void try_bind(T t) {
    auto f = bind(func<T>, 1);
}

int add(int x, int y) {
    cout << "add " << x << " " << y << "\n";
    return x + y;
}

void add_void(int x, int y) {
    cout << "add_void " << x << " " << y << "\n";
    // return x + y;
}

class A {
public:
    int x;
    int y;
    A() {
        x = 10;
        y = 30;
    }
    int add(int z, int q) {
        cout << "cls add " << x << " " << y << " " << z << "\n";
        return x + y + z;
    }
};


int main() {
    Router router;
    //try_bind(1);
    router.register_handler("cdd", add);
    A a;
    router.register_handler("add", &A::add, &a);
    auto buffer = pack_args("add", 1, 2);
    auto chars = buffer.release();
    router.router(chars, sizeof(chars));

    buffer = pack_args("cdd", 1, 2);
    // string msg = string(buffer.release());
    // cout << "msg: " << msg << endl;
    //string result;
    chars = buffer.release();
    router.router(chars, sizeof(chars));
    // router.apply_member(&A::add, &a, chars, sizeof(chars), result);
    // auto r = unpack<tuple<int, int>>(result.c_str(), result.size());
    // cout << get<0>(r) << endl;
    // cout << get<1>(r) << endl;
    return 0;
}