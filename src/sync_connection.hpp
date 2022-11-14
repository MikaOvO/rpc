#pragma once

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h> 
#include <sys/uio.h>
#include <mutex>
#include <cstring>
#include <thread>
#include <fstream>
#include <ctime>
#include <iostream>

#include "epoll_utils.hpp"
#include "timer.hpp"

class SyncConnection : public std::enable_shared_from_this<SyncConnection> {
public:
    SyncConnection() = default;
    ~SyncConnection() = default;
    void init(int epoll_fd, int sock_fd, triger trig_mode, std::shared_ptr<Router> router_ptr) {
        req_id_ = 0;
        epoll_fd_ = epoll_fd;
        sock_fd_ = sock_fd;
        trig_mode_ = trig_mode;
        router_ptr_ = router_ptr;
        read_index = 0;
        write_index = 0;
    } 
    uint64_t get_req_id() {
        return req_id_;
    }
    int64_t get_conn_id() {
        return conn_id_;
    }
    void set_conn_id(int64_t conn_id) {
        conn_id_ = conn_id;
    }
    void work() {
        switch (status_) {
            case status::read_head : {
                
            } 
            case status::read_body : {

            }
            case status::write : {

            }
        }
    }
    void read_head() {
        int bytes_read = 0;
        if (trig_mode_ == triger::ET_triger) {

        }
    }
    void read_body() {
        
    }
    void write() {

    }
private:
    int epoll_fd_;
    int sock_fd_;
    triger trig_mode_;
    
    std::atomic<bool> has_stop_;
    uint32_t write_size_;
    uint32_t body_len_;
    uint64_t req_id_;
    int64_t conn_id_;

    uint32_t write_index;
    uint32_t read_index;

    std::shared_ptr<Router> router_ptr_;
public:
    status status_; 
    size_t bytes_to_read_;

    char head_[HEADER_LENGTH];
    std::vector<char> body_;
};