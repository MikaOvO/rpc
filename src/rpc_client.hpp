#pragma once

#include <boost/asio.hpp>
#include <queue>
#include <vector>
#include <iostream>

#include "common.hpp"
#include "msgpack_utils.hpp"

using namespace boost;

template<typename T>
class FutureResult {
public:
    FutureResult(uint64_t id, std::future<T> &&future): id_(id), future_(future) {

    }
    template<typename Rep, typename Per>
    std::future_status wait_for(const std::chrono::duration<Rep, Per> &rel_time) {
        return future_.wait_for(rel_time);
    }
    T get() {
        return future_.get();
    }
    uint64_t id_;
    std::future<T> future_;
};

class ReqResult {
public:
    ReqResult(std::string &data) : data_(std::move(data)) {
    }
    template <typename T> 
    T get_result() {
        auto tp = unpack<std::tuple<int, T> >(data_.c_str(), data.length());
        return std::get<1>(tp);
    }
private:
    std::string data_;
};

class RpcClient : private asio::noncopyable {
public:
    RpcClient(const std::string &host, unsigned short port) 
             : socket_(io_service_), work_(io_service_), host_(std::move(host)), port_(port), has_connect_(false), body_(INIT_BUFFER_SIZE), req_id_(0) {
        std::cout << "init\n";
        thread_ptr_ = std::make_shared<std::thread>(
            [this]() {
                io_service_.run();
            }
        );
        std::cout << "init success\n";
    }
    ~RpcClient() {
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
        system::error_code ignore_ec;
        if (socket_.is_open()) {
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore_ec);
            socket_.close(ignore_ec);
        }
        has_connect_ = false;

        if (thread_ptr_ != nullptr) {
            io_service_.stop();
            if (thread_ptr_->joinable()) {
                thread_ptr_->join();
            }
        }
    }
    template<size_t timeout, typename T = void, typename... Args>
    typename std::enable_if<std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        
    }
    
    template<typename T = void, typename... Args>
    typename std::enable_if<std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        call<CLIENT_DEFAULT_TIMEOUT, T>(rpc_name, std::forward<Args>(args)...);
    }

    template<size_t timeout, typename T, typename... Args>
    typename std::enable_if<!std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        auto future_result = async_call(rpc_name, std::forward<Args>(args)...);
        auto status = future_result.wait_for(std::chrono::milliseconds(timeout));
        if (status == std::future_status::timeout || status == std::future_status::deferred) {
            throw std::runtime_error("sync call timeout or deferred");
        }
        return future_result.get().get_result<T>();
    }
    
    template<typename T, typename... Args>
    typename std::enable_if<!std::is_void<T>::value, T>::type
    call(const std::string &rpc_name, Args &&...args) {
        return call<CLIENT_DEFAULT_TIMEOUT, T>(rpc_name, std::forward<Args>(args)...);
    }
    template <typename... Args>
    FutureResult<ReqResult> async_call(const std::string &rpc_name,
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
        return FutureResult<ReqResult>(req_id, std::move(future));
    }
public:
    void async_connect() {
        std::cout << "connect...\n";
        assert(port_ != 0);
        auto addr = asio::ip::address::from_string(host_);
        socket_.async_connect({addr, port_}, [this](const system::error_code &ec) {
            if (has_connect_) {
                return;
            }

            if (!ec) {
                has_connect_ = true;
                read_head();
            }
        });
    }
    void call_back(uint64_t req_id, std::string &data) 
    {
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
    void read_head() {
        asio::async_read(socket_, asio::buffer(head_, HEADER_LENGTH), [this](system::error_code ec, size_t length){
            if (!socket_.is_open()) {
                return;
            }
            if (!ec) {
                RpcHeader *header = (RpcHeader *)(head_);
                req_id_ = header->req_id;
                body_len_ = header->body_len;
                std::cout << "head: " << req_id_ << " " << body_len_ << std::endl;
                if (body_len_ > 0 && body_len_ < BUFFER_SIZE) {
                    read_body();
                } else {
                    stop();
                }
            } else {
                stop();
            }
        });
    }
    void read_body() {
        asio::async_read(socket_, asio::buffer(body_.data(), body_len_), [this](system::error_code ec, size_t length) {
            if (!socket_.is_open()) {
                return;
            }

            if (!ec) {
                read_head();
                // TODO logic
                std::cout << "body: " << body_.data() << std::endl;
            } else {
                stop();
            }
        });
    }
    void write(uint64_t req_id, msgpack::sbuffer &&buffer) {
        size_t size = buffer.size();
        if (size < BUFFER_SIZE) {
            throw std::runtime_error("params size too large!");
        }
        Message msg{req_id, std::make_shared<std::string>(buffer.release())};

        std::unique_lock<std::mutex>(write_queue_mutex_);
        write_queue_.push_back(std::move(msg));
        if (write_queue_.size() > 1) {
            return;
        }
        write();
    }
    void write() {
        Message &msg = write_queue_.front();

        write_size_ = (uint32_t)msg.content->size();
        std::array<asio::const_buffer, 3> write_buffers;
        write_buffers[0] = asio::buffer(&write_size_, sizeof(uint32_t));
        write_buffers[1] = asio::buffer(&msg.req_id, sizeof(uint64_t));
        write_buffers[2] = asio::buffer(msg.content->data(), write_size_);

        asio::async_write(socket_, write_buffers, [this](system::error_code ec, size_t length) {
            if (!has_connect_) {
                return;
            }
            if (ec) {
                stop();
                return;
            }

            std::unique_lock<std::mutex> lock(write_queue_mutex_);
            write_queue_.pop_front();
            if (!write_queue_.empty()) {
                write();
            }
        });
    }
    asio::io_context io_service_;

    std::mutex write_queue_mutex_;
    std::deque<Message> write_queue_;
    uint32_t write_size_;
    uint32_t body_len_;
    char head_[HEADER_LENGTH];
    std::vector<char> body_;

    bool has_connect_;
    asio::ip::tcp::socket socket_;
    asio::io_context::work work_;
    std::shared_ptr<std::thread> thread_ptr_;
    std::string host_;
    unsigned short port_;

    std::mutex req_mutex_;
    uint64_t req_id_;
    std::unordered_map<std::uint64_t, std::shared_ptr<std::promise<ReqResult>>> future_map_;
};