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

#include "common.hpp"
#include "router.hpp"
#include "sync_connection.hpp"
#include "thread_pool.hpp"
#include "timer.hpp"
#include "utils.hpp"

class SyncRpcServer : public std::enable_shared_from_this<SyncConnection> {
public:
    SyncRpcServer(unsigned short port, int thread_num, triger trig_mode) : has_stop_(false), trig_mode_(trig_mode), client_number_(0) {
        port_ = port;
        users_ = new SyncConnection[MAX_FD];
        thread_pool_ = new ThreadPool(thread_num);
        events_ = new epoll_event[MAX_EVENTS];
        trig_mode_ = trig_mode;
        router_ptr_ = std::make_shared<Router>(new Router());
    }
    ~SyncRpcServer() {
        
    }
    static void handler(std::shared_ptr<SyncRpcServer> server, int sig) {
        server->stop();
    }
    void run() {
        auto func = std::bind(handler, this->shared_from_this());

        add_sig(SIGINT, func, false);
        add_sig(SIGTERM, func, false);

        thread_pool_->run();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        event_listen();
        event_loop();
    }
    void stop() {
        if (has_stop_) {
            return;
        }
        Timer::stop();
        thread_pool_->shutdown();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (listen_fd_ != -1) {
            close(listen_fd_);
        }
        if (epoll_fd_ != -1) {
            close(epoll_fd_);
        }
        delete[] users_;
        delete thread_pool_;
        delete[] events_;

        has_stop_ = true;
    }
private:
    void add_user(int sock_fd, struct sockaddr_in client_address) {
        add_fd(epoll_fd_, sock_fd, true, trig_mode_);
        users_[sock_fd].init(epoll_fd_, sock_fd, trig_mode_, router_ptr_);
        Timer::add_sock(sock_fd);
    }

    bool deal_client_data() {
        struct sockaddr_in client_address;
        socklen_t client_addrlength = sizeof(client_address);
        if (trig_mode_ == triger::LT_triger) {
            int sock_fd = accept(listen_fd_, (struct sockaddr *)&client_address, &client_addrlength);
            if (sock_fd < 0) {
                return false;
            }
            if (client_number_ >= MAX_FD - 1) {
                return false;
            }
            add_user(sock_fd, client_address);
        } else {
            while (1) {
                int sock_fd = accept(listen_fd_, (struct sockaddr *)&client_address, &client_addrlength);
                if (sock_fd < 0) {
                    break;
                }
                if (client_number_ >= MAX_FD - 1) {
                    return false;
                }
                add_user(sock_fd, client_address);                
            }
            return false;
        }        
        return true;
    }

    void event_listen() {
        listen_fd_ = socket(PF_INET, SOCK_STREAM, 0);
        assert(listen_fd_ >= 0);

        int ret = 0;
        struct sockaddr_in address;
        bzero(&address, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port_);

        ret = bind(listen_fd_, (struct sockaddr *)&address, sizeof(address));
        assert(ret >= 0);
        ret = listen(listen_fd_, 5);
        assert(ret >= 0);
    
        epoll_fd_ = epoll_create(5);
        assert(epoll_fd_ != -1);
        
        add_fd(epoll_fd_, listen_fd_, false, trig_mode_);
        
        Timer::init(epoll_fd_);
        Timer::run();
    }

    void event_loop() {

    }

    triger trig_mode_;
    unsigned short port_;
    std::atomic<bool> has_stop_;

    SyncConnection *users_;
    ThreadPool *thread_pool_;
    epoll_event *events_;

    int listen_fd_;
    int epoll_fd_;

    int client_number_;
    
    std::shared_ptr<Router> router_ptr_;
};