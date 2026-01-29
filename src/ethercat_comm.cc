#include "xiaoyao/ethercat_comm.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#define EC_TIMEOUTMON 500
#define NSEC_PER_SEC 1000000000

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
int64_t EtherCATComm::cycletime = 50000000;
ecx_contextt EtherCATComm::ctx_;
ecx_contextt EtherCATComm::ctx_shadow_;  // 影子上下文初始化
uint8 EtherCATComm::IOmap_[4096] = {0};
bool EtherCATComm::threads_started_ = false;
std::mutex EtherCATComm::context_mutex_;
std::mutex EtherCATComm::rt_context_mutex_;
std::atomic<bool> EtherCATComm::ctx_dirty_{false};
std::function<void(int)> EtherCATComm::state_update_callback_ = nullptr;
std::function<void(int)> EtherCATComm::progress_callback_ = nullptr;

static LockFreeQueue<std::pair<std::vector<uint8>, uint16>, 32> pdo_queue_;
static std::atomic<bool> print_debug_info{false};
LogFile* EtherCATComm::log_file_ = nullptr;
EtherCATComm::EtherCATComm() {}

EtherCATComm::~EtherCATComm() {}

void EtherCATComm::StartThreads() {
    if (!threads_started_) {
        dorun = 1;
        osal_thread_create_rt(&threadrt, 128000, reinterpret_cast<void*>(Ecatthread), NULL);
        osal_thread_create(&thread1, 128000, reinterpret_cast<void*>(Ecatcheck), NULL);
        threads_started_ = true;
        // auto now = std::chrono::system_clock::now();
        // auto time_t = std::chrono::system_clock::to_time_t(now);
        // std::stringstream ss;
        // ss << "ethercatlog/log_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
        //    << ".txt";
        // log_file_ = new LogFile(ss.str());
        // log_file_->WriteLog("threads_started_");
    }
}

void EtherCATComm::StopThreads() {
    if (threads_started_) {
        threads_started_ = false;
        dorun = 0;
        const int max_wait_ms = 2000;
        int wait_count = 0;
        while (threads_started_ && wait_count < max_wait_ms) {
            osal_usleep(10000);
            wait_count += 10;
        }
        if (threads_started_) {
            std::cerr << "Warning: Threads did not stop gracefully within timeout" << std::endl;
        }
    }
}

map<string, string> EtherCATComm::ListAdapters() {
    ec_adaptert* adapter = nullptr;
    ec_adaptert* head = nullptr;
    map<string, string> adapter_names;
    adapter_names.clear();

    head = adapter = ec_find_adapters();
    while (adapter != nullptr) {
        if ((std::string(adapter->name).find("Loopback") != std::string::npos)) {
            adapter = adapter->next;
            continue;
        }
        adapter_names.emplace(string(adapter->name), string(adapter->desc));
        adapter = adapter->next;
    }
    ec_free_adapters(head);
    return adapter_names;
}

