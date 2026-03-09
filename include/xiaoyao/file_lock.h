#ifndef XIAOYAO_FILE_LOCK_H_
#define XIAOYAO_FILE_LOCK_H_

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace xiaoyao {

/**
 * @brief 跨平台文件锁类
 *
 * 提供进程间的互斥访问机制，防止多个进程同时使用同一设备。
 * 使用 RAII 模式管理锁的生命周期，确保异常安全。
 *
 * 平台支持：
 * - Windows: 使用 CreateFile 独占模式
 * - Linux: 使用 flock 系统调用
 *
 * 锁文件格式：xiaoyao_ethernet_{md5(adapter_name)}.lock
 * 锁文件位置：
 * - Windows: %TEMP% 目录
 * - Linux: /tmp 目录
 */
class FileLock {
public:
    /**
     * @brief 构造函数
     *
     * 初始化文件锁对象，将锁状态设为未锁定。
     */
    FileLock();

    /**
     * @brief 析构函数
     *
     * 自动释放锁（如果已获取）。忽略所有异常以确保析构安全。
     */
    ~FileLock();

    /**
     * @brief 尝试获取文件锁
     *
     * 使用非阻塞模式尝试获取锁。如果锁已被其他进程占用，
     * 立即返回 false。
     *
     * @param lock_file 锁文件完整路径
     * @return 成功返回 true，失败返回 false
     *
     * @note 锁文件内容包含进程 ID 和网卡名，用于调试
     * @note Windows: 使用 CreateFile 独占模式
     * @note Linux: 使用 open() + flock(LOCK_EX | LOCK_NB)
     */
    bool Acquire(const std::string& lock_file);

    /**
     * @brief 释放文件锁
     *
     * 关闭文件句柄/描述符并删除锁文件。
     * 如果锁未获取，此方法不执行任何操作。
     *
     * @note 此方法是幂等的，多次调用是安全的
     */
    void Release();

    /**
     * @brief 查询当前锁定状态
     *
     * @return 已锁定返回 true，未锁定返回 false
     */
    bool IsLocked() const;

private:
    std::string lock_file_;  ///< 锁文件路径

#ifdef _WIN32
    int fd_;                 ///< Windows 文件描述符（使用 _open() 而非 CreateFile）
#else
    int fd_;                 ///< Linux 文件描述符
#endif

    bool is_locked_;         ///< 锁定状态标志
};

/**
 * @brief 生成网卡锁文件路径
 *
 * 根据网卡名生成锁文件的完整路径。
 *
 * @param adapter_name 网卡名称（如 "eth0" 或 "\Device\NPF_{GUID}"）
 * @return 锁文件完整路径
 *
 * 路径格式：
 * - Windows: %TEMP%\xiaoyao_ethernet_{md5(adapter_name)}.lock
 * - Linux: /tmp/xiaoyao_ethernet_{md5(adapter_name)}.lock
 *
 * @note 使用 MD5 哈希避免网卡名中的特殊字符问题
 * @note 与 Python SDK 使用完全相同的路径格式
 */
std::string GetAdapterLockPath(const std::string& adapter_name);

} // namespace xiaoyao

#endif // XIAOYAO_FILE_LOCK_H_
