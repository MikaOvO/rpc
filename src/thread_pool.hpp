#pragma once

#include <future>

#include "work_thread.hpp"

class ThreadPool {
public:
    ThreadPool(int thread_number) {
        thread_number_ = thread_number;
        thread_common_data_ptr_ = std::make_shared<ThreadCommonData>();
    }
    ~ThreadPool() {
        if (! thread_common_data_ptr_ -> get_shutdown()) {
            shutdown();
        }
    }
    void run() {
        for (int i = 0; i < thread_number_; ++i) {
            thread_vector_.emplace_back(WorkThread(i, thread_common_data_ptr_));
        }
    }
    void shutdown() {
        thread_common_data_ptr_ -> set_shutdown(true);
        thread_common_data_ptr_ ->thread_conditional_variable.notify_all();
        for (int i = 0; i < thread_number_; ++i) {
            if (thread_vector_[i].joinable()) {
                thread_vector_[i].join();
            }
        }
    }
    template <typename F, typename... Args>
    auto ThreadPool::submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        std::function<void()> warpper_func = [task_ptr]()
        {
            (*task_ptr)();
        };

        thread_common_data_ptr_ -> safe_queue.push(warpper_func);
        thread_common_data_ptr_ -> thread_conditional_variable.notify_one();

        return task_ptr -> get_future();
    }
private:
    std::shared_ptr<ThreadCommonData> thread_common_data_ptr_;
    std::vector<std::thread> thread_vector_;
    int thread_number_;
};