int EtherCATComm::Connect(std::string device_name) {
    std::lock_guard<std::mutex> lock(context_mutex_);

    ResetContext();

    // 初始化实时上下文
    if (ecx_init(&ctx_, device_name.c_str()) <= 0) {
        return -1;
    }

    int config_result = ecx_config_init(&ctx_);
    if (config_result <= 0 || ctx_.slavecount <= 0) {
        ecx_close(&ctx_);
        return -2;
    }

    ec_groupt* group = &ctx_.grouplist[0];
    int map_result = ecx_config_map_group(&ctx_, &IOmap_, 0);
    if (map_result <= 0) {
        ecx_close(&ctx_);
        return -3;
    }

    mappingdone = 1;
    dorun = 1;
    expectedWKC = (group->outputsWKC * 2) + group->inputsWKC;

    ecx_configdc(&ctx_);

    for (int si = 1; si <= ctx_.slavecount; si++) {
        ec_slavet* slave = &ctx_.slavelist[si];
        if (slave->CoEdetails > 0) {
            ecx_slavembxcyclic(&ctx_, si);
        }
    }

    uint16_t safe_op_state = ecx_statecheck(&ctx_, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

    if (safe_op_state != EC_STATE_SAFE_OP) {
        ecx_close(&ctx_);
        return -4;
    }

    ctx_.slavelist[0].state = EC_STATE_OPERATIONAL;
    ecx_writestate(&ctx_, 0);

    StartThreads();

    uint16_t op_state = ecx_statecheck(&ctx_, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
    if (op_state != EC_STATE_OPERATIONAL) {
        ecx_close(&ctx_);
        return -5;
    }

    inOP = 1;

    return 0;
}

int EtherCATComm::Disconnect() {
    StopThreads();
    std::lock_guard<std::mutex> lock(context_mutex_);
    inOP = 0;
    dorun = 0;

    if (ctx_.slavecount > 0) {
        if (ctx_.slavelist[1].state == EC_STATE_OPERATIONAL) {
            ctx_.slavelist[0].state = EC_STATE_SAFE_OP;
            ecx_writestate(&ctx_, 0);
            ecx_statecheck(&ctx_, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);
        }

        if (ctx_.slavelist[1].state == EC_STATE_SAFE_OP ||
            ctx_.slavelist[1].state == EC_STATE_PRE_OP) {
            ctx_.slavelist[0].state = EC_STATE_INIT;
            ecx_writestate(&ctx_, 0);
        }
    }

    // 关闭实时上下文
    ecx_close(&ctx_);
    // 清空 PDO 队列
    std::pair<std::vector<uint8>, uint16> temp_item;
    while (pdo_queue_.Pop(temp_item)) {
        // 继续弹出元素直到队列为空
    }
    return 0;
}

void EtherCATComm::ResetContext() {
    memset(&ctx_, 0, sizeof(ecx_contextt));
    memset(&ctx_shadow_, 0, sizeof(ecx_contextt));
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

    // while (retries-- > 0) {
    // 只在 SDO 调用时持有锁，允许实时线程在重试期间运行
    std::lock_guard<std::mutex> lock(rt_context_mutex_);
    result = ecx_SDOread(&ctx_, slave, index, subindex, FALSE, size, data, timeout);
    //     if (result > 0) {
    //         break;
    //     }

    //     // 释放锁后等待，让实时线程有机会运行
    //     lock.~lock_guard();
    //     osal_usleep(10000);
    // }

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

    // while (retries-- > 0) {
    result = ecx_SDOwrite(&ctx_, slave, index, subindex, FALSE, size, data, timeout);
    //     if (result > 0) {
    //         break;
    //     }
    //     osal_usleep(10000);
    // }

    return result;
}

int EtherCATComm::SendRxPDO(uint16 slave, uint16 pdo_index, uint32 data_size, uint8* data) {
    // SendRxPDO 使用无锁队列，不需要加锁
    // 使用 rt_context_mutex_ 验证参数
    std::lock_guard<std::mutex> lock(rt_context_mutex_);

    if (!data || data_size <= 0 || slave <= 0 || slave > ctx_.slavecount) {
        return -1;
    }

    if (ctx_.slavelist[slave].Obytes < data_size) {
        return -1;
    }

    std::vector<uint8> data_copy(data, data + data_size);
    std::pair<std::vector<uint8>, uint16> pdo_item(data_copy, slave);
    if (data[1] == 1) {
        // 清除 pdo_queue_ 队列
        std::pair<std::vector<uint8>, uint16> temp_item;
        while (pdo_queue_.Pop(temp_item)) {
            // 继续弹出元素直到队列为空
        }
    }

    if (!pdo_queue_.Push(pdo_item)) {
        return -1;
    }
    return wkc;
}

uint8_t* EtherCATComm::ReadTxPDO(uint16 slave) {
    // 使用 rt_context_mutex_ 读取，避免与实时线程冲突
    std::lock_guard<std::mutex> lock(rt_context_mutex_);

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

void EtherCATComm::ProcessPendingPDOs() {
    std::pair<std::vector<uint8>, uint16> pdo_item;
    while (pdo_queue_.Pop(pdo_item)) {
        const auto& data = pdo_item.first;
        uint16 slave = pdo_item.second;

        // 保护实时上下文的访问
        std::lock_guard<std::mutex> lock(rt_context_mutex_);

        if (data.size() <= ctx_.slavelist[slave].Obytes) {
            memcpy(ctx_.slavelist[slave].outputs, data.data(), data.size());
        }
    }
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
        char buffer[80] = {0};
        buffer[1] = 1;
        memcpy(ctx_.slavelist[1].outputs, buffer, 80);
        ecx_send_processdata(&ctx_);
    }

    while (threads_started_) {
        add_time_ns(&ts, cycletime + toff);
        osal_monotonic_sleep(&ts);

        if (dorun > 0) {
            cycle++;

            // 接收过程数据 - 需要锁保护
            {
                std::lock_guard<std::mutex> lock(rt_context_mutex_);
                wkc = ecx_receive_processdata(&ctx_, EC_TIMEOUTRET);

                if (ctx_.slavelist[0].hasdc && (wkc > 0)) {
                    ec_sync(ctx_.DCtime, cycletime, &toff);
                }

                ecx_mbxhandler(&ctx_, 0, 4);
            }

            osal_usleep(500);
            if (wkc != expectedWKC) {
                dowkccheck++;
            } else {
                dowkccheck = 0;
            }

            ProcessPendingPDOs();

            // 发送过程数据 - 需要锁保护
            {
                std::lock_guard<std::mutex> lock(rt_context_mutex_);
                ecx_send_processdata(&ctx_);
            }

            // 标记影子上下文需要更新
            ctx_dirty_.store(true, std::memory_order_release);
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

                NotifySlaveState(slaveix, slave->state);

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
                    NotifySlaveState(slaveix, slave->state);
                }
            }
            if (!ctx_.grouplist[currentgroup].docheckstate) {
                NotifySlaveState(1, 8);
            }
            dowkccheck = 0;
        }
    }
}

