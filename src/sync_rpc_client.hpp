#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <iostream>
#include <strings.h>
#include <future>
#include <queue>
#include <memory>
#include <vector>

#include "common.hpp"
#include "msgpack_utils.hpp"
#include "epoll_utils.hpp"
#include "utils.hpp"

class ReqResult {
public:
    ReqResult(std::string &data) : data_(std::move(data)) {
        // std::cout<<"[result]" << data_.length() << std::endl;
        // debug_pack<std::tuple<int, int> >(data_, 2);
    }
    bool check_valid() {
        // std::cout<<"[result]" << data_.length() << std::endl;
        auto tp = unpack<std::tuple<int> >(data_.data(), data_.length());
        if (std::get<0>(tp) == Result_FAIL) {
            return false;
        }
        return true;
    }
    template <typename T> 
    T get_result() {
        // std::cout<<"[result]" << data_.length() << std::endl;
        auto tp = unpack<std::tuple<int, T> >(data_.data(), data_.length());
        return std::get<1>(tp);
    }
private:
    std::string data_;
};

class SyncRpcClient {
public:
    SyncRpcClient(const std::string &host, unsigned short port, int client_id) : 
                host_(std::move(host)), port_(port), has_connect_(false), req_id_(0), client_id_(client_id) {
        // body_ = new char[BUFFER_SIZE];
        // res_ = new char[BUFFER_SIZE];
        body_ = nullptr;
        res_ = nullptr;
    }
    ~SyncRpcClient() {
        stop();
    }
    void run() {
        if (thread_ptr_ != nullptr && thread_ptr_->joinable()) {
            thread_ptr_->join();
        }
    }
    void stop() {
        if (!has_connect_) {
            return;
        }
        has_connect_ = false;
        shutdown(socket_fd_, SHUT_RDWR);
        if (thread_ptr_->joinable()) {
            thread_ptr_->join();  
        }
        if (body_ != nullptr) {
            delete []body_;
        }
        if (res_ != nullptr) {
            delete []res_;
        }
    }
    void try_connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        set_blocking(socket_fd_);
        struct sockaddr_in servaddr;
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port_);
        inet_pton(AF_INET, host_.c_str(), &servaddr.sin_addr);
        int ret = connect(socket_fd_, (struct sockaddr*) &servaddr, sizeof(servaddr));
        assert(ret >= 0);

        has_connect_ = true;

        thread_ptr_ = std::make_shared<std::thread>(
            [this]() {
                read();
            }
        );
    }
    template<size_t timeout, typename T = void, typename... Args>
    typename std::enable_if<std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        auto future = async_call(rpc_name, std::forward<Args>(args)...);
        auto status = future.wait_for(std::chrono::seconds(timeout));
        if (status == std::future_status::timeout || status == std::future_status::deferred) {
            throw std::runtime_error("sync call timeout or deferred\n");
        }
        ReqResult result = future.get();
        if (!result.check_valid()) {
            throw std::runtime_error("call error, exception: " + result.get_result<std::string>() + "\n");    
        }
    }
    
    template<typename T = void, typename... Args>
    typename std::enable_if<std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        call<CLIENT_DEFAULT_TIMEOUT, T>(rpc_name, std::forward<Args>(args)...);
    }

    template<size_t timeout, typename T, typename... Args>
    typename std::enable_if<!std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        auto future = async_call(rpc_name, std::forward<Args>(args)...);
        auto status = future.wait_for(std::chrono::seconds(timeout));
        if (status == std::future_status::timeout || status == std::future_status::deferred) {
            throw std::runtime_error("sync call timeout or deferred\n");
        }
        ReqResult result = future.get();
        // std::cout << &result << std::endl;
        if (!result.check_valid()) {
            throw std::runtime_error("call error, exception: " + result.get_result<std::string>() + "\n");    
        }
        return result.get_result<T>();
    }
    
    template<typename T, typename... Args>
    typename std::enable_if<!std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        return call<CLIENT_DEFAULT_TIMEOUT, T>(rpc_name, std::forward<Args>(args)...);
    }    

    template <typename... Args>
    std::future<ReqResult> async_call(const std::string &rpc_name,
                                        Args &&...args) {
        auto p = std::make_shared<std::promise<ReqResult>>();
        std::future<ReqResult> future = p->get_future();
        uint64_t req_id = 0;
        {
            std::unique_lock<std::mutex> lock(req_mutex_);
            ++req_id_;
            req_id = req_id_;
            future_map_.emplace(req_id, std::move(p));
        }
        // std::cout << "async_call" << req_id << std::endl;
        std::string ret = pack_args_str(rpc_name, std::forward<Args>(args)...);
        // std::cout << "call " << client_id_ << " " << req_id << std::endl;
        write(req_id, std::move(ret));
        return std::move(future);
    }

    void call_back(uint64_t req_id, std::string &data) {
        {
            std::unique_lock<std::mutex> lock(req_mutex_);
            // std::cout << "call_back " << client_id_ << " " << req_id << " " << data.size() << std::endl;
            if (future_map_.find(req_id) == future_map_.end()) {
                // std::cout << "call_back_error " << client_id_ << " " << req_id << " " << data.size() << std::endl;
                throw std::runtime_error("Request not found.");
            }
            auto &f = future_map_[req_id];
            f->set_value(ReqResult(data));
            future_map_.erase(req_id);
        }
    }

    void write(uint64_t req_id, std::string &&str) {
        size_t size = str.size();
        if (size >= BUFFER_SIZE) {
            throw std::runtime_error("params size too large!");
        }
        Message msg{req_id, std::make_shared<std::string>(std::move(str))};

        // debug_pack<std::tuple<std::string, int, int> >(*msg.content, 3);

        RpcHeader header{msg.content->size(), msg.req_id};
        res_len_ = msg.content->size() + HEADER_LENGTH;
        res_ = resize(res_, res_len_);

        memcpy(res_, &header, HEADER_LENGTH);
        memcpy(res_ + HEADER_LENGTH, msg.content->data(), msg.content->size());

        send(socket_fd_, res_, res_len_, 0);
    }

    void read() {
        int tmp_req_id;
        int has_size, need_size;
        int read_size;
        while (has_connect_) {
            need_size = HEADER_LENGTH;
            has_size = 0;
            while (has_size < need_size) {
                read_size = recv(socket_fd_, head_ + has_size, HEADER_LENGTH - has_size, 0);
                if (read_size <= 0) {
                    break;
                } else {
                    has_size += read_size;
                }
            }
            // debug_char(head_, has_size);
            if (!has_connect_) {
                break;
            }
            RpcHeader *header = (RpcHeader *)(head_);
            tmp_req_id = header->req_id;
            body_len_ = header->body_len;
            body_ = resize(body_, body_len_);
            if (body_len_ > 0 && body_len_ < BUFFER_SIZE) {
                has_size = 0;
                need_size = body_len_;
                while (has_size < need_size) {
                    read_size = recv(socket_fd_, body_ + has_size, body_len_ - has_size, 0);
                    if (read_size <= 0) {
                        break;
                    } else {
                        has_size += read_size;
                    }
                }
                // debug_char(body_, has_size);
                std::string res = std::string(body_, body_len_);
                call_back(tmp_req_id, res);
            } else {
                stop();
            }
        }
    }
private:
    int client_id_;

    std::string host_;
    unsigned short port_;
    std::atomic<bool> has_connect_;

    int socket_fd_;

    std::shared_ptr<std::thread> thread_ptr_;
    std::mutex req_mutex_;
    uint64_t req_id_;
    std::unordered_map<std::uint64_t, std::shared_ptr<std::promise<ReqResult>>> future_map_;

    uint32_t res_len_;
    uint32_t body_len_;
    char head_[HEADER_LENGTH];
    
    char *body_;
    char *res_;
};