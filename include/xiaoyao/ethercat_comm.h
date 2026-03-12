#ifndef ETHERCAT_COMM_H_
#define ETHERCAT_COMM_H_

#define FWBUFSIZE (8 * 1024 * 1024)
#include <soem/soem.h>
#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
using namespace std;

#include "xiaoyao/file_lock.h"

namespace xiaoyao {

class EtherCATComm {
   public:
    EtherCATComm();
    ~EtherCATComm();

    static map<string, string> SearchAdapters();
    int Connect(std::string device_name);
    int Disconnect();

    int SDORead(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex, int* size,
                void* data, int timeout);
    int SDOWrite(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex, int size,
                 void* data, int timeout);
    int SendRxPDO(uint16 slave_id, uint16 pdo_index, uint32 data_size, uint8* data);
    uint8_t* ReadTxPDO(uint16 slave);
    int BootUpdate(char* ifname, uint16_t slave, char* filename,
                   std::function<void(int)> progressCallback);

   private:
    static std::mutex context_mutex_;     // 保护 SDO 操作和配置变更
    static std::mutex rt_context_mutex_;  // 保护实时上下文访问
    static xiaoyao::FileLock device_lock_; // 设备锁，确保进程间互斥访问网卡
    static ecx_contextt ctx_;             // 实时线程专用上下文
    static std::function<void(int)> progress_callback_;

    void ResetContext();
    bool InputBin(char* fname, int* length);
    static void ProcessPendingPDOs();

   private:
    static uint8 IOmap_[4096];
    static OSAL_THREAD_HANDLE threadrt, thread1;
    static bool threads_started_;
    static int expectedWKC;
    static int wkc;
    static int mappingdone, dorun, inOP, dowkccheck;
    static int currentgroup;
    static int cycle;
    static int64_t cycletime;

    static void add_time_ns(ec_timet* ts, int64 addtime);
    static void ec_sync(int64 reftime, int64 cycletime, int64* offsettime);
    static OSAL_THREAD_FUNC_RT Ecatthread(void);
    static OSAL_THREAD_FUNC Ecatcheck(void);
    void StartThreads();
    void StopThreads();

   private:
    char file_buffer[FWBUFSIZE] = {0};
    static void FoeProgressHook(uint16 slave, int32 packetnumber, int32 totalsize);

   private:
    static std::function<void(int)> state_update_callback_;
    static std::function<void(const uint8_t*, size_t)> data_callback_;
    static std::mutex data_callback_mutex_;

   public:
    /// @brief 检查是否已连接设备
    /// @return true 表示主站已初始化且检测到从站
    bool IsConnected() const {
        return ctx_.slavecount > 0;
    }

    static void SetStateUpdateCallback(std::function<void(int)> callback) {
        state_update_callback_ = callback;
    }
    static void NotifySlaveState(int slave, int state) {
        if (state_update_callback_) {
            state_update_callback_(state);
        }
    }

    // 数据回调：在接收到PDO数据后触发
    static void SetDataCallback(std::function<void(const uint8_t*, size_t)> callback) {
        std::lock_guard<std::mutex> lock(data_callback_mutex_);
        data_callback_ = callback;
    }
    static void NotifyDataReceived(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(data_callback_mutex_);
        if (data_callback_) {
            data_callback_(data, size);
        }
    }
};

}  // namespace xiaoyao

#endif  // ETHERCAT_COMM_H_