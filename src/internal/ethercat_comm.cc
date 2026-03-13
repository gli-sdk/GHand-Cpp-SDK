#include "ethercat_comm.h"

#include <windows.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#define EC_TIMEOUTMON 500
#define NSEC_PER_SEC 1000000000

namespace xiaoyao {
namespace internal {

OSAL_THREAD_HANDLE
EtherCATComm::threadrt, EtherCATComm::thread1;
int EtherCATComm::expectedWKC;
int EtherCATComm::wkc;
int EtherCATComm::mappingdone = 0;
int EtherCATComm::dorun = 0;
int EtherCATComm::inOP = 0;
int EtherCATComm::dowkccheck = 0;
int EtherCATComm::currentgroup = 0;
int EtherCATComm::cycle = 0;
int64_t EtherCATComm::cycletime = 10000000;
ecx_contextt EtherCATComm::ctx_;
uint8 EtherCATComm::IOmap_[4096] = {0};
bool EtherCATComm::threads_started_ = false;
std::mutex EtherCATComm::context_mutex_;
std::mutex EtherCATComm::rt_context_mutex_;
FileLock EtherCATComm::device_lock_;  // 设备锁静态成员定义
std::function<void(int)> EtherCATComm::state_update_callback_ = nullptr;
std::function<void(int)> EtherCATComm::progress_callback_ = nullptr;
std::function<void(const uint8_t*, size_t)> EtherCATComm::data_callback_ = nullptr;
std::mutex EtherCATComm::data_callback_mutex_;

// static LockFreeQueue<std::pair<std::vector<uint8>, uint16>, 32> pdo_queue_;
static uint8 rxpdo_buffer_[80];
static std::atomic<bool> print_debug_info{false};
EtherCATComm::EtherCATComm() {}

EtherCATComm::~EtherCATComm() {}

void EtherCATComm::StartThreads() {
    if (!threads_started_) {
        dorun = 1;
        osal_thread_create_rt(&threadrt, 128000, reinterpret_cast<void*>(Ecatthread), NULL);
        osal_thread_create(&thread1, 128000, reinterpret_cast<void*>(Ecatcheck), NULL);

        // // 设置进程优先级类别
        // // 提高整个进程的优先级，确保窗口最小化后仍有足够的CPU时间
        // SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

        // // 设置线程优先级
        // // EtherCAT 实时线程需要高优先级以确保实时性能
        // SetThreadPriority(threadrt, THREAD_PRIORITY_TIME_CRITICAL);
        // // 检线程使用稍低优先级，避免影响实时线程
        // SetThreadPriority(thread1, THREAD_PRIORITY_ABOVE_NORMAL);

        threads_started_ = true;
    }
}

void EtherCATComm::StopThreads() {
    if (threads_started_) {
        threads_started_ = false;
        dorun = 0;
        // const int max_wait_ms = 2000;
        // int wait_count = 0;
        // while (threads_started_ && wait_count < max_wait_ms) {
        osal_usleep(10000);
        // wait_count += 10;
        //}
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

int EtherCATComm::Connect(std::string device_name) {
    std::lock_guard<std::mutex> lock(context_mutex_);

    // 步骤1：尝试获取设备锁（防止多进程同时访问）
    std::string lock_path = GetAdapterLockPath(device_name);
    if (!device_lock_.Acquire(lock_path)) {
        std::cerr << "Failed to connect: adapter " << device_name
                  << " is already locked by another process" << std::endl;
        return -1;  // 设备已被占用
    }

    memset(rxpdo_buffer_, 0, sizeof(rxpdo_buffer_));
    rxpdo_buffer_[1] = {0x01};

    ResetContext();
    if (ecx_init(&ctx_, device_name.c_str()) <= 0) {
        device_lock_.Release();
        return -2;
    }

    int config_result = ecx_config_init(&ctx_);
    if (config_result <= 0 || ctx_.slavecount <= 0) {
        device_lock_.Release();
        return -3;
    }

    ec_groupt* group = &ctx_.grouplist[0];
    int map_result = ecx_config_map_group(&ctx_, &IOmap_, 0);
    if (map_result <= 0) {
        device_lock_.Release();
        return -4;
    }

    mappingdone = 1;
    dorun = 1;
    expectedWKC = (group->outputsWKC * 2) + group->inputsWKC;

    // ❌ 禁用DC分布式时钟同步
    // 原因：单从站系统不需要DC同步，禁用后可避免窗口最小化时的周期波动
    // ecx_configdc(&ctx_);

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

    inOP = 1;

    return 0;
}

int EtherCATComm::Disconnect() {
    StopThreads();
    std::lock_guard<std::mutex> lock(context_mutex_);
    inOP = 0;
    dorun = 0;

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

    mappingdone = 0;
    expectedWKC = 0;
    wkc = 0;
    cycle = 0;
    currentgroup = 0;
    dorun = 0;
    inOP = 0;
    dowkccheck = 0;
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

    // 直接使用实时上下文进行 SDO 写操作
    // 使用互斥锁保护
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
    // SendRxPDO 使用无锁队列，不需要加锁
    // 使用 rt_context_mutex_ 验证参数
    // std::lock_guard<std::mutex> lock(rt_context_mutex_);

    if (!data || data_size <= 0 || slave <= 0 || slave > ctx_.slavecount) {
        return -1;
    }

    if (ctx_.slavelist[slave].Obytes < data_size || data_size > sizeof(rxpdo_buffer_)) {
        return -1;
    }

    // if (data_size <= ctx_.slavelist[slave].Obytes) {
    //     memcpy(ctx_.slavelist[slave].outputs, data, data_size);
    // }
    memcpy(rxpdo_buffer_, data, data_size);

    // std::vector<uint8> data_copy(data, data + data_size);
    // std::pair<std::vector<uint8>, uint16> pdo_item(data_copy, slave);
    // if (data[1] == 1) {
    //     // 清除 pdo_queue_ 队列
    //     std::pair<std::vector<uint8>, uint16> temp_item;
    //     while (pdo_queue_.Pop(temp_item)) {
    //         // 继续弹出元素直到队列为空
    //     }
    // }

    // if (!pdo_queue_.Push(pdo_item)) {
    //     return -1;
    // }
    return 1;
}

uint8_t* EtherCATComm::ReadTxPDO(uint16 slave) {
    // 使用 rt_context_mutex_ 读取，避免与实时线程冲突
    // std::lock_guard<std::mutex> lock(rt_context_mutex_);

    if (slave > 0 && slave <= ctx_.slavecount) {
        return ctx_.slavelist[slave].inputs;
    }
    return nullptr;
}
OSAL_THREAD_FUNC_RT EtherCATComm::Ecatthread(void) {
    ec_timet ts;
    int ht;
    static int64_t toff = 0;

    while (!mappingdone) {
        osal_usleep(500);
    }

    osal_get_monotonic_time(&ts);
    ht = (ts.tv_nsec / 1000000) + 1;
    ts.tv_nsec = ht * 1000000;

    // 初始化 - 需要锁保护
    {
        std::lock_guard<std::mutex> lock(rt_context_mutex_);
        memcpy(ctx_.slavelist[1].outputs, rxpdo_buffer_, sizeof(rxpdo_buffer_));
        ecx_send_processdata(&ctx_);
    }

    while (threads_started_) {
        // add_time_ns(&ts, cycletime + toff);
        // osal_monotonic_sleep(&ts);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        if (dorun > 0) {
            cycle++;

            // 接收过程数据 - 需要锁保护
            uint8_t* pdo_copy = nullptr;
            size_t pdo_size = 0;

            {
                std::lock_guard<std::mutex> lock(rt_context_mutex_);
                wkc = ecx_receive_processdata(&ctx_, EC_TIMEOUTRET);

                // 快速复制PDO数据用于回调
                if (wkc >= expectedWKC && data_callback_) {
                    // 获取slave 1的输入数据（PDO）
                    uint8_t* inputs = ctx_.slavelist[1].inputs;
                    pdo_size = ctx_.slavelist[1].Ibytes;

                    // 创建数据副本（避免在锁中调用回调）
                    if (pdo_size > 0) {
                        pdo_copy = new uint8_t[pdo_size];
                        memcpy(pdo_copy, inputs, pdo_size);
                    }
                }
            }

            // 在锁外调用回调（避免阻塞实时线程）
            if (pdo_copy != nullptr && data_callback_) {
                NotifyDataReceived(pdo_copy, pdo_size);
                delete[] pdo_copy;
            }

            if (wkc != expectedWKC) {
                dowkccheck++;

            } else {
                dowkccheck = 0;
            }

            {
                // 使用互斥锁保护发送操作，防止与接收/SDO冲突
                std::lock_guard<std::mutex> lock(rt_context_mutex_);
                memcpy(ctx_.slavelist[1].outputs, rxpdo_buffer_, sizeof(rxpdo_buffer_));
                ecx_send_processdata(&ctx_);
            }
        }
    }
}

OSAL_THREAD_FUNC EtherCATComm::Ecatcheck(void) {
    int slaveix;
    while (threads_started_) {
        osal_usleep(50000);

        // 使用 try_lock 避免阻塞实时线程
        if (inOP && ((dowkccheck > 2) || ctx_.grouplist[currentgroup].docheckstate)) {
            std::unique_lock<std::mutex> lock(rt_context_mutex_, std::try_to_lock);

            if (!lock.owns_lock()) {
                // 实时线程正在运行，跳过本次检查
                continue;
            }

            ctx_.grouplist[currentgroup].docheckstate = FALSE;
            ecx_readstate(&ctx_);
            for (slaveix = 1; slaveix <= ctx_.slavecount; slaveix++) {
                ec_slavet* slave = &ctx_.slavelist[slaveix];

                if ((slave->group == currentgroup) && (slave->state != EC_STATE_OPERATIONAL)) {
                    ctx_.grouplist[currentgroup].docheckstate = TRUE;
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
                            threads_started_ = false;  // 异常防止崩溃
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
                            threads_started_ = TRUE;
                        }
                    } else {
                        slave->islost = FALSE;
                        threads_started_ = TRUE;
                    }
                }
            }
            dowkccheck = 0;
        }
    }
}
}  // namespace internal
}  // namespace xiaoyao