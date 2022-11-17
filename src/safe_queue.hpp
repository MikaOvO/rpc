#pragma once

#include <queue>
#include <mutex>
#include <thread>

template<typename T>
class SafeQueue {
private:
    std::queue<T> que_;
    std::mutex mu_;
public:
    SafeQueue() {}
    ~SafeQueue() {}
    bool empty() {
        std::lock_guard<std::mutex> lock(mu_);
        return que_.empty();
    }
    size_t size() {
        std::lock_guard<std::mutex> lock(mu_);
        return que_.size();
    }
    void push(T t) {
        std::lock_guard<std::mutex> lock(mu_);
        que_.push(t);
    }
    T front() {
        std::lock_guard<std::mutex> lock(mu_);
        return que_.front();
    }
    void pop() {
        std::lock_guard<std::mutex> lock(mu_);
        que_.pop();
    }
    bool pop_with_check(T &t) {
        std::lock_guard<std::mutex> lock(mu_);
        if (que_.size() == 0) {
            return false;
        }
        t = que_.front();
        que_.pop();
        return true;
    }
};
