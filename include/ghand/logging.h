#ifndef GHAND_LOGGING_H_
#define GHAND_LOGGING_H_

#include <string>

#include "ghand/export.h"

namespace ghand {

enum class LogLevel { DEBUG = 10, INFO = 20, WARNING = 30, ERR = 40 };

GHAND_API void Log(LogLevel level, const char* file, int line,
                   const std::string& message);
GHAND_API void ConfigureConsole(LogLevel level);
GHAND_API void ConfigureFile(const std::string& filename,
                             LogLevel level = LogLevel::DEBUG);

}  // namespace ghand

#endif  // GHAND_LOGGING_H_
