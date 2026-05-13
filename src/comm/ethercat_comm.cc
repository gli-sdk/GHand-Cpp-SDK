// 在包含任何头文件之前禁用 inline 宏冲突错误
#ifdef _WIN32
    #define _USE_MATH_DEFINES
    #pragma warning(disable: 4005)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "ethercat_comm.h"
#include "ghand/logging.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/resource.h>
    #include <errno.h>
#endif

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#define EC_TIMEOUTMON 500
#define NSEC_PER_SEC 1000000000

namespace ghand {
namespace internal {

// 每关节数据尺寸: float(4B) + uint8 velocity(1B) + uint8 torque(1B)
constexpr size_t kEthercatJointDataSize = 6;

// === 静态成员定义 ===
std::function<void(int)> EtherCATComm::progress_callback_ = nullptr;
EtherCATComm* EtherCATComm::foe_instance_ = nullptr;
static std::atomic<bool> print_debug_info{false};

EtherCATComm::EtherCATComm(const ProductConfig& config) : config_(config) {}

EtherCATComm::~EtherCATComm() {
    StopThreads();
    Disconnect();
}

// === 静态线程包装函数 ===

OSAL_THREAD_FUNC_RT EtherCATComm::EcatthreadWrapper(void* arg) {
    auto* self = static_cast<EtherCATComm*>(arg);
    self->Ecatthread();
}

OSAL_THREAD_FUNC EtherCATComm::EcatcheckWrapper(void* arg) {
    auto* self = static_cast<EtherCATComm*>(arg);
    self->Ecatcheck();
}

void EtherCATComm::StartThreads() {
    if (!threads_started_) {
        dorun_ = 1;
        osal_thread_create_rt(&threadrt_, 128000, reinterpret_cast<void*>(EcatthreadWrapper), this);
        osal_thread_create(&thread1_, 128000, reinterpret_cast<void*>(EcatcheckWrapper), this);

        threads_started_ = true;
    }
}

void EtherCATComm::StopThreads() {
    if (threads_started_) {
        threads_started_ = false;
        dorun_ = 0;
        for (int i = 0; i < 50; ++i) {
            osal_usleep(1000);
        }
    }
}

std::map<std::string, std::string> EtherCATComm::SearchAdapters() {
    ec_adaptert* adapter = nullptr;
    ec_adaptert* head = nullptr;
    std::map<std::string, std::string> adapter_names;
    adapter_names.clear();

    head = adapter = ec_find_adapters();
    while (adapter != nullptr) {
        if ((std::string(adapter->name).find("Loopback") != std::string::npos)) {
            adapter = adapter->next;
            continue;
        }
        adapter_names.emplace(std::string(adapter->name), std::string(adapter->desc));
        adapter = adapter->next;
    }
    ec_free_adapters(head);
    return adapter_names;
}

int EtherCATComm::Connect(const std::string& device_name) {
    LOG_INFO("EtherCAT connecting to: " << device_name);

    std::lock_guard<std::mutex> lock(context_mutex_);

    // 步骤1：尝试获取设备锁（防止多进程同时访问）
    std::string lock_path = GetAdapterLockPath(device_name);
    if (!device_lock_.Acquire(lock_path)) {
        LOG_ERROR("Adapter " << device_name << " is already locked by another process");
        return -1;  // 设备已被占用
    }

    memset(rxpdo_buffer_, 0, sizeof(rxpdo_buffer_));
    rxpdo_buffer_[1] = {0x01};

    ResetContext();
    if (ecx_init(&ctx_, device_name.c_str()) <= 0) {
        LOG_ERROR("Unable to initialize EtherCAT adapter: " << device_name);
        device_lock_.Release();
        return -2;
    }

    int config_result = ecx_config_init(&ctx_);
    if (config_result <= 0 || ctx_.slavecount <= 0) {
        LOG_ERROR("No devices found on adapter: " << device_name);
        device_lock_.Release();
        return -3;
    }

    ec_groupt* group = &ctx_.grouplist[0];
    int map_result = ecx_config_map_group(&ctx_, &IOmap_, 0);
    if (map_result <= 0) {
        device_lock_.Release();
        return -4;
    }

    mappingdone_ = 1;
    dorun_ = 1;
    expectedWKC_ = (group->outputsWKC * 2) + group->inputsWKC;

    ecx_configdc(&ctx_);

    for (int si = 1; si <= ctx_.slavecount; si++) {
        ec_slavet* slave = &ctx_.slavelist[si];
        if (slave->CoEdetails > 0) {
            ecx_slavembxcyclic(&ctx_, si);
        }
    }

    uint16_t safe_op_state = ecx_statecheck(&ctx_, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

    if (safe_op_state != EC_STATE_SAFE_OP) {
        device_lock_.Release();
        return -5;
    }

    ctx_.slavelist[0].state = EC_STATE_OPERATIONAL;
    ecx_writestate(&ctx_, 0);

    StartThreads();

    uint16_t op_state = ecx_statecheck(&ctx_, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
    if (op_state != EC_STATE_OPERATIONAL) {
        device_lock_.Release();
        return -6;
    }

    inOP_ = 1;
    is_connected_ = true;

    return 0;
}

int EtherCATComm::Disconnect() {
    StopThreads();
    std::lock_guard<std::mutex> lock(context_mutex_);
    inOP_ = 0;
    is_connected_ = false;
    dorun_ = 0;

    memset(rxpdo_buffer_, 0, sizeof(rxpdo_buffer_));
    rxpdo_buffer_[1] = {0x01};
    if (ctx_.slavecount > 0) {
        ctx_.slavelist[0].state = EC_STATE_INIT;
        ecx_writestate(&ctx_, 0);
    }

    // 释放设备锁
    device_lock_.Release();

    return 0;
}

void EtherCATComm::ResetContext() {
    memset(&ctx_, 0, sizeof(ecx_contextt));
    memset(&IOmap_, 0, 4096);

    mappingdone_ = 0;
    expectedWKC_ = 0;
    wkc_ = 0;
    cycle_ = 0;
    currentgroup_ = 0;
    dorun_ = 0;
    inOP_ = 0;
    dowkccheck_ = 0;
}

int EtherCATComm::SDORead(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex,
                          int* size, void* data, int timeout) {
    if (!data || !size) {
        return -1;
    }

    int retries = 3;
    int result = -1;

    while (retries-- > 0) {
        {
            std::lock_guard<std::mutex> lock(rt_context_mutex_);
            result = ecx_SDOread(&ctx_, slave, index, subindex, FALSE, size, data, timeout);
            if (result > 0) break;
        }
        osal_usleep(10000);
    }

    return result;
}

int EtherCATComm::SDOWrite(std::uint16_t slave, std::uint16_t index, std::uint8_t subindex,
                           int size, void* data, int timeout) {
    if (!data || size <= 0) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(rt_context_mutex_);

    int retries = 3;
    int result = -1;

    while (retries-- > 0) {
        result = ecx_SDOwrite(&ctx_, slave, index, subindex, FALSE, size, data, timeout);
        if (result > 0) {
            break;
        }
        osal_usleep(10000);
    }

    return result;
}

int EtherCATComm::SendRxPDO(uint16 slave, uint16 pdo_index, uint32 data_size, uint8* data) {
    if (!data || data_size <= 0 || slave <= 0 || slave > ctx_.slavecount) {
        return -1;
    }

    if (ctx_.slavelist[slave].Obytes < data_size || data_size > sizeof(rxpdo_buffer_)) {
        return -1;
    }

    memcpy(rxpdo_buffer_, data, data_size);

    return 1;
}

uint8_t* EtherCATComm::ReadTxPDO(uint16 slave) {
    if (slave > 0 && slave <= ctx_.slavecount) {
        return ctx_.slavelist[slave].inputs;
    }
    return nullptr;
}

void EtherCATComm::add_time_ns(ec_timet* ts, int64 addtime) {
    ec_timet addts;
    addts.tv_nsec = addtime % NSEC_PER_SEC;
    addts.tv_sec = (addtime - addts.tv_nsec) / NSEC_PER_SEC;
    osal_timespecadd(ts, &addts, ts);
}

void EtherCATComm::ec_sync(int64 reftime, int64 cycletime, int64* offsettime) {
    static int64 integral = 0;
    int64 delta = (reftime - 500000) % cycletime;
    if (delta > (cycletime / 2)) {
        delta -= cycletime;
    }
    integral += delta;
    *offsettime = (int64)((delta * 0.01f) + (integral * 0.00002f));
}

void EtherCATComm::Ecatthread() {
    ec_timet ts;
    int ht;
    static int64_t toff = 0;

    while (!mappingdone_) {
        osal_usleep(500);
    }

    osal_get_monotonic_time(&ts);
    ht = (ts.tv_nsec / 1000000) + 1;
    ts.tv_nsec = ht * 1000000;

    {
        std::lock_guard<std::mutex> lock(rt_context_mutex_);
        memcpy(ctx_.slavelist[1].outputs, rxpdo_buffer_, sizeof(rxpdo_buffer_));
        ecx_send_processdata(&ctx_);
    }

    while (threads_started_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        if (dorun_ > 0) {
            cycle_++;

            // 接收过程数据
            uint8_t* pdo_copy = nullptr;
            size_t pdo_size = 0;

            {
                std::lock_guard<std::mutex> lock(rt_context_mutex_);
                wkc_ = ecx_receive_processdata(&ctx_, EC_TIMEOUTRET);

                if (wkc_ >= expectedWKC_ && inOP_) {
                    uint8_t* inputs = ctx_.slavelist[1].inputs;
                    pdo_size = ctx_.slavelist[1].Ibytes;

                    if (pdo_size > 0) {
                        pdo_copy = new uint8_t[pdo_size];
                        memcpy(pdo_copy, inputs, pdo_size);
                    }
                }
            }

            // 在锁外调用回调
            if (pdo_copy != nullptr) {
                ParseAndNotify(pdo_copy, pdo_size);
                delete[] pdo_copy;
            }

            if (wkc_ != expectedWKC_) {
                dowkccheck_++;
            } else {
                dowkccheck_ = 0;
            }

            {
                std::lock_guard<std::mutex> lock(rt_context_mutex_);
                memcpy(ctx_.slavelist[1].outputs, rxpdo_buffer_, sizeof(rxpdo_buffer_));
                ecx_send_processdata(&ctx_);
            }
        }
    }
}

void EtherCATComm::Ecatcheck() {
    int slaveix;
    while (threads_started_) {
        osal_usleep(50000);

        if (inOP_ && ((dowkccheck_ > 2) || ctx_.grouplist[currentgroup_].docheckstate)) {
            std::unique_lock<std::mutex> lock(rt_context_mutex_, std::try_to_lock);

            if (!lock.owns_lock()) {
                continue;
            }

            ctx_.grouplist[currentgroup_].docheckstate = FALSE;
            ecx_readstate(&ctx_);
            for (slaveix = 1; slaveix <= ctx_.slavecount; slaveix++) {
                ec_slavet* slave = &ctx_.slavelist[slaveix];

                if ((slave->group == currentgroup_) && (slave->state != EC_STATE_OPERATIONAL)) {
                    ctx_.grouplist[currentgroup_].docheckstate = TRUE;
                    if (slave->state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
                        slave->state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                        ecx_writestate(&ctx_, slaveix);
                    } else if (slave->state == EC_STATE_SAFE_OP) {
                        slave->state = EC_STATE_OPERATIONAL;
                        if (slave->mbxhandlerstate == ECT_MBXH_LOST)
                            slave->mbxhandlerstate = ECT_MBXH_CYCLIC;
                        ecx_writestate(&ctx_, slaveix);
                    } else if (slave->state > EC_STATE_NONE) {
                        if (ecx_reconfig_slave(&ctx_, slaveix, EC_TIMEOUTMON) >= EC_STATE_PRE_OP) {
                            slave->islost = FALSE;
                        }
                    } else if (!slave->islost) {
                        ecx_statecheck(&ctx_, slaveix, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                        if (slave->state == EC_STATE_NONE) {
                            slave->islost = TRUE;
                            threads_started_ = false;
                            is_connected_ = false;
                            slave->mbxhandlerstate = ECT_MBXH_LOST;
                            if (slave->Ibytes) {
                                memset(slave->inputs, 0x00, slave->Ibytes);
                            }
                        }
                    }
                }
                if (slave->islost) {
                    if (slave->state <= EC_STATE_INIT) {
                        if (ecx_recover_slave(&ctx_, slaveix, EC_TIMEOUTMON)) {
                            slave->islost = FALSE;
                            threads_started_ = true;
                            is_connected_ = true;
                        }
                    } else {
                        slave->islost = FALSE;
                        threads_started_ = true;
                        is_connected_ = true;
                    }
                }
            }
            dowkccheck_ = 0;
        }
    }
}

bool EtherCATComm::InputBin(const char* fname, int* length) {
    FILE* fp = nullptr;
    int cc = 0, c;

#ifdef _WIN32
    errno_t err = fopen_s(&fp, fname, "rb");
    if (err != 0 || fp == NULL) return false;
#else
    fp = fopen(fname, "rb");
    if (fp == NULL) return false;
#endif

    while (((c = fgetc(fp)) != EOF) && (cc < static_cast<int>(kFirmwareBufferSize))) {
        file_buffer_[cc++] = (uint8_t)c;
    }

    *length = cc;
    fclose(fp);
    return true;
}

int EtherCATComm::BootUpdate(const std::string& ifname, uint16_t slave,
                             const std::string& file_path,
                             std::function<void(int)> progressCallback) {
    (void)ifname;

    // 预检查：写入 0x5A 到 0x2005:0x01
    std::uint8_t command = 0x5A;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    std::uint8_t result = 0xFF;
    int ret = SDOWrite(1, 0x2005, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        ret = SDORead(1, 0x2005, 0x02, &size, &state, EC_TIMEOUTRXM);
        if (ret > 0 && state == 0) {
            ret = SDORead(1, 0x2005, 0x03, &size, &result, EC_TIMEOUTRXM);
        }
    }
    if (result != 1) {
        return 0;
    }

    if (slave <= 0 || slave > ctx_.slavecount) {
        return -1;
    }
    int filesize = 0;
    inOP_ = 0;
    dorun_ = 0;
    StopThreads();
    progress_callback_ = progressCallback;
    foe_instance_ = this;
    ctx_.slavelist[0].state = EC_STATE_SAFE_OP;
    ecx_writestate(&ctx_, 0);
    ecx_statecheck(&ctx_, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

    ctx_.slavelist[0].state = EC_STATE_PRE_OP;
    ecx_writestate(&ctx_, 0);
    ecx_statecheck(&ctx_, 0, EC_STATE_PRE_OP, EC_TIMEOUTSTATE);

    ctx_.slavelist[0].state = EC_STATE_INIT;
    ecx_writestate(&ctx_, 0);
    ecx_statecheck(&ctx_, 0, EC_STATE_INIT, EC_TIMEOUTSTATE);

    ctx_.slavelist[slave].mbxhandlerstate = 0;

    ecx_FOEdefinehook(&ctx_, reinterpret_cast<void*>(EtherCATComm::FoeProgressHook));
    ctx_.slavelist[slave].state = EC_STATE_BOOT;
    ecx_writestate(&ctx_, slave);

    if (ecx_statecheck(&ctx_, slave, EC_STATE_BOOT, EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT) {
        if (InputBin(file_path.c_str(), &filesize)) {
            char file_name[] = "ECATFW__firmware";
            int update_result =
                ecx_FOEwrite(&ctx_, slave, file_name, 0, filesize, &file_buffer_, EC_TIMEOUTSTATE);

            LOG_DEBUG("Releasing device lock after firmware update (result: " << update_result << ")");
            device_lock_.Release();

            foe_instance_ = nullptr;
            return update_result;
        } else {
            LOG_DEBUG("Releasing device lock after firmware file read failure");
            device_lock_.Release();
            foe_instance_ = nullptr;
            return -2;
        }
    } else {
        LOG_DEBUG("Releasing device lock after BOOT state transition failure");
        device_lock_.Release();
        foe_instance_ = nullptr;
        return -1;
    }
    return 0;
}

void EtherCATComm::FoeProgressHook(uint16 slave, int32 packetnumber, int32 totalsize) {
    static int32 last_packet = -1;

    if (packetnumber != last_packet) {
        last_packet = packetnumber;

        if (foe_instance_ && slave > 0 && slave <= foe_instance_->ctx_.slavecount) {
            int maxdata = foe_instance_->ctx_.slavelist[slave].mbx_l - 12;
            if (maxdata <= 0) return;

            int sent_data = packetnumber * maxdata;
            if (sent_data > totalsize) {
                sent_data = totalsize;
            }

            int percentage = 0;
            if (totalsize > 0) {
                percentage = (sent_data * 100 / totalsize);
            }

            fflush(stdout);
            if (progress_callback_) {
                progress_callback_(percentage);
            }
        }
    }
}

// ===== IComm 业务接口实现 =====

void EtherCATComm::SetJointsCallback(JointsCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    joints_callback_ = cb;
}

void EtherCATComm::SetHandStateCallback(HandStateCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    hand_state_callback_ = cb;
}

void EtherCATComm::SetTactileDataCallback(TactileDataCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    tactile_callback_ = cb;
}

DeviceInfo EtherCATComm::GetDeviceInfo() {
    DeviceInfo info;
    std::uint8_t value[255] = {0};
    int size = sizeof(value);
    int result = -1;

    result = SDORead(1, 0x1008, 0x00, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        info.device_name = std::string(reinterpret_cast<char*>(value));
    }

    memset(value, 0, sizeof(value));
    result = SDORead(1, 0x1009, 0x00, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        info.hardware_version = std::string(reinterpret_cast<char*>(value));
    }

    memset(value, 0, sizeof(value));
    result = SDORead(1, 0x100A, 0x00, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        info.software_version = std::string(reinterpret_cast<char*>(value));
    }

    memset(value, 0, sizeof(value));
    result = SDORead(1, 0x1018, 0x04, &size, &value, EC_TIMEOUTRXM);
    if (result == 1) {
        unsigned int serial_num = 0;
        serial_num |= static_cast<unsigned char>(value[0]);
        serial_num |= static_cast<unsigned char>(value[1]) << 8;
        serial_num |= static_cast<unsigned char>(value[2]) << 16;
        serial_num |= static_cast<unsigned char>(value[3]) << 24;
        info.serial_number = serial_num;
    }

    std::uint8_t motor_ver[3] = {0};
    int motor_size = sizeof(std::uint8_t);
    int motor_result[3] = {0};
    motor_result[0] = SDORead(1, 0x2007, 0x01, &motor_size, &motor_ver[0], EC_TIMEOUTRXM);
    motor_result[1] = SDORead(1, 0x2007, 0x02, &motor_size, &motor_ver[1], EC_TIMEOUTRXM);
    motor_result[2] = SDORead(1, 0x2007, 0x03, &motor_size, &motor_ver[2], EC_TIMEOUTRXM);
    if (motor_result[0] == 1 && motor_result[1] == 1 && motor_result[2] == 1) {
        info.motor_driver_version =
            std::to_string(motor_ver[0]) + "." +
            std::to_string(motor_ver[1]) + "." +
            std::to_string(motor_ver[2]);
    }

    return info;
}

HandType EtherCATComm::GetHandType() {
    std::uint8_t value = 0;
    int size = sizeof(value);
    int result = SDORead(1, 0x2001, 0x00, &size, &value, EC_TIMEOUTRXM);

    if (result == 1) {
        switch (value) {
            case 1: return HandType::LEFT;
            case 2: return HandType::RIGHT;
            default: return HandType::NONE;
        }
    }
    return HandType::NONE;
}

bool EtherCATComm::MoveJoints(const std::vector<JointCommand>& joints,
                              ControlMode mode) {
    if (!IsConnected()) {
        LOG_ERROR("Cannot move joints: device not connected");
        return false;
    }
    if (joints.empty()) {
        LOG_WARNING("MoveJoints called with empty joint list");
        return false;
    }

    std::vector<uint8_t> buffer(joints.size() * kEthercatJointDataSize + 2, 0);
    size_t offset = 0;

    buffer[offset++] = static_cast<uint8_t>(mode);
    buffer[offset++] = 0;  // stop

    for (const auto& joint : joints) {
        float angle = joint.angle * (static_cast<float>(M_PI) / 180.0f);
        memcpy(buffer.data() + offset, &angle, sizeof(angle));
        offset += sizeof(angle);
        buffer[offset++] = joint.velocity;
        buffer[offset++] = joint.torque;
    }

    LOG_INFO("Sending PDO data");
    SendRxPDO(1, ECT_SDO_RXPDOASSIGN, buffer.size(), buffer.data());
    return true;
}

void EtherCATComm::Stop() {
    LOG_INFO("Sending stop command");
    std::vector<uint8_t> buffer(config_.valid_joints.size() * kEthercatJointDataSize + 2, 0);
    if (buffer.size() > 1) {
        buffer[1] = 1;
    }
    SendRxPDO(1, ECT_SDO_RXPDOASSIGN, buffer.size(), buffer.data());
}

bool EtherCATComm::ClearFault() {
    LOG_INFO("Clearing device fault");

    std::uint8_t command = 0x01;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    std::uint8_t result = 0;
    int ret = -1;
    int times = 100;
    ret = SDOWrite(1, 0x2002, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        while (times--) {
            ret = SDORead(1, 0x2002, 0x02, &size, &state, EC_TIMEOUTRXM);
            if (ret > 0 && state == 0) {
                ret = SDORead(1, 0x2002, 0x03, &size, &result, EC_TIMEOUTRXM);
                return result == 1;
            }
        }
    }
    LOG_ERROR("Clear fault operation timed out");
    return false;
}

bool EtherCATComm::InitJoint() {
    LOG_INFO("Initializing joint positions");

    std::uint8_t command = 0x01;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    std::uint8_t result = 0;
    int ret = -1;
    int times = 100;
    ret = SDOWrite(1, 0x2003, 0x01, size, &command, EC_TIMEOUTRXM);
    if (ret > 0) {
        while (times--) {
            ret = SDORead(1, 0x2003, 0x02, &size, &state, EC_TIMEOUTRXM);
            if (ret > 0 && state == 0) {
                ret = SDORead(1, 0x2003, 0x03, &size, &result, EC_TIMEOUTRXM);
                return result == 1;
            }
        }
    }
    LOG_ERROR("Joint initialization timed out");
    return false;
}

bool EtherCATComm::OpenTactile() {
    std::uint8_t command = 0x01;
    int size = sizeof(std::uint8_t);
    int result = SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    return result > 0;
}

bool EtherCATComm::CloseTactile() {
    std::uint8_t command = 0x02;
    int size = sizeof(std::uint8_t);
    int result = SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    return result > 0;
}

bool EtherCATComm::ZeroTactile() {
    std::uint8_t command = 0x04;
    std::uint8_t state = 0xFF;
    int size = sizeof(std::uint8_t);
    int result = SDOWrite(1, 0x2004, 0x01, size, &command, EC_TIMEOUTRXM);
    result = SDORead(1, 0x2004, 0x03, &size, &state, EC_TIMEOUTRXM);
    return result > 0;
}

// ===== PDO 解析辅助函数 =====
namespace {

Force ParseResultantForce(const uint8_t* data) {
    Force resultant;
    resultant.x = 0.0f;
    resultant.y = 0.0f;
    resultant.z = 0.0f;

    if (data != nullptr) {
        int16_t raw_x = static_cast<int16_t>(data[0] | (data[1] << 8));
        resultant.x = static_cast<float>(static_cast<int8_t>(raw_x & 0xFF)) * 0.1f;

        int16_t raw_y = static_cast<int16_t>(data[2] | (data[3] << 8));
        resultant.y = static_cast<float>(static_cast<int8_t>(raw_y & 0xFF)) * 0.1f;

        uint16_t raw_z = static_cast<uint16_t>(data[4] | (data[5] << 8));
        resultant.z = static_cast<float>(static_cast<uint8_t>(raw_z & 0xFF)) * 0.1f;
    }

    return resultant;
}

std::vector<Force> ParseSampleForces(const uint8_t* data, int sensor_count) {
    std::vector<Force> forces;
    forces.reserve(sensor_count);

    if (data != nullptr && sensor_count > 0) {
        for (int i = 0; i < sensor_count; i++) {
            Force sample_force;
            sample_force.x = static_cast<float>(static_cast<int8_t>(data[i * 3])) * 0.1f;
            sample_force.y = static_cast<float>(static_cast<int8_t>(data[i * 3 + 1])) * 0.1f;
            sample_force.z = static_cast<float>(static_cast<uint8_t>(data[i * 3 + 2])) * 0.1f;
            forces.push_back(sample_force);
        }
    }

    return forces;
}

}  // anonymous namespace

void EtherCATComm::ParseAndNotify(const uint8_t* data, size_t size) {
    std::vector<Joint> parsed_joints;
    HandState parsed_temperature;

    if (data == nullptr || size == 0) {
        LOG_WARNING("Received invalid data: null or empty");
        return;
    }

    size_t offset = 0;
    uint8_t hand_state, hand_error;
    int16_t temperature;

    memcpy(&hand_state, data + offset, sizeof(hand_state));
    offset += sizeof(hand_state);

    memcpy(&hand_error, data + offset, sizeof(hand_error));
    offset += sizeof(hand_error);

    memcpy(&temperature, data + offset, sizeof(temperature));
    offset += sizeof(temperature);

    parsed_temperature.state = static_cast<State>(hand_state);
    parsed_temperature.error = static_cast<ErrorCode>(hand_error);
    parsed_temperature.temperature = temperature;

    parsed_joints.reserve(config_.valid_joints.size());
    for (const auto& joint_id : config_.valid_joints) {
        uint8_t joint_state, joint_error;
        float joint_angle;
        uint8_t joint_velocity, joint_torque;

        memcpy(&joint_state, data + offset, sizeof(joint_state));
        offset += sizeof(joint_state);

        memcpy(&joint_error, data + offset, sizeof(joint_error));
        offset += sizeof(joint_error);

        memcpy(&joint_angle, data + offset, sizeof(joint_angle));
        offset += sizeof(joint_angle);

        memcpy(&joint_velocity, data + offset, sizeof(joint_velocity));
        offset += sizeof(joint_velocity);

        memcpy(&joint_torque, data + offset, sizeof(joint_torque));
        offset += sizeof(joint_torque);

        Joint joint;
        joint.id = joint_id;
        joint.state = static_cast<State>(joint_state);
        joint.error = static_cast<ErrorCode>(joint_error);
        joint.angle = joint_angle * (180.0f / static_cast<float>(M_PI));
        joint.velocity = joint_velocity;
        joint.torque = joint_torque;
        parsed_joints.push_back(joint);
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (joints_callback_) {
        joints_callback_(parsed_joints);
    }
    if (hand_state_callback_) {
        hand_state_callback_(parsed_temperature);
    }

    if (config_.has_tactile && offset < size) {
        TactileData tactile_data;

        if (offset + 2 <= size) {
            tactile_data.sensor_state = data[offset];
            tactile_data.sensor_error = data[offset + 1];
            offset += 2;
        }

        tactile_data.regions.reserve(config_.tactile_regions.size());
        for (size_t i = 0; i < config_.tactile_regions.size(); ++i) {
            const auto& rc = config_.tactile_regions[i];
            RegionTactile region;
            region.region_name = rc.name.c_str();
            region.state = (tactile_data.sensor_state & (1 << i)) != 0;

            if (offset + 6 <= size) {
                region.resultant_force = ParseResultantForce(data + offset);
                offset += 6;
            }

            int sample_size = rc.sensor_count * 3;
            if (rc.sensor_count > 0 && offset + sample_size <= size) {
                region.distributed_forces = ParseSampleForces(data + offset, rc.sensor_count);
                offset += sample_size;
            }

            tactile_data.regions.push_back(std::move(region));
        }

        if (tactile_callback_) {
            tactile_callback_(tactile_data);
        }
    }
}

}  // namespace internal
}  // namespace ghand
