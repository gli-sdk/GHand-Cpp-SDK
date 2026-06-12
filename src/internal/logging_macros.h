#ifndef SRC_INTERNAL_LOGGING_MACROS_H_
#define SRC_INTERNAL_LOGGING_MACROS_H_

#include <sstream>

#include "ghand/logging.h"

#define GHAND_LOG_DEBUG(x)                                      \
  do {                                                          \
    std::ostringstream _log_stream;                             \
    _log_stream << x;                                           \
    ghand::Log(ghand::LogLevel::DEBUG, __FILE__, __LINE__,      \
               _log_stream.str());                              \
  } while (0)

#define GHAND_LOG_INFO(x)                                       \
  do {                                                          \
    std::ostringstream _log_stream;                             \
    _log_stream << x;                                           \
    ghand::Log(ghand::LogLevel::INFO, __FILE__, __LINE__,       \
               _log_stream.str());                              \
  } while (0)

#define GHAND_LOG_WARNING(x)                                    \
  do {                                                          \
    std::ostringstream _log_stream;                             \
    _log_stream << x;                                           \
    ghand::Log(ghand::LogLevel::WARNING, __FILE__, __LINE__,    \
               _log_stream.str());                              \
  } while (0)

#define GHAND_LOG_ERROR(x)                                      \
  do {                                                          \
    std::ostringstream _log_stream;                             \
    _log_stream << x;                                           \
    ghand::Log(ghand::LogLevel::ERR, __FILE__, __LINE__,        \
               _log_stream.str());                              \
  } while (0)

#endif  // SRC_INTERNAL_LOGGING_MACROS_H_
