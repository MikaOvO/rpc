#pragma once

#include <cstdint>
#include <string>
#include "stdlib.h"

const static char* log_dir = "/home/mika/workspace/cpp_workspace/rpc/log";
const static int LOG_LEVEL = 0;

// sync params
const static long CONNECT_TIME_SLOT = 3600;
const static long ALARM_TIME_SLOT = 5;
const static int MAX_FD = 1024;
const static int MAX_EVENTS = 10000;
enum class triger {
    ET_triger = 0,
    LT_triger = 1
};

enum class status {
    read_head = 0,
    read_body = 1,
    write = 2
};

// async params
const static size_t HEADER_LENGTH = 12;
const static size_t INIT_BUFFER_SIZE = 128;
const static size_t BUFFER_SIZE = 10 * 1024 * 1024;
const static size_t CLIENT_DEFAULT_TIMEOUT = 50;
const static size_t MSGPACK_INIT_BUFFER_SIZE = 128; 
const static size_t CHECK_TIME = 50;
const static size_t THREAD_NUMBER_PER_CONTEXT = 3;
const static int WAIT_CONNECT_TIME = 5;

const static int Result_OK = 0;
const static int Result_FAIL = 1;

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