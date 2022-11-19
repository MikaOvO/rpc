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
#include <set>

#include "epoll_utils.hpp"
#include "utils.hpp"
#include "common.hpp"
#include "log.hpp"
#include "thread_pool.hpp"

class Timer {
public:
    using expire_sock = std::pair<time_t, int>;
public:
    Timer() {

    }
    ~Timer() {

    }

    static void init(int epollfd, ThreadPool *thread_pool) {
        epollfd_ = epollfd;
        thread_pool_ = thread_pool;
    }
    static void run() {
        if (!stop_) {
            return;
        }
        Log::write_log_default(0, "[timer] run\n");
        stop_ = false;
        add_sig(SIGALRM, alarm_handler, false);
        alarm(ALARM_TIME_SLOT);
    }
    static void stop() {
        if (stop_) {
            return;
        }
        Log::write_log_default(0, "[timer] stop\n");
        stop_ = true;
        std::lock_guard<std::mutex> guard(lock_);
        for (auto pair : expire_map_) {
            int sockfd = pair.first;
            close(sockfd);
        }
    }
    static void alarm_handler(int sig) {
        if (stop_) {
            return;
        }
        thread_pool_->submit(tick);
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
            Log::write_log_default(0, "[timer] clear %d\n", sockfd);
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
        if (expire_map_.find(sockfd) == expire_map_.end()) {
            return ;
        }
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
    static std::priority_queue<expire_sock, std::vector<expire_sock>, std::greater<expire_sock> > sock_queue_;
    static std::unordered_map<int, time_t> expire_map_;
    static ThreadPool* thread_pool_;
};

ThreadPool* Timer::thread_pool_(nullptr);
std::atomic<bool> Timer::stop_(true);
std::mutex Timer::lock_;
int Timer::epollfd_;
std::priority_queue<Timer::expire_sock, std::vector<Timer::expire_sock>, std::greater<Timer::expire_sock> > Timer::sock_queue_;
std::unordered_map<int, time_t> Timer::expire_map_;
