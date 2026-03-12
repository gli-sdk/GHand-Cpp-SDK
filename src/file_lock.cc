#include "internal/file_lock.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

// MinGW 可能没有定义这些常量，手动定义
#ifndef _LK_NBLCK
#define _LK_NBLCK 0x01  /* 非阻塞锁 */
#define _LK_LOCK  0x02  /* 阻塞锁 */
#define _LK_UNLCK 0x03  /* 解锁 */
#define _LK_RLCK  0x04  /* 读锁 */
#endif

#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#endif

namespace xiaoyao {

// =============================================================================
// MD5 哈希函数实现（任务 1.3）
// =============================================================================

namespace {

/**
 * @brief 生成字符串的 MD5 哈希值
 *
 * 平台特定的实现：
 * - Windows: 使用 Cryptographic API (CryptHashData)
 * - Linux: 使用 OpenSSL MD5() 函数
 *
 * @param input 输入字符串
 * @return 32 字符的十六进制 MD5 哈希字符串，失败返回空字符串
 */
std::string MD5Hash(const std::string& input) {
#ifdef _WIN32
    // Windows 实现：使用 Cryptographic API
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[16];
    DWORD cbHash = 16;

    // 获取加密服务提供者句柄
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        std::cerr << "Failed to acquire crypt context" << std::endl;
        return "";
    }

    // 创建哈希对象
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        std::cerr << "Failed to create hash" << std::endl;
        return "";
    }

    // 计算哈希值
    if (!CryptHashData(hHash, (BYTE*)input.c_str(), static_cast<DWORD>(input.length()), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        std::cerr << "Failed to hash data" << std::endl;
        return "";
    }

    // 获取哈希结果
    if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        std::cerr << "Failed to get hash param" << std::endl;
        return "";
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    // 转换为十六进制字符串
    std::stringstream ss;
    for (int i = 0; i < 16; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)rgbHash[i];
    }
    return ss.str();

#else
    // Linux 实现：使用 OpenSSL
    unsigned char digest[16];

    MD5((unsigned char*)input.c_str(), input.length(), digest);

    // 转换为十六进制字符串
    std::stringstream ss;
    for (int i = 0; i < 16; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
#endif
}

} // anonymous namespace

// =============================================================================
// FileLock 类实现（任务 1.2）
// =============================================================================

FileLock::FileLock()
    : fd_(-1)
    , is_locked_(false) {
}

FileLock::~FileLock() {
    try {
        Release();
    } catch (...) {
        // 忽略析构时的所有异常，确保析构安全
    }
}

bool FileLock::Acquire(const std::string& lock_file) {
    if (is_locked_) {
        std::cerr << "Lock already acquired" << std::endl;
        return false; // 已经持有锁
    }

    lock_file_ = lock_file;

#ifdef _WIN32
    // Windows 实现：完全模拟 Python SDK 的方式
    // 直接使用 _open() + _locking()，与 Python 的 open() + msvcrt.locking() 一致

    // 1. 使用 _sopen_s() 打开文件（与 Python 的 open() 一致）
    int fd = -1;
    errno_t err = _sopen_s(&fd, lock_file.c_str(), _O_RDWR | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    if (err != 0 || fd < 0) {
        std::cerr << "Failed to open lock file: " << lock_file << std::endl;
        return false;
    }

    // 2. 使用 _locking() 获取文件锁（与 Python 的 msvcrt.locking() 一致）
    // 参数：文件描述符、锁模式（非阻塞）、锁定字节数（1字节）
    if (_locking(fd, _LK_NBLCK, 1) < 0) {
        std::cerr << "Failed to acquire lock: " << lock_file << std::endl;
        _close(fd);
        return false;
    }

    // 3. 写入当前进程 ID 和网卡名（与 Python SDK 格式一致）
    DWORD pid = GetCurrentProcessId();
    std::string content = std::to_string(pid) + "\n" + lock_file + "\n";
    _write(fd, content.c_str(), static_cast<unsigned int>(content.size()));

    // 保存文件描述符用于后续释放
    fd_ = fd;
    is_locked_ = true;
    return true;

#else
    // Linux 实现：使用 open() + flock()
    fd_ = open(lock_file.c_str(), O_WRONLY | O_CREAT, 0666);
    if (fd_ < 0) {
        std::cerr << "Failed to open lock file: " << lock_file << std::endl;
        return false;
    }

    // 尝试获取非阻塞独占锁
    if (flock(fd_, LOCK_EX | LOCK_NB) < 0) {
        std::cerr << "Failed to acquire lock: " << lock_file << std::endl;
        close(fd_);
        fd_ = -1;
        return false;
    }

    // 写入当前进程 ID 和网卡名
    pid_t pid = getpid();
    std::string content = std::to_string(pid) + "\n" + lock_file + "\n";
    write(fd_, content.c_str(), content.size());
    fsync(fd_);

    is_locked_ = true;
    return true;
#endif
}

void FileLock::Release() {
    if (!is_locked_) {
        return; // 未持有锁，直接返回
    }

#ifdef _WIN32
    // Windows 实现：释放锁并关闭文件
    if (fd_ >= 0) {
        // 1. 释放文件锁
        _locking(fd_, _LK_UNLCK, 1);

        // 2. 关闭文件描述符
        _close(fd_);
        fd_ = -1;
    }

    // 3. 删除锁文件
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

bool FileLock::IsLocked() const {
    return is_locked_;
}

// =============================================================================
// GetAdapterLockPath 函数实现（任务 1.4）
// =============================================================================

std::string GetAdapterLockPath(const std::string& adapter_name) {
    // 生成网卡名的 MD5 哈希
    std::string hash = MD5Hash(adapter_name);
    if (hash.empty()) {
        std::cerr << "Failed to generate MD5 hash for adapter: " << adapter_name << std::endl;
        return "";
    }

// #ifdef _WIN32
    // Windows: 使用 %TEMP% 目录
    char temp_path[MAX_PATH];
    DWORD result = GetTempPathA(MAX_PATH, temp_path);
    if (result == 0 || result > MAX_PATH) {
        std::cerr << "Failed to get temp path" << std::endl;
        return "";
    }

    std::string lock_path(temp_path);
    // 确保路径以反斜杠结尾
    if (!lock_path.empty() && lock_path.back() != '\\') {
        lock_path += '\\';
    }
    lock_path += "xiaoyao_ethernet_" + hash + ".lock";

// #else
//     // Linux: 固定使用 /tmp 目录
//     std::string lock_path = "/tmp/xiaoyao_ethernet_" + hash + ".lock";
// #endif

    return lock_path;
}

} // namespace xiaoyao
