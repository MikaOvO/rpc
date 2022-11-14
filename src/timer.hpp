#pragma once

#include <time.h>
#include <functional>
#include <queue>
#include <vector>
#include <map>
#include <thread>
#include <cassert>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <mutex>
#include <atomic>

#include "epoll_utils.hpp"
#include "utils.hpp"
#include "common.hpp"

class Timer {
public:
    using expire_sock = std::pair<time_t, int>;
public:
    Timer() {

    }
    ~Timer() {

    }

    static void init(int epollfd) {
        epollfd = epollfd_;
    }
    static void run() {
        add_sig(SIGALRM, alarm_handler, false);
        alarm(ALARM_TIME_SLOT);
    }
    static void stop() {
        stop_ = true;
        if (clean_thread.joinable()) {
            clean_thread.join();
        }
    }
    static void alarm_handler(int sig) {
        clean_thread = std::thread([]() {
            tick();
        });
        if (stop_) {
            return;
        }
        alarm(ALARM_TIME_SLOT);
    }
    static void tick() {
        std::lock_guard<std::mutex> guard(lock_);
        time_t cur = time(NULL);
        while (sock_queue_.size()) {
            auto p = sock_queue_.top();
            int sockfd = p.second;
            sock_queue_.pop();
            if (p.first + CONNECT_TIME_SLOT > cur) {
                break;
            }
            if (expire_map_.find(sockfd) == expire_map_.end() || expire_map_[sockfd] != p.first) {
                continue;
            }
            delete_sock(sockfd);
        }
    }
    static void add_sock(int sockfd) {
        std::lock_guard<std::mutex> guard(lock_);
        time_t cur = time(NULL);
        expire_map_[sockfd] = cur;
        sock_queue_.push(expire_sock(cur, sockfd));
    }
    static void delete_sock(int sockfd) {
        std::lock_guard<std::mutex> guard(lock_);
        expire_map_.erase(sockfd);
        epoll_ctl(epollfd_, EPOLL_CTL_DEL, sockfd, 0);
        close(sockfd);
    }
    static void flush_sock(int sockfd) {
        std::lock_guard<std::mutex> guard(lock_);
        time_t cur = time(NULL);
        Timer::expire_map_[sockfd] = cur;
        sock_queue_.push(expire_sock(cur, sockfd));
    }
public:
    static std::atomic<bool> stop_;
    static std::mutex lock_;
    static int epollfd_;
    static std::thread clean_thread;
    static std::priority_queue<expire_sock, std::vector<expire_sock>, std::greater<expire_sock> > sock_queue_;
    static std::unordered_map<int, time_t> expire_map_;
};

std::atomic<bool> Timer::stop_(false);
std::thread Timer::clean_thread;
std::mutex Timer::lock_;
int Timer::epollfd_;
std::priority_queue<Timer::expire_sock, std::vector<Timer::expire_sock>, std::greater<Timer::expire_sock> > Timer::sock_queue_;
std::unordered_map<int, time_t> Timer::expire_map_;
