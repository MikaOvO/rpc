#pragma once

#include <msgpack.hpp>

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