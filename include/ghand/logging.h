#ifndef GHAND_LOGGING_H_
#define GHAND_LOGGING_H_

#include <sstream>
#include <string>

#include "export.h"

// 防止 Windows ERROR 宏冲突
#ifdef ERROR
#undef ERROR
#endif

namespace ghand {

enum class LogLevel { DEBUG = 10, INFO = 20, WARNING = 30, ERR = 40 };

GHAND_API void Log(LogLevel level, const char* file, int line,
                   const std::string& message);
GHAND_API void ConfigureConsole(LogLevel level);
GHAND_API void ConfigureFile(const std::string& filename,
                             LogLevel level = LogLevel::DEBUG);

}  // namespace ghand

#define LOG_DEBUG(x)                                                           \
  do {                                                                         \
    std::ostringstream _log_stream;                                            \
    _log_stream << x;                                                          \
    ghand::Log(ghand::LogLevel::DEBUG, __FILE__, __LINE__, _log_stream.str()); \
  } while (0)

#define LOG_INFO(x)                                                           \
  do {                                                                        \
    std::ostringstream _log_stream;                                           \
    _log_stream << x;                                                         \
    ghand::Log(ghand::LogLevel::INFO, __FILE__, __LINE__, _log_stream.str()); \
  } while (0)

#define LOG_WARNING(x)                                       \
  do {                                                       \
    std::ostringstream _log_stream;                          \
    _log_stream << x;                                        \
    ghand::Log(ghand::LogLevel::WARNING, __FILE__, __LINE__, \
               _log_stream.str());                           \
  } while (0)

#define LOG_ERROR(x)                                                         \
  do {                                                                       \
    std::ostringstream _log_stream;                                          \
    _log_stream << x;                                                        \
    ghand::Log(ghand::LogLevel::ERR, __FILE__, __LINE__, _log_stream.str()); \
  } while (0)

#endif  // GHAND_LOGGING_H_
