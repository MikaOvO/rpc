#include <msgpack.hpp>
#include <string>
#include <iostream>
#include <sstream>

#include "../src/msgpack_utils.hpp"
#include "../src/tuple_to_args_utils.hpp"
#include "../src/function_traits.hpp"

using namespace std;

void func(int x, bool y, char z) {
    cout << x << " " << y << " " << z << endl;
}

int main()
{
    // msgpack::type::tuple<int, bool, std::string> src(1, true, "example");
    auto buffer = pack_args(1, true, 'c');
    string msg = string(buffer.release());
    cout << "msg: " << msg << endl;
    // FunctionTraits<decltype(func)>::arg_type args;
    // auto args = unpack<FunctionTraits<decltype(func)>::arg_type>(msg.c_str(), msg.size());
    // cout << std::get<0>(args) << endl;
    // cout << std::get<1>(args) << endl;
    // cout << std::get<2>(args) << endl;
    
    // call(func, args);
    
    // auto p = unpack<std::tuple<int> >(msg.c_str(), msg.size());
    // cout << std::get<0>(p) << endl;
    // cout << std::get<1>(p) << endl;
    // cout << std::get<2>(p) << endl;
    return 0;
}