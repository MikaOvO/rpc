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
    bool SafeQueue<T>::empty() {
        std::lock_guard<std::mutex> lock(mu);
        return que.empty();
    }
    size_t SafeQueue<T>::size() {
        std::lock_guard<std::mutex> lock(mu_);
        return que_.size();
    }
    void SafeQueue<T>::push(T t) {
        std::lock_guard<std::mutex> lock(mu_);
        que_.push(t);
    }
    T SafeQueue<T>::front() {
        std::lock_guard<std::mutex> lock(mu_);
        return que_.front();
    }
    void SafeQueue<T>::pop() {
        std::lock_guard<std::mutex> lock(mu_);
        que_.pop();
    }
    bool SafeQueue<T>::pop_with_check(T &t) {
        std::lock_guard<std::mutex> lock(mu_);
        if (que_.size() == 0) {
            return false;
        }
        t = que_.front();
        que_.pop();
        return true;
    }
};
