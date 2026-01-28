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

#include "log_file.h"

class EtherCATComm {
   public:
    EtherCATComm();
    ~EtherCATComm();

    static map<string, string> ListAdapters();
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
    static ecx_contextt ctx_;             // 实时线程专用上下文
    static ecx_contextt ctx_shadow_;      // SDO 操作专用影子上下文
    static std::atomic<bool> ctx_dirty_;  // 标记影子上下文是否需要同步
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
    static LogFile* log_file_;  // 日志文件流
    std::mutex log_mutex_;      // 日志互斥锁

   private:
    static std::function<void(int)> state_update_callback_;

   public:
    static void SetStateUpdateCallback(std::function<void(int)> callback) {
        state_update_callback_ = callback;
    }
    static void NotifySlaveState(int slave, int state) {
        if (state_update_callback_) {
            state_update_callback_(state);
        }
    }
};
#endif

template <typename T, size_t Capacity>
class LockFreeQueue {
   private:
    struct Node {
        T data;
        std::atomic<size_t> next;

        Node() : next(0) {}
    };

    typedef char Cacheline[64];
    Cacheline pad0;
    std::atomic<size_t> head;
    Cacheline pad1;
    std::atomic<size_t> tail;
    Cacheline pad2;
    std::vector<Node> nodes;

   public:
    LockFreeQueue() : head(0), tail(0), nodes(Capacity + 1) {}

    bool Push(const T& item) {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % (Capacity + 1);

        if (next_tail == head.load(std::memory_order_acquire)) {
            return false;  // 队列满
        }

        nodes[current_tail].data = item;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    bool Pop(T& item) {
        size_t current_head = head.load(std::memory_order_relaxed);

        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;  // 队列空
        }

        item = std::move(nodes[current_head].data);
        head.store((current_head + 1) % (Capacity + 1), std::memory_order_release);
        return true;
    }
};