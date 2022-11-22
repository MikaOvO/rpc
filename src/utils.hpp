#pragma once

#include <string.h>
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
#include <algorithm> 
#include <math.h>
#include <mutex>
#include <iostream>
#include <unordered_map>

void add_sig(int sig, void (*handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

char* resize(char *p, int n) {
    char *q = new char[n + 1];
    q[n] = '\0';
    if (p != nullptr)
        delete []p;
    return q;
}

void debug_char(char *p, int n) {
    for (int i = 0; i < n; ++i) {
        std::cout << (int)(p[i]) << " ";
    } 
    std::cout << "\n";
}

class time_count {
public:
    std::mutex mtx;
    long double count_;
    std::unordered_map<int, std::chrono::_V2::steady_clock::time_point> mp_;
    time_count() : count_(-1) {
    
    }
    ~time_count() {
        std::cout << "cost_time : " << count_  << "s" << std::endl;
    }
    void add(int i) {
        std::unique_lock<std::mutex> lock(mtx);
        auto f = std::chrono::steady_clock::now();
        mp_[i] = f;
    }
    void call(int i) {
        std::unique_lock<std::mutex> lock(mtx);
        auto num = (std::chrono::steady_clock::now() - mp_[i]);
        int64_t tmp = std::chrono::duration_cast<std::chrono::microseconds>(num).count();
        count_ = std::max(count_, (long double)tmp / 1e6);
    }
};

// char* resize_and_copy(char *p, int pn, int n) {
//     char *q = new char[n];
//     if (p != nullptr) {
//         memcpy(q, p, pn);
//         delete []p;
//     }
//     swap(p, q);
// }