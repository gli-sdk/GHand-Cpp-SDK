#ifndef GHAND_INTERNAL_FILE_LOCK_H_
#define GHAND_INTERNAL_FILE_LOCK_H_

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ghand {
namespace internal {

/**
 * @brief 跨平台文件锁内部实现类
 *
 * 提供进程间的互斥访问机制，防止多个进程同时使用同一设备。
 * 使用 RAII 模式管理锁的生命周期，确保异常安全。
 *
 * 平台支持：
 * - Windows: 使用 CreateFile 独占模式
 * - Linux: 使用 flock 系统调用
 *
 * 锁文件格式：ghand_ethernet_{md5(adapter_name)}.lock
 * 锁文件位置：
 * - Windows: %TEMP% 目录
 * - Linux: /tmp 目录
 */
class FileLock {
 public:
  FileLock();
  ~FileLock();

  bool Acquire(const std::string& lock_file);
  void Release();
  bool IsLocked() const;

 private:
  std::string lock_file_;

#ifdef _WIN32
  int fd_;
#else
  int fd_;
#endif

  bool is_locked_;
};

std::string GetAdapterLockPath(const std::string& adapter_name);

}  // namespace internal
}  // namespace ghand

#endif  // GHAND_INTERNAL_FILE_LOCK_H_
