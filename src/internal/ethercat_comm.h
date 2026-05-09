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
#include "icomm.h"
#include "product_config.h"

namespace xiaoyao {
namespace internal {

constexpr size_t kFirmwareBufferSize = 8 * 1024 * 1024;

/**
 * @brief EtherCAT通信内部实现类
 *
 * 该类处理所有EtherCAT通信相关的实现细节。
 * 通过Pimpl模式和internal命名空间与公共API分离。
 */
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
    static std::mutex context_mutex_;
    static std::mutex rt_context_mutex_;
    static FileLock device_lock_;
    static ecx_contextt ctx_;
    static std::function<void(int)> progress_callback_;

    void ResetContext();
    bool InputBin(const char* fname, int* length);

    static uint8_t IOmap_[4096];
    static OSAL_THREAD_HANDLE threadrt;
    static OSAL_THREAD_HANDLE thread1;
    static bool threads_started_;
    static int expectedWKC;
    static int wkc;
    static int mappingdone;
    static int dorun;
    static int inOP;
    // 连接状态标志：表示设备的逻辑连接状态
    // 特殊情况：BootUpdate() 后保持为 true（期望重连成功），仅在确认重连失败后才设置为 false
    static bool is_connected_;
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
    static EtherCATComm* active_instance_;

    // 实例级别的结构化数据回调
    JointsCallback joints_callback_;
    HandStateCallback hand_state_callback_;
    TactileDataCallback tactile_callback_;
    std::mutex callback_mutex_;

    // 解析 PDO 原始数据并触发结构化回调
    void ParseAndNotify(const uint8_t* data, size_t size);

    const ProductConfig& config_;
};

}  // namespace internal
}  // namespace xiaoyao

#endif  // XIAOYAO_INTERNAL_ETHERCAT_COMM_H_
