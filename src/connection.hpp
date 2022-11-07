#pragma once

#include <boost/asio.hpp>
#include <queue>
#include <vector>

#include "common.hpp"
#include "log.hpp"
#include "router.hpp"

using namespace boost;

class Connection : public std::enable_shared_from_this<Connection>,
                   private asio::noncopyable {
public:
    Connection(asio::io_service &io_service, Router &router, size_t timeout_seconds)
              : socket_(io_service), timeout_seconds_(timeout_seconds), has_stop_(false), body_(INIT_BUFFER_SIZE), timer_(io_service), router_(router) {
    }
    ~Connection() {
        stop();
    }
    void run() {
        read_head();
    }
    bool has_stop() {
        return has_stop_;
    }
    void stop() {
        if (has_stop_) {
            return;
        }
        system::error_code ignore_ec;
        if (socket_.is_open()) {
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore_ec);
            socket_.close(ignore_ec);
        }
        has_stop_ = true;
    }
    int64_t get_conn_id() {
        return conn_id_;
    }
    void set_conn_id(int64_t conn_id) {
        conn_id_ = conn_id;
    }
    asio::ip::tcp::socket& get_socket() {
        return socket_;
    }
    void response(uint64_t req_id, std::string data) {
        size_t len = data.size();
        std::unique_lock<std::mutex> lock(write_queue_mutex_);
        write_queue_.push_back({req_id_, std::make_shared<std::string>(std::move(data))});
        if (write_queue_.size() > 1) {
            return;
        }        
        write();
    }
private:
    void read_head() {
        reset_timer();
        asio::async_read(socket_, asio::buffer(head_, HEADER_LENGTH), [this](system::error_code ec, size_t length){
            if (!socket_.is_open()) {
                return;
            }
            if (!ec) {
                RpcHeader *header = (RpcHeader *)(head_);
                req_id_ = header->req_id;
                body_len_ = header->body_len;
                Log::WriteLogDefault(0, "[Connection] Get header req_id: %lld, len: %lld\n", req_id_, body_len_);
                if (body_len_ > 0 && body_len_ < BUFFER_SIZE) {
                    if (body_.size() < body_len_) {
                        body_.resize(body_len_);
                    }
                    read_body();
                } else if (body_len_ == 0) {
                    cancel_timer();
                    read_head();
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
            cancel_timer();
            
            if (!socket_.is_open()) {
                return;
            }

            if (!ec) {
                read_head();
                Log::WriteLogDefault(0, "[Connection] Get body: %s\n", body_.data());
                router_.router<Connection>(body_.data(), length, this->shared_from_this());
            } else {
                stop();
            }
        });
    }
    void write() {
        Message &msg = write_queue_.front();

        write_size_ = (uint32_t)msg.content->size();
        std::array<asio::const_buffer, 3> write_buffers;
        write_buffers[0] = asio::buffer(&write_size_, sizeof(uint32_t));
        write_buffers[1] = asio::buffer(&msg.req_id, sizeof(uint64_t));
        write_buffers[2] = asio::buffer(msg.content->data(), write_size_);

        asio::async_write(socket_, write_buffers, [this](system::error_code ec, size_t length) {
            if (has_stop_) {
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
    void reset_timer() {
        timer_.expires_from_now(std::chrono::seconds(timeout_seconds_));
        timer_.async_wait([this](const system::error_code &ec) {
            if (has_stop_) {
                return;
            }
            if (ec) {
                return;
            }
            stop();
        });
    }
    void cancel_timer() {
        timer_.cancel();
    }
    std::mutex write_queue_mutex_;
    std::deque<Message> write_queue_;
    bool has_stop_;
    uint32_t write_size_;
    uint32_t body_len_;
    uint64_t req_id_;
    int64_t conn_id_;
    size_t timeout_seconds_;
    asio::ip::tcp::socket socket_;
    asio::steady_timer timer_;
    char head_[HEADER_LENGTH];
    std::vector<char> body_;
    
    Router &router_;
};