bool EtherCATComm::InputBin(char* fname, int* length) {
    FILE* fp = nullptr;
    errno_t err;

    int cc = 0, c;

    err = fopen_s(&fp, fname, "rb");
    if (err != 0 || fp == NULL) return false;

    while (((c = fgetc(fp)) != EOF) && (cc < FWBUFSIZE)) {
        file_buffer[cc++] = (uint8)c;
    }

    *length = cc;
    fclose(fp);
    return true;
}
int EtherCATComm::BootUpdate(char* ifname, uint16_t slave, char* file_path,
                             std::function<void(int)> progressCallback) {
    // std::lock_guard<std::mutex> lock(context_mutex_);

    if (slave <= 0 || slave > ctx_.slavecount) {
        return -1;
    }
    int filesize = 0;
    inOP = 0;
    dorun = 0;
    StopThreads();
    progress_callback_ = progressCallback;
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
        NotifySlaveState(1, 3);

        if (InputBin(file_path, &filesize)) {
            char file_name[] = "ECATFW__firmware";
            int update_result =
                ecx_FOEwrite(&ctx_, slave, file_name, 0, filesize, &file_buffer, EC_TIMEOUTSTATE);
            return update_result;
        } else {
            return -2;
        }
    } else {
        return -1;
    }
    return 0;
}

void EtherCATComm::FoeProgressHook(uint16 slave, int32 packetnumber, int32 totalsize) {
    static int32 last_packet = -1;

    if (packetnumber != last_packet) {
        last_packet = packetnumber;

        // 添加边界检查
        if (slave > 0 && slave <= ctx_.slavecount) {
            int maxdata = ctx_.slavelist[slave].mbx_l - 12;
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
