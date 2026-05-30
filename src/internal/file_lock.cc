#include "file_lock.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "ghand/logging.h"
#include "logging_macros.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <wincrypt.h>
#include <windows.h>

// MinGW may not define these constants; define them manually
#ifndef _LK_NBLCK
#define _LK_NBLCK 0x01 /* Non-blocking lock */
#define _LK_LOCK 0x02  /* Blocking lock */
#define _LK_UNLCK 0x03 /* Unlock */
#define _LK_RLCK 0x04  /* Read lock */
#endif

#else
#include <fcntl.h>
#include <openssl/md5.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ghand {
namespace internal {

// =============================================================================
// MD5 hash function implementation (Task 1.3)
// =============================================================================

namespace {

/**
 * @brief Generate MD5 hash of a string
 *
 * Platform-specific implementation:
 * - Windows: Uses Cryptographic API (CryptHashData)
 * - Linux: Uses OpenSSL MD5() function
 *
 * @param input Input string
 * @return 32-character hexadecimal MD5 hash string, empty string on failure
 */
std::string MD5Hash(const std::string& input) {
#ifdef _WIN32
  // Windows implementation: uses Cryptographic API
  HCRYPTPROV hProv = 0;
  HCRYPTHASH hHash = 0;
  BYTE rgbHash[16];
  DWORD cbHash = 16;

  // Acquire cryptographic service provider handle
  if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT)) {
    GHAND_LOG_DEBUG("Failed to acquire crypt context");
    return "";
  }

  // Create hash object
  if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
    CryptReleaseContext(hProv, 0);
    GHAND_LOG_DEBUG("Failed to create hash");
    return "";
  }

  // Compute hash value
  if (!CryptHashData(hHash,
                     reinterpret_cast<BYTE*>(const_cast<char*>(input.c_str())),
                     static_cast<DWORD>(input.length()), 0)) {
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    GHAND_LOG_DEBUG("Failed to hash data");
    return "";
  }

  // Retrieve hash result
  if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0)) {
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    GHAND_LOG_DEBUG("Failed to get hash param");
    return "";
  }

  CryptDestroyHash(hHash);
  CryptReleaseContext(hProv, 0);

  // Convert to hexadecimal string
  std::stringstream ss;
  for (int i = 0; i < 16; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(rgbHash[i]);
  }
  return ss.str();

#else
  // Linux implementation: uses OpenSSL
  unsigned char digest[16];

  MD5((unsigned char*)input.c_str(), input.length(), digest);

  // Convert to hexadecimal string
  std::stringstream ss;
  for (int i = 0; i < 16; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(digest[i]);
  }
  return ss.str();
#endif
}

}  // anonymous namespace

// =============================================================================
// FileLock class implementation (Task 1.2)
// =============================================================================

FileLock::FileLock() : fd_(-1), is_locked_(false) {}

FileLock::~FileLock() {
  try {
    Release();
  } catch (...) {
    // Ignore all exceptions during destruction to ensure safe cleanup
  }
}

bool FileLock::Acquire(const std::string& lock_file) {
  if (is_locked_) {
    std::cerr << "Lock already acquired" << '\n';
    return false;  // Lock already held
  }

  lock_file_ = lock_file;

#ifdef _WIN32
  // Windows implementation: fully mimics Python SDK approach
  // Uses same behavior as Python open() + msvcrt.locking()
  FILE* file_ptr = nullptr;
  errno_t err = fopen_s(&file_ptr, lock_file.c_str(), "w");
  if (err != 0 || file_ptr == nullptr) {
    return false;
  }

  int fd = _fileno(file_ptr);
  if (fd == -1) {
    fclose(file_ptr);
    return false;
  }

  if (_locking(fd, _LK_NBLCK, 1) < 0) {
    fclose(file_ptr);
    return false;
  }

  DWORD pid = GetCurrentProcessId();
  std::string content = std::to_string(pid) + "\n" + lock_file + "\n";
  fwrite(content.c_str(), static_cast<size_t>(content.size()), 1, file_ptr);
  fflush(file_ptr);

  fd_ = _dup(fd);
  if (fd_ == -1) {
    fclose(file_ptr);
    return false;
  }

  fclose(file_ptr);
  is_locked_ = true;
  return true;

#else
  // Linux implementation: uses open() + flock()
  fd_ = open(lock_file.c_str(), O_WRONLY | O_CREAT, 0666);
  if (fd_ < 0) {
    return false;
  }

  if (flock(fd_, LOCK_EX | LOCK_NB) < 0) {
    close(fd_);
    fd_ = -1;
    return false;
  }

  pid_t pid = getpid();
  std::string content = std::to_string(pid) + "\n" + lock_file + "\n";
  ssize_t written = write(fd_, content.c_str(), content.size());
  (void)written;
  fsync(fd_);

  is_locked_ = true;
  return true;
#endif
}

void FileLock::Release() {
  if (!is_locked_) {
    return;  // Lock not held, return immediately
  }

#ifdef _WIN32
  // Windows implementation: release lock and close file
  if (fd_ >= 0) {
    // 1. Release file lock
    _locking(fd_, _LK_UNLCK, 1);

    // 2. Close file descriptor
    _close(fd_);
    fd_ = -1;
  }

  // 3. Delete lock file
  DeleteFileA(lock_file_.c_str());

#else
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
  unlink(lock_file_.c_str());
#endif

  is_locked_ = false;
  lock_file_.clear();
}

bool FileLock::IsLocked() const { return is_locked_; }

// =============================================================================
// GetAdapterLockPath function implementation (Task 1.4)
// =============================================================================

std::string GetAdapterLockPath(const std::string& adapter_name) {
  // Generate MD5 hash of adapter name
  std::string hash = MD5Hash(adapter_name);
  if (hash.empty()) {
    std::cerr << "Failed to generate MD5 hash for adapter: " << adapter_name
              << '\n';
    return "";
  }

#ifdef _WIN32
  // Windows: uses %TEMP% directory
  char temp_path[MAX_PATH];
  DWORD result = GetTempPathA(MAX_PATH, temp_path);
  if (result == 0 || result > MAX_PATH) {
    std::cerr << "Failed to get temp path" << '\n';
    return "";
  }

  std::string lock_path(temp_path);
  // Ensure path ends with backslash
  if (!lock_path.empty() && lock_path.back() != '\\') {
    lock_path += '\\';
  }
  lock_path += "ghand_ethernet_" + hash + ".lock";

#else
  // Linux: fixed to use /tmp directory
  std::string lock_path = "/tmp/ghand_ethernet_" + hash + ".lock";
#endif

  return lock_path;
}

}  // namespace internal
}  // namespace ghand
