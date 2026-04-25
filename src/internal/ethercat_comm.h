#ifndef XIAOYAO_INTERNAL_ETHERCAT_COMM_H_
#define XIAOYAO_INTERNAL_ETHERCAT_COMM_H_

#include <soem/soem.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/mman.h>
#endif

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "file_lock.h"

namespace xiaoyao {
namespace internal {

constexpr size_t kFirmwareBufferSize = 8 * 1024 * 1024;

/**
 * @brief EtherCAT通信内部实现类
 *
 * 该类处理所有EtherCAT通信相关的实现细节。
 * 通过Pimpl模式和internal命名空间与公共API分离。
 */
class EtherCATComm {
public:
    EtherCATComm();
    ~EtherCATComm();

    static std::map<std::string, std::string> SearchAdapters();
    int Connect(std::string device_name);
    int Disconnect();

    int SDORead(std::uint16_t slave,
                std::uint16_t index,
                std::uint8_t subindex,
                int* size,
                void* data,
                int timeout);

    int SDOWrite(std::uint16_t slave,
                 std::uint16_t index,
                 std::uint8_t subindex,
                 int size,
                 void* data,
                 int timeout);

    int SendRxPDO(uint16_t slave_id,
                  uint16_t pdo_index,
                  uint32_t data_size,
                  uint8_t* data);

    uint8_t* ReadTxPDO(uint16_t slave);
    bool IsConnected() const {
        return is_connected_;
    }

    void SetDataCallback(std::function<void(const uint8_t*, size_t)> callback) {
        std::lock_guard<std::mutex> lock(data_callback_mutex_);
        data_callback_ = callback;
    }

    void NotifyDataReceived(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(data_callback_mutex_);
        if (data_callback_) {
            data_callback_(data, size);
        }
    }

    int BootUpdate(const std::string& ifname,
                   uint16_t slave,
                   const std::string& filename,
                   std::function<void(int)> progressCallback);


 private:
    std::mutex context_mutex_;
    std::mutex rt_context_mutex_;
    FileLock device_lock_;
    ecx_contextt ctx_;
    std::function<void(int)> progress_callback_;

    uint8_t IOmap_[4096];
    OSAL_THREAD_HANDLE threadrt;
    OSAL_THREAD_HANDLE thread1;
    bool threads_started_;
    int expectedWKC;
    int wkc;
    int mappingdone;
    int dorun;
    int inOP;
    // 连接状态标志：表示设备的逻辑连接状态
    // 特殊情况：BootUpdate() 后保持为 true（期望重连成功），仅在确认重连失败后才设置为 false
    bool is_connected_;
    int dowkccheck;
    int currentgroup;
    int cycle;
    int64_t cycletime;
    uint8_t rxpdo_buffer_[80];

    void ResetContext();
    bool InputBin(const char* fname, int* length);

    static void add_time_ns(ec_timet* ts, int64_t addtime);
    static void ec_sync(int64_t reftime, int64_t cycletime, int64_t* offsettime);

    // 线程入口（静态，兼容 SOEM 的线程创建 API）
    static void Ecatthread(void* param);
    static void Ecatcheck(void* param);
    // 实际的线程逻辑
    void EcatthreadImpl();
    void EcatcheckImpl();

    void StartThreads();
    void StopThreads();

    char file_buffer[kFirmwareBufferSize] = {0};
    static void FoeProgressHook(uint16_t slave, int32_t packetnumber, int32_t totalsize);

    std::function<void(int)> state_update_callback_;
    std::function<void(const uint8_t*, size_t)> data_callback_;
    std::mutex data_callback_mutex_;
};

}  // namespace internal
}  // namespace xiaoyao

#endif  // XIAOYAO_INTERNAL_ETHERCAT_COMM_H_
