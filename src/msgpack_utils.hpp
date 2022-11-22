#pragma once

#include <msgpack.hpp>
#include <iostream>

#include "log.hpp"
#include "common.hpp"

template <typename... Args> 
msgpack::sbuffer pack_args(Args &&...args) {
    msgpack::sbuffer buffer(MSGPACK_INIT_BUFFER_SIZE);
    msgpack::pack(buffer, std::forward_as_tuple(std::forward<Args>(args)...));
    return buffer;
}

template <typename Arg, typename... Args>
std::string pack_args_str(Arg arg, Args &&...args) {
    msgpack::sbuffer buffer(MSGPACK_INIT_BUFFER_SIZE);
    msgpack::pack(buffer, std::forward_as_tuple(arg, std::forward<Args>(args)...));
    return std::string(buffer.data(), buffer.size());
}

template <typename T>
T unpack(const char *data, size_t length) {
    try {
        msgpack::unpacked msg;
        msgpack::unpack(msg, data, length);
        return msg.get().as<T>();
    } catch (...) {
        throw std::invalid_argument("unpack failed: Args not match!");
    }
}

template <typename T>
void debug_pack_1(const char *data, size_t length) {
    auto tp = unpack<T>(data, length);
    std::cout << std::get<0>(tp) << std::endl;
}   

template <typename T>
void debug_pack_2(const char *data, size_t length) {
    auto tp = unpack<T>(data, length);
    std::cout << std::get<0>(tp) << " " << std::get<1>(tp) << std::endl;
}
template <typename T>
void debug_pack_3(const char *data, size_t length) {
    auto tp = unpack<T>(data, length);
    std::cout << std::get<0>(tp) << " " << std::get<1>(tp) << " " << std::get<2>(tp) << std::endl;
}

template <typename T>
void debug_pack(std::string s, int num) {
    if (num == 3) {
        debug_pack_3<T>(s.c_str(), s.size());
    } else if (num == 2) {
        debug_pack_2<T>(s.c_str(), s.size());
    } else {
        debug_pack_1<T>(s.c_str(), s.size());    
    }
}