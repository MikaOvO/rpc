#pragma once

#include <cstdint>
#include <string>

const static size_t HEADER_LENGTH = 12;
const static size_t INIT_BUFFER_SIZE = 128;
const static size_t BUFFER_SIZE = 10 * 1024 * 1024;
const static size_t CLIENT_DEFAULT_TIMEOUT = 5;
const static size_t MSGPACK_INIT_BUFFER_SIZE = 128; 
const static size_t CHECK_TIME = 5;

#pragma pack(4)
struct RpcHeader {
    uint32_t body_len;
    uint64_t req_id;
};
#pragma pack()

struct Message {
    uint64_t req_id;
    // std::string content;
    std::shared_ptr<std::string> content;
};