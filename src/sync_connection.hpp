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
    ~SyncConnection() = default;
    void init(int epoll_fd, int sock_fd, triger trig_mode) {
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
    // must get mutex
    bool is_write_end() {
        return write_queue_.size() == 0 && write_index_ == res_len_;
    }
    void response(uint64_t req_id, std::string data) { 
        std::unique_lock<std::mutex> lock(write_mtx_);
        bool flag = is_write_end();
        write_queue_.push_back({req_id_, std::make_shared<std::string>(std::move(data))});
        if (flag) {
            write();
        }
    }
    bool read() {
        switch (status_) {
            case status::read_head : {
                if (! read_head()) {
                    return false;
                }
                if (read_index_ == HEADER_LENGTH) {
                    RpcHeader *header = (RpcHeader *)(head_);
                    req_id_ = header->req_id;
                    body_len_ = header->body_len;
                    if (body_len_ > 0 && body_len_ < BUFFER_SIZE) {
                        if (body_.size() < body_len_) {
                            body_.resize(body_len_);
                        }
                    } else {
                        return false;
                    }
                    read_index_ = 0;
                    status_ = status::read_body;
                }
                // break;
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
            bytes_read = recv(sock_fd_, body_.data() + read_index_, body_len_ - read_index_, 0);
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
                bytes_read = recv(sock_fd_, body_.data() + read_index_, body_len_ - read_index_, 0);
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
        // std::unique_lock<std::mutex> lock(write_mtx_);

        if (is_write_end()) {
            mod_fd(epoll_fd_, sock_fd_, EPOLLIN, true, trig_mode_);
            return true;
        }

        if (write_index_ == res_len_) {
            write_index_ = 0;
            Message &msg = write_queue_.front();
            write_queue_.pop_front();
            res_len_ = msg.content->size();
            res_.resize(res_len_ + HEADER_LENGTH);
            
            memcpy(res_.data(), &msg, HEADER_LENGTH);
            memcpy(res_.data() + HEADER_LENGTH, msg.content->c_str(), res_len_);
        }

        int bytes_send;
        bool flag = false;
        if (trig_mode_ == triger::LT_triger) {
            bytes_send = send(sock_fd_, res_.data() + write_index_, res_len_ - write_index_, 0);
            write_index_ += bytes_send;
            if (bytes_send <= 0) {
                flag = false;
            }
            flag = true;
        } else {
            while (1) {
                bytes_send = send(sock_fd_, res_.data() + write_index_, res_len_ - write_index_, 0);
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
            if (is_write_end()) {
                mod_fd(epoll_fd_, sock_fd_, EPOLLIN, true, trig_mode_);
            } else {
                mod_fd(epoll_fd_, sock_fd_, EPOLLIN | EPOLLOUT, true, trig_mode_);    
            }
        } else {
            Timer::delete_sock(sock_fd_);
        }

        return flag;
    }
    template <typename Func>
    void register_handler(std::string const &name, const Func &f) {
        router_.register_handler(name, f);
    }
    template <typename Func, typename Self>
    void register_handler(std::string const &name, const Func &f, Self *self) {
        router_.register_handler(name, f, self);
    }
private:
    int epoll_fd_;
    int sock_fd_;
    triger trig_mode_;
    
    std::atomic<bool> has_stop_;
    uint32_t res_len_;
    uint32_t body_len_;
    uint64_t req_id_;
    int64_t conn_id_;

    uint32_t write_index_;
    uint32_t read_index_;

public:
    std::mutex write_mtx_;
    std::deque<Message> write_queue_;

    status status_; 
    size_t bytes_to_read_;

    char head_[HEADER_LENGTH];
    std::vector<char> body_;
    std::vector<char> res_;
};


