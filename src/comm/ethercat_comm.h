#ifndef GHAND_INTERNAL_ETHERCAT_COMM_H_
#define GHAND_INTERNAL_ETHERCAT_COMM_H_

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
#include "icomm.h"
#include "product_config.h"

namespace ghand {
namespace internal {

constexpr size_t kFirmwareBufferSize = 8 * 1024 * 1024;

class EtherCATComm : public IComm {
public:
    explicit EtherCATComm(const ProductConfig& config);
    ~EtherCATComm() override;

    // ===== IComm 接口实现 =====
    int Connect(const std::string& device_name) override;
    int Disconnect() override;
    bool IsConnected() const override {
        return is_connected_;
    }
    std::map<std::string, std::string> SearchAdapters() override;

    DeviceInfo GetDeviceInfo() override;
    HandType GetHandType() override;

    bool MoveJoints(const std::vector<JointCommand>& joints,
                    ControlMode mode) override;
    void Stop() override;

    bool ClearFault() override;
    bool InitJoint() override;

    bool OpenTactile() override;
    bool CloseTactile() override;
    bool ZeroTactile() override;

    void SetJointsCallback(JointsCallback cb) override;
    void SetHandStateCallback(HandStateCallback cb) override;
    void SetTactileDataCallback(TactileDataCallback cb) override;

    int BootUpdate(const std::string& device_name,
                   uint16_t slave,
                   const std::string& filename,
                   std::function<void(int)> progress) override;

    // ===== 底层 EtherCAT API（保留供内部使用）=====
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

private:
    // === 实例状态 ===
    ecx_contextt ctx_{};
    uint8_t IOmap_[4096] = {0};

    OSAL_THREAD_HANDLE threadrt_{};
    OSAL_THREAD_HANDLE thread1_{};
    bool threads_started_ = false;

    int expectedWKC_ = 0;
    int wkc_ = 0;
    int mappingdone_ = 0;
    std::atomic<int> dorun_{0};
    int inOP_ = 0;
    bool is_connected_ = false;
    int dowkccheck_ = 0;
    int currentgroup_ = 0;
    int cycle_ = 0;
    int64_t cycletime_ = 10000000;

    uint8_t rxpdo_buffer_[80] = {0};

    std::mutex context_mutex_;
    std::mutex rt_context_mutex_;
    FileLock device_lock_;

    std::function<void(int)> state_update_callback_;

    char file_buffer_[kFirmwareBufferSize] = {0};

    // 回调
    JointsCallback joints_callback_;
    HandStateCallback hand_state_callback_;
    TactileDataCallback tactile_callback_;
    std::mutex callback_mutex_;

    const ProductConfig& config_;

    // === FOE 固件升级共享状态（同一时刻仅允许一个固件升级操作）===
    static std::function<void(int)> progress_callback_;
    static EtherCATComm* foe_instance_;

    // === 方法 ===
    void ResetContext();
    bool InputBin(const char* fname, int* length);

    void StartThreads();
    void StopThreads();

    // 实例线程方法（由静态包装函数调用）
    void Ecatthread();
    void Ecatcheck();

    // 静态线程入口（OSAL 要求 C 函数指针）
    static OSAL_THREAD_FUNC_RT EcatthreadWrapper(void* arg);
    static OSAL_THREAD_FUNC EcatcheckWrapper(void* arg);

    // 静态辅助方法
    static void add_time_ns(ec_timet* ts, int64_t addtime);
    static void ec_sync(int64_t reftime, int64_t cycletime, int64_t* offsettime);
    static void FoeProgressHook(uint16_t slave, int32_t packetnumber, int32_t totalsize);

    // 解析 PDO 原始数据并触发结构化回调
    void ParseAndNotify(const uint8_t* data, size_t size);
};

}  // namespace internal
}  // namespace ghand

#endif  // GHAND_INTERNAL_ETHERCAT_COMM_H_
