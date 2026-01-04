#ifndef LOG_FILE_H
#define LOG_FILE_H

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

class LogFile {
   private:
    std::string file_path_;
    std::mutex log_mutex_;

   public:
    explicit LogFile(const std::string& file_path) : file_path_(file_path) {}

    ~LogFile() { Close(); }

    void WriteLog(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::ofstream log_file(file_path_, std::ios::app);
        if (log_file.is_open()) {
            log_file << message << std::endl;
            log_file.close();
        } else {
            std::cerr << "Failed to open log file: " << file_path_ << std::endl;
        }
    }

    void ClearLog() {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::ofstream log_file(file_path_, std::ios::trunc);
        if (log_file.is_open()) {
            log_file.close();
        } else {
            std::cerr << "Failed to clear log file: " << file_path_ << std::endl;
        }
    }

    void Close() {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::ofstream log_file(file_path_, std::ios::app);
        if (log_file.is_open()) {
            log_file.close();
        }
    }
};

#endif  // LOG_FILE_H