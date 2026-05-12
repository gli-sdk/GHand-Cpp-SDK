#ifndef XIAOYAO_LOGGING_H_
#define XIAOYAO_LOGGING_H_

#include "export.h"
#include <string>
#include <sstream>

// 防止 Windows 宏冲突
#ifdef ERROR
#undef ERROR
#endif

namespace xiaoyao {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    DEBUG = 10,
    INFO = 20,
    WARNING = 30,
    ERR = 40
};

// 前向声明 detail 命名空间
namespace detail {
GHAND_API void LogMessage(LogLevel level, const char* file, int line, const std::string& message);
}

/**
 * @brief 配置控制台日志级别
 *
 * 只支持 INFO 和 DEBUG 两个级别，用于降低日志级别门槛（从默认 WARNING 升级）。
 *
 * @param level 日志级别，只接受 INFO 或 DEBUG
 */
GHAND_API void ConfigureConsole(LogLevel level);

/**
 * @brief 配置文件日志输出
 *
 * 文件日志与控制台日志独立，可以设置不同的级别。
 * 默认使用详细格式（包含文件名和行号）。
 *
 * @param filename 日志文件路径
 * @param level 日志级别，默认为 DEBUG
 */
GHAND_API void ConfigureFile(const std::string& filename, LogLevel level = LogLevel::DEBUG);

}  // namespace xiaoyao

// ============================================================================
// 便捷日志宏
// ============================================================================

// 避免与内部宏重定义（当内部头文件已包含时）
#ifndef LOG_DEBUG
#define LOG_DEBUG(x) \
    do { \
        std::ostringstream _log_stream; \
        _log_stream << x; \
        xiaoyao::detail::LogMessage(xiaoyao::LogLevel::DEBUG, __FILE__, __LINE__, _log_stream.str()); \
    } while(0)
#endif

#ifndef LOG_INFO
#define LOG_INFO(x) \
    do { \
        std::ostringstream _log_stream; \
        _log_stream << x; \
        xiaoyao::detail::LogMessage(xiaoyao::LogLevel::INFO, __FILE__, __LINE__, _log_stream.str()); \
    } while(0)
#endif

#ifndef LOG_WARNING
#define LOG_WARNING(x) \
    do { \
        std::ostringstream _log_stream; \
        _log_stream << x; \
        xiaoyao::detail::LogMessage(xiaoyao::LogLevel::WARNING, __FILE__, __LINE__, _log_stream.str()); \
    } while(0)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(x) \
    do { \
        std::ostringstream _log_stream; \
        _log_stream << x; \
        xiaoyao::detail::LogMessage(xiaoyao::LogLevel::ERR, __FILE__, __LINE__, _log_stream.str()); \
    } while(0)
#endif

#endif  // XIAOYAO_LOGGING_H_
