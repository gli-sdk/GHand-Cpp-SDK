#ifndef XIAOYAO_INTERNAL_LOGGER_H_
#define XIAOYAO_INTERNAL_LOGGER_H_

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <ctime>

// 防止 Windows 宏冲突
#ifdef ERROR
#undef ERROR
#endif

namespace xiaoyao {
namespace internal {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    DEBUG = 10,
    INFO = 20,
    WARNING = 30,
    ERR = 40
};

/**
 * @brief 内部日志实现类
 *
 * 这个类包含所有的实现细节，不暴露给用户。
 */
class Logger {
public:
    static Logger& GetInstance();
    void SetConsoleLevel(LogLevel level);
    void SetFileLog(const std::string& filename, LogLevel level = LogLevel::DEBUG);
    void Log(LogLevel level, const char* file, int line, const std::string& message);
    std::ostringstream& GetStream(LogLevel level, const char* file, int line);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string FormatTime(bool iso_format = false) const;
    std::string LevelToString(LogLevel level) const;
    std::string FormatMessage(LogLevel level, const char* file, int line,
                              const std::string& message, bool verbose) const;

    LogLevel console_level_;
    std::unique_ptr<std::ofstream> file_stream_;
    LogLevel file_level_;
    std::mutex mutex_;
    std::unique_ptr<std::ostringstream> stream_buffer_;
    LogLevel current_level_;
    std::string current_file_;
    int current_line_;
};

}  // namespace internal
}  // namespace xiaoyao

// ============================================================================
// 内部日志宏（供 SDK 内部使用）
// ============================================================================

#define LOG_DEBUG(x) \
    do { \
        auto& stream = ::xiaoyao::internal::Logger::GetInstance().GetStream( \
            ::xiaoyao::internal::LogLevel::DEBUG, __FILE__, __LINE__); \
        stream << x; \
        ::xiaoyao::internal::Logger::GetInstance().Log( \
            ::xiaoyao::internal::LogLevel::DEBUG, __FILE__, __LINE__, stream.str()); \
    } while(0)

#define LOG_INFO(x) \
    do { \
        auto& stream = ::xiaoyao::internal::Logger::GetInstance().GetStream( \
            ::xiaoyao::internal::LogLevel::INFO, __FILE__, __LINE__); \
        stream << x; \
        ::xiaoyao::internal::Logger::GetInstance().Log( \
            ::xiaoyao::internal::LogLevel::INFO, __FILE__, __LINE__, stream.str()); \
    } while(0)

#define LOG_WARNING(x) \
    do { \
        auto& stream = ::xiaoyao::internal::Logger::GetInstance().GetStream( \
            ::xiaoyao::internal::LogLevel::WARNING, __FILE__, __LINE__); \
        stream << x; \
        ::xiaoyao::internal::Logger::GetInstance().Log( \
            ::xiaoyao::internal::LogLevel::WARNING, __FILE__, __LINE__, stream.str()); \
    } while(0)

#define LOG_ERROR(x) \
    do { \
        auto& stream = ::xiaoyao::internal::Logger::GetInstance().GetStream( \
            ::xiaoyao::internal::LogLevel::ERR, __FILE__, __LINE__); \
        stream << x; \
        ::xiaoyao::internal::Logger::GetInstance().Log( \
            ::xiaoyao::internal::LogLevel::ERR, __FILE__, __LINE__, stream.str()); \
    } while(0)

#endif  // XIAOYAO_INTERNAL_LOGGER_H_
