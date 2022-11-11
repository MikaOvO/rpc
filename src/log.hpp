#pragma once

#include <mutex>
#include <cstring>
#include <thread>
#include <fstream>
#include <ctime>
#include <iostream>
#include <map>
#include <unordered_map>

#include "common.hpp"

const static char* level_info[4] = {
    "[debug]  ", 
    "[info]   ",
    "[warning]",
    "[error]  "
};
const static char* default_file_name = "log";

class Log {
public:
    template<typename... Args>
    static void WriteLog(int level, const char *file_name_, const char *format, Args &&...args) {
        if (level < LOG_LEVEL) {
            return;
        }
        time_t now = time(0);
        tm *local_tm = localtime(&now);
        char year[6], month[4], day[4];
        
        snprintf(year, 6, "%04d", local_tm->tm_year + 1900);
        snprintf(month, 4, "%02d", local_tm->tm_mon + 1);
        snprintf(day, 4, "%02d", local_tm->tm_mday);
        
        char hour[4], minute[4], second[4];
        
        snprintf(hour, 4, "%02d", local_tm->tm_hour);
        snprintf(minute, 4, "%02d", local_tm->tm_min);
        snprintf(second, 4, "%02d", local_tm->tm_sec);
        
        char file[128], buffer[1024];

        snprintf(file, 128, "%s/%s_%s_%s_%s.txt", log_dir, year, month, day, file_name_);
        snprintf(buffer, 1024, format, args...);

        std::unique_lock<std::mutex> lock(mtx);
        // if (current_file == nullptr || file != current_file) {
        //     if (current_file != nullptr) {
        //         file_stream.close();
        //     }
        //     file_stream.open(file, std::ios::app);
        //     current_file = file;
        // }
        file_stream.open(file, std::ios::app);

        file_stream << hour << ":" << minute << ":" << second << " ";
        file_stream << level_info[level] << " ";  
        file_stream << buffer;

        file_stream.close();
    }

    template<typename... Args>
    static void WriteLogDefault(int level, const char *format, Args &&...args) {
        Log::WriteLog(level, default_file_name, format, args...);
    }

private:
    Log() {
    }
    ~Log() {
    }
    std::ofstream static file_stream;
    std::mutex static mtx;
};

std::ofstream Log::file_stream;
std::mutex Log::mtx;