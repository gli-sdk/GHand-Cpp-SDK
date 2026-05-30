#include "ghand/logging.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>

namespace ghand {
namespace internal {

// Logger class declaration (for internal use only in this file)
class Logger {
 public:
  static Logger& GetInstance();
  void SetConsoleLevel(LogLevel level);
  void SetFileLog(const std::string& filename,
                  LogLevel level = LogLevel::DEBUG);
  void Log(LogLevel level, const char* file, int line,
           const std::string& message);
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

// ============================================================================
// Logger class implementation
// ============================================================================

Logger::Logger()
    : console_level_(LogLevel::WARNING),
      file_stream_(nullptr),
      file_level_(LogLevel::DEBUG),
      mutex_(),
      stream_buffer_(new std::ostringstream()),
      current_level_(LogLevel::INFO),
      current_file_(),
      current_line_(0) {}

Logger::~Logger() {
  if (file_stream_ && file_stream_->is_open()) {
    file_stream_->close();
  }
}

Logger& Logger::GetInstance() {
  static Logger instance;
  return instance;
}

void Logger::SetConsoleLevel(LogLevel level) {
  // Only INFO and DEBUG levels are allowed
  if (level != LogLevel::INFO && level != LogLevel::DEBUG) {
    std::cerr
        << "[ERROR] Invalid console log level. Only INFO and DEBUG are allowed."
        << '\n';
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  console_level_ = level;
}

void Logger::SetFileLog(const std::string& filename, LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Close existing file stream
  if (file_stream_ && file_stream_->is_open()) {
    file_stream_->close();
  }

  // Create new file stream
  file_stream_.reset(new std::ofstream(filename, std::ios::app));
  if (!file_stream_->is_open()) {
    std::cerr << "[ERROR] Failed to open log file: " << filename << '\n';
    file_stream_.reset();
    return;
  }

  file_level_ = level;
}

void Logger::Log(LogLevel level, const char* file, int line,
                 const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check console log level
  if (level >= console_level_) {
    std::string msg = FormatMessage(level, file, line, message, false);
    std::cerr << msg << '\n';
  }

  // Check file log level
  if (file_stream_ && file_stream_->is_open() && level >= file_level_) {
    std::string msg = FormatMessage(level, file, line, message, true);
    *file_stream_ << msg << '\n';
    file_stream_->flush();
  }
}

std::ostringstream& Logger::GetStream(LogLevel level, const char* file,
                                      int line) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_level_ = level;
  current_file_ = file;
  current_line_ = line;
  stream_buffer_->str("");
  stream_buffer_->clear();
  return *stream_buffer_;
}

std::string Logger::FormatTime(bool iso_format) const {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::tm tm_buf;
#ifdef _WIN32
  localtime_s(&tm_buf, &time);
#else
  localtime_r(&time, &tm_buf);
#endif

  std::ostringstream oss;
  if (iso_format) {
    oss << std::setfill('0') << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << ms.count();
  } else {
    oss << std::setfill('0') << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
  }
  return oss.str();
}

std::string Logger::LevelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

std::string Logger::FormatMessage(LogLevel level, const char* file, int line,
                                  const std::string& message,
                                  bool verbose) const {
  std::ostringstream oss;

  if (verbose) {
    // Verbose format (for file logging)
    // Extract filename (remove path)
    const char* filename = file;
    const char* last_slash = file;
    while (*file) {
      if (*file == '/' || *file == '\\') {
        last_slash = file + 1;
      }
      file++;
    }
    filename = last_slash;

    oss << FormatTime(true) << " "
        << "[" << LevelToString(level) << "] "
        << "[" << filename << ":" << line << "] " << message;
  } else {
    // Concise format (for console)
    oss << FormatTime(false) << " "
        << "[" << LevelToString(level) << "] " << message;
  }

  return oss.str();
}

}  // namespace internal

void Log(LogLevel level, const char* file, int line,
         const std::string& message) {
  internal::Logger::GetInstance().Log(level, file, line, message);
}

void ConfigureConsole(LogLevel level) {
  internal::Logger::GetInstance().SetConsoleLevel(level);
}

void ConfigureFile(const std::string& filename, LogLevel level) {
  internal::Logger::GetInstance().SetFileLog(filename, level);
}

}  // namespace ghand
