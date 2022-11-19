#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>

#include "safe_queue.hpp"

class ThreadCommonData {
public:
    SafeQueue<std::function<void()>> safe_queue;
    std::condition_variable thread_conditional_variable;
    std::mutex thread_mutex;
    ThreadCommonData() : shutdown_(false) {
        
    }
    void set_shutdown(bool shutdown) {
        std::lock_guard<std::mutex> lock(shutdown_mutex_);
        shutdown_ = shutdown;
    }
    bool get_shutdown() {
        std::lock_guard<std::mutex> lock(shutdown_mutex_);
        return shutdown_;
    }
private:
    bool shutdown_;
    std::mutex shutdown_mutex_;
};

class WorkThread : std::thread {
public:
    WorkThread(int thread_id, std::shared_ptr<ThreadCommonData> thread_common_data_ptr) {
        thread_id_ = thread_id;
        thread_common_data_ptr_ = thread_common_data_ptr;
    }
    void operator()() {
        std::function<void()> func;
        bool pop_flag;
        while (! thread_common_data_ptr_ -> get_shutdown()) {
            std::unique_lock<std::mutex> lock(thread_common_data_ptr_ -> thread_mutex);
            if (thread_common_data_ptr_ -> safe_queue.empty()) {
                thread_common_data_ptr_ -> thread_conditional_variable.wait(lock);
            }
            lock.unlock();
            pop_flag = thread_common_data_ptr_ -> safe_queue.pop_with_check(func);
            if (pop_flag) {
                func();
            }
        }
    }
private:
    int thread_id_;
    std::shared_ptr<ThreadCommonData> thread_common_data_ptr_;
};