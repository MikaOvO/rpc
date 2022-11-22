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
#include "router.hpp"
#include "timer.hpp"

class SyncConnection : public std::enable_shared_from_this<SyncConnection> {
public:
    SyncConnection() = default;
    ~SyncConnection() {
        if (body_ != nullptr) {
            delete []body_;
        }
        if (res_ != nullptr) {
            delete []res_;
        }
    }
    void init(int epoll_fd, int sock_fd, triger trig_mode) {
        status_ = status::read_head;
        req_id_ = 0;
        epoll_fd_ = epoll_fd;
        sock_fd_ = sock_fd;
        trig_mode_ = trig_mode;
        read_index_ = 0;
        write_index_ = 0;
        res_len_ = 0;
        body_len_ = 0;
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
    void reset_read() {
        status_ = status::read_head;
        read_index_ = 0;
    }
    bool is_read_end() {
        return status_ == status::read_body && read_index_ == body_len_;
    }
    
    void response(uint64_t req_id, std::string data) { 
        // Log::write_log_default(0, "[connection] response req_id: %d, data: %s\n", flag, data.c_str());
        // debug_char(data.data(), data.size());
        write_index_ = 0;
        RpcHeader header{data.size(), req_id};
        
        res_len_ = data.size() + HEADER_LENGTH;
        res_ = resize(res_, res_len_);

        memcpy(res_, &header, HEADER_LENGTH);
        memcpy(res_ + HEADER_LENGTH, data.data(), data.size());

        write();
    }
    bool read() {
        Log::write_log_default(0, "[connection] %d is read status %d read_index %d\n", sock_fd_, status_, read_index_);
        switch (status_) {
            case status::read_head : {
                if (! read_head()) {
                    return false;
                }
                if (read_index_ == HEADER_LENGTH) {
                    RpcHeader *header = (RpcHeader *)(head_);
                    req_id_ = header->req_id;
                    body_len_ = header->body_len;
                    Log::write_log_default(0, "[connection] %d read req_id %d\n", sock_fd_, req_id_);
                    if (body_len_ > 0 && body_len_ < BUFFER_SIZE) {
                        body_ = resize(body_, body_len_);
                    } else {
                        return false;
                    }
                    read_index_ = 0;
                    status_ = status::read_body;
                }
                else {
                    break;
                }
            } 
            case status::read_body : {
                if (! read_body()) {
                    return false;
                }
                break;
            }
        }
        return true;
    }
    bool read_head() {
        int bytes_read;
        if (read_index_ == HEADER_LENGTH) {
            return true;
        }
        if (trig_mode_ == triger::LT_triger) {
            bytes_read = recv(sock_fd_, head_ + read_index_, HEADER_LENGTH - read_index_, 0);
            read_index_ += bytes_read;
            if (bytes_read <= 0) {
                return false;
            }
            return true;
        } else {
           while (1) {
                if (read_index_ == HEADER_LENGTH) {
                    return true;
                }
                bytes_read = recv(sock_fd_, head_ + read_index_, HEADER_LENGTH - read_index_, 0);
                if (bytes_read == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                    break;
                }
                if (bytes_read <= 0) {
                    return false;
                }
                read_index_ += bytes_read;
           }
           return true;
        }
        return false;
    }
    bool read_body() {
        int bytes_read;
        if (read_index_ == body_len_) {
            return true;
        }
        if (trig_mode_ == triger::LT_triger) {
            bytes_read = recv(sock_fd_, body_ + read_index_, body_len_ - read_index_, 0);
            read_index_ += bytes_read;
            if (bytes_read <= 0) {
                return false;
            }
            return true;
        } else {
            while (1) {
                if (read_index_ == body_len_) {
                    return true;
                }
                bytes_read = recv(sock_fd_, body_ + read_index_, body_len_ - read_index_, 0);
                if (bytes_read == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                    break;
                }
                if (bytes_read <= 0) {
                    return false;
                }
                read_index_ += bytes_read;
           }
           return true;            
        }
        return false;        
    }
    bool write() {
        Log::write_log_default(0, "[connection] %d is write\n", sock_fd_);
        // std::unique_lock<std::mutex> lock(write_mtx_);d

        // Log::write_log_default(0, "[connection] write res_len: %d\n", res_len_);

        int bytes_send;
        bool flag = false;
        if (trig_mode_ == triger::LT_triger) {
            bytes_send = send(sock_fd_, res_ + write_index_, res_len_ - write_index_, 0);
            // std::cout << "[send]" << bytes_send << std::endl;
            // debug_char(res_ + write_index_, bytes_send);
            if (bytes_send <= 0) {
                flag = false;
            }
            write_index_ += bytes_send;
            flag = true;
        } else {
            while (1) {
                if (write_index_ == res_len_) {
                    break;
                }
                bytes_send = send(sock_fd_, res_ + write_index_, res_len_ - write_index_, 0);
                if (bytes_send == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                    flag = true;
                    break;
                }
                if (bytes_send <= 0) {
                    flag = false;
                }
                write_index_ += bytes_send;
           }
            flag = true;               
        }

        if (flag) {
            if (write_index_ == res_len_) {
                reset_read();
                mod_fd(epoll_fd_, sock_fd_, EPOLLIN, true, trig_mode_);
            } else {
                mod_fd(epoll_fd_, sock_fd_, EPOLLOUT, true, trig_mode_);    
            }
        } else {
            Timer::delete_sock(sock_fd_);
        }

        return flag;
    }
private:
    int epoll_fd_;
    triger trig_mode_;
    
    std::atomic<bool> has_stop_;
    uint64_t req_id_;
    int64_t conn_id_;

    uint32_t write_index_;
    uint32_t read_index_;

public:
    int sock_fd_;

    uint32_t res_len_;
    uint32_t body_len_;

    status status_; 
    size_t bytes_to_read_;

    char head_[HEADER_LENGTH];
    char* body_;
    char* res_;
};




