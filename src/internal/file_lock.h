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
 * @brief Cross-platform file lock internal implementation class
 *
 * Provides inter-process mutual exclusion to prevent multiple processes from using the same device simultaneously.
 * Uses RAII to manage the lock lifecycle, ensuring exception safety.
 *
 * Platform support:
 * - Windows: uses CreateFile exclusive mode
 * - Linux: uses flock system call
 *
 * Lock file format: ghand_ethernet_{md5(adapter_name)}.lock
 * Lock file location:
 * - Windows: %TEMP% directory
 * - Linux: /tmp directory
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
