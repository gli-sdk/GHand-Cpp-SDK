#ifndef ETHERCAT_COMM_H_
#define ETHERCAT_COMM_H_

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

#include "xiaoyao/file_lock.h"

namespace xiaoyao {

constexpr size_t kFirmwareBufferSize = 8 * 1024 * 1024;

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

    int BootUpdate(const std::string& ifname,
                   uint16_t slave,
                   const std::string& filename,
                   std::function<void(int)> progressCallback);

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

private:
    static std::mutex context_mutex_;
    static std::mutex rt_context_mutex_;
    static xiaoyao::FileLock device_lock_;
    static ecx_contextt ctx_;
    static std::function<void(int)> progress_callback_;

    void ResetContext();
    bool InputBin(const char* fname, int* length);
    static void ProcessPendingPDOs();

    static uint8_t IOmap_[4096];
    static OSAL_THREAD_HANDLE threadrt;
    static OSAL_THREAD_HANDLE thread1;
    static bool threads_started_;
    static int expectedWKC;
    static int wkc;
    static int mappingdone;
    static int dorun;
    static int inOP;
    static int dowkccheck;
    static int currentgroup;
    static int cycle;
    static int64_t cycletime;

    static void add_time_ns(ec_timet* ts, int64_t addtime);
    static void ec_sync(int64_t reftime, int64_t cycletime, int64_t* offsettime);
    static OSAL_THREAD_FUNC_RT Ecatthread(void);
    static OSAL_THREAD_FUNC Ecatcheck(void);
    void StartThreads();
    void StopThreads();

    char file_buffer[kFirmwareBufferSize] = {0};
    static void FoeProgressHook(uint16_t slave, int32_t packetnumber, int32_t totalsize);

    static std::function<void(int)> state_update_callback_;
    static std::function<void(const uint8_t*, size_t)> data_callback_;
    static std::mutex data_callback_mutex_;
};

}  // namespace xiaoyao

#endif  // ETHERCAT_COMM_H_
