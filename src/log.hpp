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
const static char* dir = "/home/mika/workspace/cpp_workspace/rpc/log";

class Log {
public:
    static Log* GetInstance() {
        static Log log;
        return &log;
    }
    
    template<typename... Args>
    void InstanceWriteLog(int level, const char *file_name_, const char *format, Args &&...args) {
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

        snprintf(file, 128, "%s/%s_%s_%s_%s.txt", dir, year, month, day, file_name_);
        snprintf(buffer, 1024, format, args...);

        if (current_file == nullptr || file != current_file) {
            if (current_file != nullptr) {
                file_stream.close();
            }
            file_stream.open(file, std::ios::app);
            current_file = file;
        }
        file_stream << hour << ":" << minute << ":" << second << " ";
        file_stream << level_info[level] << " ";  
        file_stream << buffer;

        file_stream.close();
        current_file = nullptr;
    }

    template<typename... Args>
    void InstanceWriteLogDefault(int level, const char *format, Args &&...args) {
        InstanceWriteLog(level, default_file_name, format, args...);
    }

    template<typename... Args>
    static void WriteLog(int level, const char *file_name_, const char *format, Args &&...args) {
        Log::GetInstance()->InstanceWriteLog(level, file_name_, format, args...);
    }

    template<typename... Args>
    static void WriteLogDefault(int level, const char *format, Args &&...args) {
        Log::GetInstance()->InstanceWriteLog(level, default_file_name, format, args...);
    }

private:
    Log() {
        current_file = nullptr;
    }
    virtual ~Log() {
        if (current_file != nullptr) {
            file_stream.close();
        }
    }
    char* current_file;
    std::ofstream file_stream;
};