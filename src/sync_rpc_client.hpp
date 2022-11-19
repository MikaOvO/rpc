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

class ReqResult {
public:
    ReqResult(std::string &data) : data_(std::move(data)) {
    }
    bool check_valid() {
        auto tp = unpack<std::tuple<int> >(data_.c_str(), data_.length());
        if (std::get<0>(tp) == Result_FAIL) {
            return false;
        }
        return true;
    }
    template <typename T> 
    T get_result() {
        auto tp = unpack<std::tuple<int, T> >(data_.c_str(), data_.length());
        return std::get<1>(tp);
    }
private:
    std::string data_;
};

class SyncRpcClient {
public:
    SyncRpcClient(const std::string &host, unsigned short port) : 
                host_(std::move(host)), port_(port), has_connect_(false), req_id_(0) {

    }
    ~SyncRpcClient() {

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
        shutdown(socket_fd_, SHUT_RDWR);
        if (thread_ptr_->joinable()) {
            thread_ptr_->join();  
        }
        has_connect_ = false;
    }
    void try_connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        // set_blocking(socket_fd_);
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
        auto ret = pack_args(rpc_name, std::forward<Args>(args)...);
        write(req_id, std::move(ret));
        return future;
    }

    void call_back(uint64_t req_id, std::string &data) {
        {
            std::unique_lock<std::mutex> lock(req_mutex_);
            if (future_map_.find(req_id) == future_map_.end()) {
                throw std::runtime_error("Request not found.");
            }
            auto &f = future_map_[req_id];
            f->set_value(ReqResult(data));
            future_map_.erase(req_id);
        }
    }

    void write(uint64_t req_id, msgpack::sbuffer &&buffer) {
        size_t size = buffer.size();
        if (size >= BUFFER_SIZE) {
            throw std::runtime_error("params size too large!");
        }
        Message msg{req_id, std::make_shared<std::string>(buffer.release())};
        RpcHeader header{msg.content->size(), msg.req_id};
        res_len_ = msg.content->size() + HEADER_LENGTH;
        res_.resize(res_len_);
            
        memcpy(res_.data(), &header, HEADER_LENGTH);
        memcpy(res_.data() + HEADER_LENGTH, msg.content->c_str(), msg.content->size());
    
        send(socket_fd_, res_.data(), res_len_, 0);
    }

    void read() {
        std::cout << "read" << std::endl;
        while (has_connect_) {
            recv(socket_fd_, head_, HEADER_LENGTH, 0);
            RpcHeader *header = (RpcHeader *)(head_);
            req_id_ = header->req_id;
            body_len_ = header->body_len;
            std::cout << "header " << body_len_ << std::endl;
            if (body_len_ > 0 && body_len_ < BUFFER_SIZE) {
                body_.resize(body_len_);
                recv(socket_fd_, body_.data(), body_len_, 0);
                std::string res = std::string(body_.data(), body_.size());
                call_back(req_id_, res);
            } else {
                stop();
            }
        }
    }
private:
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
    std::vector<char> body_;
    std::vector<char> res_;
};