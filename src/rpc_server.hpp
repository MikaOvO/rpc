#pragma once

#include <boost/asio.hpp>

#include "connection.hpp"
#include "io_service_pool.hpp"
#include "log.hpp"
#include "common.hpp"
#include "router.hpp"

using namespace boost;

class RpcServer : private asio::noncopyable {
public:
    RpcServer(unsigned short port, size_t size, size_t timeout_seconds = 15, size_t check_seconds = 10) 
             : client_number_(0), has_stop_(false), timeout_seconds_(timeout_seconds), check_seconds_(check_seconds), io_service_pool_(size), 
               acceptor_(io_service_pool_.get_io_service(), asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)), signals_(io_service_pool_.get_io_service()) {
        do_accept();
        signals_.add(SIGINT);
        signals_.add(SIGTERM);
        signals_.add(SIGQUIT);
        check_sthread_ptr_ = std::make_shared<std::thread>([this](){
            clean();
        });
        do_await_stop();
    }
    ~RpcServer() {
        stop();
    }
    void run() {
        Log::WriteLogDefault(0, "[RpcServer] start...\n");
        io_service_pool_.run();
        Log::WriteLogDefault(0, "[RpcServer] start success\n");
    }
    void async_run() {
        thread_ptr_ = std::make_shared<std::thread>(
            [this]() {
                io_service_pool_.run();
            }
        );
    }
    void stop() {
        if (has_stop_) {
            return;
        }
        io_service_pool_.stop();
        if (thread_ptr_) {
            thread_ptr_->join();
        }
        check_sthread_ptr_->join();
        has_stop_ = true;
        stop_check_ = true;
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
    void do_await_stop() {
        signals_.async_wait(
            [this](std::error_code, int sig) {
                Log::WriteLogDefault(0, "[server] Get sig %d\n", sig); 
                stop(); 
        });
    }
    void do_accept() {
        conn_.reset(new Connection(io_service_pool_.get_io_service(), router_, timeout_seconds_));
        acceptor_.async_accept(conn_->get_socket(), [this](system::error_code ec) {
            if (!acceptor_.is_open()) {
                return;
            }
            
            if (!ec) {
                Log::WriteLogDefault(0, "[RpcServer] Get a client \n");
                conn_->set_conn_id(client_number_);
                conn_->run();
                {
                    std::unique_lock<std::mutex> lock(connections_mutex_);
                    connections_.emplace(client_number_, conn_);
                    ++client_number_;
                }
            }

            do_accept();
        });
    }
    void clean() {
        while (!stop_check_) {
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_TIME));
            for (auto it = connections_.cbegin(); it != connections_.cend();) {
                if (it->second->has_stop()) {
                    it = connections_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    bool has_stop_;
    size_t timeout_seconds_;
    size_t check_seconds_;
    IOServicePool io_service_pool_;
    asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<std::thread> thread_ptr_;
    std::shared_ptr<std::thread> check_sthread_ptr_;
    int64_t client_number_;
    std::shared_ptr<Connection> conn_;
    std::unordered_map<int64_t, std::shared_ptr<Connection>> connections_;
    std::mutex connections_mutex_;

    asio::signal_set signals_;

    std::atomic<bool> stop_check_{false};

    Router router_;
};