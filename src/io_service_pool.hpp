#pragma once

#include <string.h>
#include <boost/asio.hpp>

#include "log.hpp"

using namespace boost;

class IOServicePool {
public:
    explicit IOServicePool(size_t pool_size) : next_io_service_(0) {
        Log::write_log_default(0, "[IOServicePool] init\n");
        if (pool_size <= 0) {
            throw std::runtime_error("io_service_pool size <= 0!");
        }
        pool_size_ = pool_size;
        for (size_t i = 0; i < pool_size; ++i) {
            std::shared_ptr<asio::io_context> service_ptr(new asio::io_context());
            std::shared_ptr<asio::io_context::work> work_ptr(new asio::io_context::work(*service_ptr));
            io_services_.emplace_back(service_ptr);
            works_.emplace_back(work_ptr);
        }
        Log::write_log_default(0, "[IOServicePool] init success\n");
    }
    void run() {
        std::vector<std::shared_ptr<std::thread> > threads;
        for (size_t i = 0; i < pool_size_; ++i) {
            for (size_t j = 0; j < THREAD_NUMBER_PER_CONTEXT; ++j) {
                threads.emplace_back(
                std::make_shared<std::thread>(
                    [](std::shared_ptr<asio::io_context> isp) { isp->run(); }, io_services_[i]   
                )
                );
            }
        }
        Log::write_log_default(0, "[IOServicePool] run success\n");
        for (size_t i = 0; i < pool_size_ * THREAD_NUMBER_PER_CONTEXT; ++i) {
            threads[i]->join();
        }
    }
    void stop() {
        for (size_t i = 0; i < pool_size_; ++i) {
            io_services_[i]->stop();
        }
    }
    asio::io_context& get_io_service() {
        auto &io_service = *io_services_[next_io_service_]; 
        ++next_io_service_;
        if (next_io_service_ == pool_size_) {
            next_io_service_ = 0;
        }
        return io_service;
    }
    IOServicePool(const IOServicePool &) = delete;
    IOServicePool& operator = (const IOServicePool &) = delete;
private:
    std::vector<std::shared_ptr<asio::io_context> > io_services_;
    std::vector<std::shared_ptr<asio::io_context::work> > works_;
    size_t next_io_service_;
    size_t pool_size_;
};