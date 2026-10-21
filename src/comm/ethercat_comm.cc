// SPDX-FileCopyrightText: 2025-2026 GLITech
// SPDX-License-Identifier: GPL-3.0-only

#include "ethercat_comm.h"

#ifdef _WIN32
#pragma warning(disable : 4005)
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <chrono>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include <soem/soem.h>

#include "ghand/logging.h"
#include "logging_macros.h"
#include "modbus_codec.h"
#include "product_config_loader.h"

constexpr int kEcTimeoutMon = 500;
constexpr int kSdoWriteSettleMs = 20;
constexpr int kFirmwareResetReadyPollMs = 500;
constexpr int kFirmwareResetReadyTimeoutMs = 5 * 60 * 1000;
constexpr int kFirmwareResetDelayMs = 1000;
constexpr int kFirmwareQueryPollMs = 500;
constexpr int kFirmwareQueryTimeoutMs = 10 * 1000;

namespace ghand {
namespace internal {

// EtherCAT SDO object dictionary indices
constexpr uint16_t kSdoDeviceName = 0x1008;
constexpr uint16_t kSdoHandType = 0x2001;
constexpr uint16_t kSdoFaultClear = 0x2002;
constexpr uint16_t kSdoTactileControl = 0x2004;
constexpr uint16_t kSdoFirmwareUpdate = 0x2005;
constexpr uint16_t kSdoFirmwareInfo = 0x2007;
constexpr uint16_t kSdoSerialNumber = 0x2009;

// SDO subindices

// RPDO joint: mode, stop and five signed 16-bit values.
constexpr size_t kEthercatJointDataSize = 12;

const std::array<JointId, 13> kStallToleranceJoints = {{
    JointId::THUMB_MCP, JointId::THUMB_TMC_FE, JointId::THUMB_TMC_AA,
    JointId::THUMB_TMC_PS, JointId::FF_PIP, JointId::FF_MCP, JointId::FF_MCP_AA,
    JointId::MF_PIP, JointId::MF_MCP, JointId::RF_PIP, JointId::RF_MCP,
    JointId::LF_PIP, JointId::LF_MCP}};

namespace {

struct FileCloser {
  void operator()(FILE* fp) const {
    if (fp != nullptr) {
      std::fclose(fp);
    }
  }
};

using FilePtr = std::unique_ptr<FILE, FileCloser>;

std::string ByteArrayString(const std::uint8_t* data, size_t size) {
  std::string result;
  for (size_t i = 0; i < size && data[i] != 0; ++i) {
    result.push_back(static_cast<char>(data[i]));
  }
  return result;
}

std::vector<JointId> ControlledJoints(const ProductConfig& config) {
  std::vector<JointId> joints;
  for (JointId joint_id : config.valid_joints) {
    if (config.joint_limits.find(joint_id) != config.joint_limits.end()) {
      joints.push_back(joint_id);
    }
  }
  return joints;
}

size_t EthercatRxpdoSize(const ProductConfig& config) {
  return config.joint_limits.size() * kEthercatJointDataSize + 2;
}

size_t EthercatTxpdoSize(const ProductConfig& config, bool compat_thumb = false) {
  size_t size = 28 + config.valid_joints.size() * 10;
  if (config.has_tactile) {
    size += 2;
    for (size_t i = 0; i < config.tactile_regions.size(); ++i) {
      int count = config.tactile_regions[i].sensor_count;
      if (compat_thumb && i == 0 && count > 28) count = 28;
      size += 6 + count * 3;
    }
  }
  return size;
}

}  // namespace

struct EtherCATComm::SoemState {
  ecx_contextt ctx{};
  uint8_t io_map[4096] = {0};
  OSAL_THREAD_HANDLE threadrt{};
  OSAL_THREAD_HANDLE thread1{};
};

// Static member definitions
std::function<void(int)> EtherCATComm::progress_callback_ = nullptr;
EtherCATComm* EtherCATComm::foe_instance_ = nullptr;
static std::atomic<bool> print_debug_info{false};

EtherCATComm::EtherCATComm(const ProductConfig& config)
    : soem_(std::unique_ptr<SoemState>(new SoemState())), config_(config) {
  rxpdo_size_ = EthercatRxpdoSize(config_);
}

EtherCATComm::~EtherCATComm() {
  StopThreads();
  Disconnect();
}

// Static thread wrapper functions

void EtherCATComm::EcatthreadWrapper(void* arg) {
  auto* self = static_cast<EtherCATComm*>(arg);
  self->Ecatthread();
}

void EtherCATComm::EcatcheckWrapper(void* arg) {
  auto* self = static_cast<EtherCATComm*>(arg);
  self->Ecatcheck();
}

void EtherCATComm::StartThreads() {
  if (!threads_started_) {
    dorun_ = 1;
    osal_thread_create_rt(&soem_->threadrt, 128000,
                          reinterpret_cast<void*>(EcatthreadWrapper), this);
    osal_thread_create(&soem_->thread1, 128000,
                       reinterpret_cast<void*>(EcatcheckWrapper), this);

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
    adapter_names.emplace(std::string(adapter->name),
                          std::string(adapter->desc));
    adapter = adapter->next;
  }
  ec_free_adapters(head);
  return adapter_names;
}

bool EtherCATComm::SetSlaveId(uint16_t slave, uint8_t slave_id) {
  (void)slave_id;
  GHAND_LOG_WARNING("SetSlaveId is not supported on EtherCAT");
  return false;
}

void EtherCATComm::UpdateConfig(const ProductConfig& config) {
  std::lock_guard<std::mutex> lock(command_mutex_);
  if (config_.model == config.model && config_.valid_joints == config.valid_joints) return;
  config_ = config;
  joint_commands_.clear();
  hand_modes_.clear();
}

bool EtherCATComm::SetBaudrateConfig(uint16_t slave, uint8_t baudrate_gear) {
  (void)baudrate_gear;
  GHAND_LOG_WARNING("SetBaudrateConfig is not supported on EtherCAT");
  return false;
}

Diagnostics EtherCATComm::GetDiagnostics() const {
  Diagnostics diagnostics;
  diagnostics.connection.connected = IsConnected();
  diagnostics.connection.comm_type = CommType::ETHERCAT;
  diagnostics.transport.tx_packets = tx_packets_.load();
  diagnostics.transport.rx_packets = rx_packets_.load();
  diagnostics.transport.tx_errors = tx_errors_.load();
  diagnostics.transport.rx_errors = rx_errors_.load();
  diagnostics.transport.reconnects = reconnects_.load();
  diagnostics.subscription.received_frames = received_frames_.load();
  diagnostics.subscription.dispatched_frames = dispatched_frames_.load();
  diagnostics.subscription.callback_errors = callback_errors_.load();
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    diagnostics.subscription.active_subscriptions =
        (joints_callback_ ? 1 : 0) + (hand_state_callback_ ? 1 : 0) +
        (tactile_callback_ ? 1 : 0);
  }
  return diagnostics;
}

int EtherCATComm::Connect(const std::string& device_name, uint8_t slave_id,
                          uint8_t baudrate_gear) {
  (void)baudrate_gear;
  GHAND_LOG_INFO("EtherCAT connecting to: " << device_name);

  std::lock_guard<std::mutex> lock(context_mutex_);

  // Step 1: try to acquire device lock (prevent multi-process access)
  std::string lock_path = GetAdapterLockPath(device_name);
  if (!device_lock_.Acquire(lock_path)) {
    GHAND_LOG_ERROR("Adapter " << device_name
                         << " is already locked by another process");
    return -1;  // device already in use
  }

  memset(rxpdo_buffer_, 0, sizeof(rxpdo_buffer_));
  rxpdo_size_ = EthercatRxpdoSize(config_);
  joint_commands_.clear(); hand_modes_.clear(); hands_.clear();

  ResetContext();
  if (ecx_init(&soem_->ctx, device_name.c_str()) <= 0) {
    GHAND_LOG_ERROR("Unable to initialize EtherCAT adapter: " << device_name);
    device_lock_.Release();
    return -2;
  }

  int config_result = ecx_config_init(&soem_->ctx);
  if (config_result <= 0 || soem_->ctx.slavecount <= 0) {
    GHAND_LOG_ERROR("No devices found on adapter: " << device_name);
    device_lock_.Release();
    return -3;
  }

  ec_groupt* group = &soem_->ctx.grouplist[0];
  int map_result = ecx_config_map_group(&soem_->ctx, &soem_->io_map, 0);
  if (map_result <= 0) {
    device_lock_.Release();
    return -4;
  }

  uint16_t safe_op_state =
      ecx_statecheck(&soem_->ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

  if (safe_op_state != EC_STATE_SAFE_OP) {
    device_lock_.Release();
    return -5;
  }

  // The hand firmware serves SDO reads only from SAFE_OP onwards.
  for (uint16_t si = 1; si <= soem_->ctx.slavecount; ++si) {
    HandType type = GetHandType(si);
    if (type != HandType::LEFT && type != HandType::RIGHT) continue;
    ConnectedHand hand; hand.slave_index = si; hand.hand_type = type; hand.device_info = GetDeviceInfo(si);
    hands_.push_back(hand);
  }
  if (hands_.empty()) {
    GHAND_LOG_ERROR("No controllable hands found on adapter: " << device_name);
    device_lock_.Release();
    return -3;
  }
  if (config_.valid_joints.empty()) {
    config_ = FindConfigByName(hands_.front().device_info.device_name);
    if (config_.valid_joints.empty()) {
      GHAND_LOG_ERROR("Unknown product \"" << hands_.front().device_info.device_name
                      << "\" reported by slave " << hands_.front().slave_index);
      device_lock_.Release();
      return -3;
    }
  }
  rxpdo_size_ = EthercatRxpdoSize(config_);
  rxpdo_buffers_.assign(static_cast<size_t>(soem_->ctx.slavecount) + 1,
                        std::vector<uint8_t>(sizeof(rxpdo_buffer_), 0));
  rxpdo_sizes_.assign(static_cast<size_t>(soem_->ctx.slavecount) + 1, 0);
  for (const auto& hand : hands_) rxpdo_sizes_[hand.slave_index] = rxpdo_size_;

  for (const auto& hand : hands_) {
    auto& entry = soem_->ctx.slavelist[hand.slave_index];
    if (entry.Obytes != rxpdo_size_ ||
        (entry.Ibytes != EthercatTxpdoSize(config_) && entry.Ibytes != EthercatTxpdoSize(config_, true))) {
      GHAND_LOG_ERROR("EtherCAT PDO mapping does not match the configured product");
      hands_.clear(); device_lock_.Release(); return -4;
    }
  }

  mappingdone_ = 1;
  dorun_ = 1;
  expectedWKC_ = (group->outputsWKC * 2) + group->inputsWKC;

  ecx_configdc(&soem_->ctx);

  soem_->ctx.slavelist[0].state = EC_STATE_OPERATIONAL;
  ecx_writestate(&soem_->ctx, 0);

  StartThreads();

  uint16_t op_state =
      ecx_statecheck(&soem_->ctx, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
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
  rxpdo_size_ = EthercatRxpdoSize(config_);
  joint_commands_.clear();
  hand_modes_.clear();
  if (soem_->ctx.slavecount > 0) {
    soem_->ctx.slavelist[0].state = EC_STATE_INIT;
    ecx_writestate(&soem_->ctx, 0);
  }

  hands_.clear();
  rxpdo_buffers_.clear(); rxpdo_sizes_.clear();
  // Release device lock
  device_lock_.Release();

  return 0;
}

void EtherCATComm::ResetContext() {
  memset(&soem_->ctx, 0, sizeof(ecx_contextt));
  memset(&soem_->io_map, 0, 4096);

  mappingdone_ = 0;
  expectedWKC_ = 0;
  wkc_ = 0;
  cycle_ = 0;
  currentgroup_ = 0;
  dorun_ = 0;
  inOP_ = 0;
  dowkccheck_ = 0;
}

int EtherCATComm::SDORead(std::uint16_t slave, std::uint16_t index,
                          std::uint8_t subindex, int* size, void* data,
                          int timeout) {
  if (!data || !size) {
    return -1;
  }

  int retries = 3;
  int result = -1;

  while (retries-- > 0) {
    {
      std::lock_guard<std::mutex> lock(rt_context_mutex_);
      result = ecx_SDOread(&soem_->ctx, slave, index, subindex, FALSE, size,
                           data, timeout);
      if (result > 0) break;
    }
    osal_usleep(10000);
  }

  return result;
}

int EtherCATComm::SDOWrite(std::uint16_t slave, std::uint16_t index,
                           std::uint8_t subindex, int size, void* data,
                           int timeout) {
  if (!data || size <= 0) {
    return -1;
  }

  std::lock_guard<std::mutex> lock(rt_context_mutex_);

  int retries = 3;
  int result = -1;

  while (retries-- > 0) {
    result = ecx_SDOwrite(&soem_->ctx, slave, index, subindex, FALSE, size,
                          data, timeout);
    if (result > 0) {
      break;
    }
    osal_usleep(10000);
  }

  return result;
}

int EtherCATComm::SendRxPDO(uint16 slave, uint16 pdo_index, uint32 data_size,
                            uint8* data) {
  (void)pdo_index;
  if (!data || data_size <= 0 || slave <= 0 || slave > soem_->ctx.slavecount) {
    return -1;
  }

  if (soem_->ctx.slavelist[slave].Obytes < data_size ||
      data_size > sizeof(rxpdo_buffer_)) {
    return -1;
  }

  std::lock_guard<std::mutex> lock(rt_context_mutex_);
  if (rxpdo_buffers_.size() <= slave) {
    rxpdo_buffers_.resize(static_cast<size_t>(soem_->ctx.slavecount) + 1,
                          std::vector<uint8_t>(sizeof(rxpdo_buffer_), 0));
    rxpdo_sizes_.resize(static_cast<size_t>(soem_->ctx.slavecount) + 1, 0);
  }

  memcpy(rxpdo_buffers_[slave].data(), data, data_size);
  rxpdo_sizes_[slave] = data_size;

  if (slave == 1) {
    memcpy(rxpdo_buffer_, data, data_size);
    rxpdo_size_ = data_size;
  }

  return 1;
}

uint8_t* EtherCATComm::ReadTxPDO(uint16 slave) {
  if (slave > 0 && slave <= soem_->ctx.slavecount) {
    return soem_->ctx.slavelist[slave].inputs;
  }
  return nullptr;
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
    for (const auto& hand : hands_) {
      int si = hand.slave_index;
      if (soem_->ctx.slavelist[si].outputs == nullptr) continue;
      size_t size = si < static_cast<int>(rxpdo_sizes_.size())
                        ? rxpdo_sizes_[si]
                        : 0;
      if (size == 0) continue;
      size_t copy_size =
          (std::min)(size,
                     static_cast<size_t>(soem_->ctx.slavelist[si].Obytes));
      memcpy(soem_->ctx.slavelist[si].outputs, rxpdo_buffers_[si].data(),
             copy_size);
    }
    ecx_send_processdata(&soem_->ctx);
  }

  while (threads_started_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (dorun_ > 0) {
      cycle_++;

      // Receive process data
      std::vector<std::pair<uint16_t, std::vector<uint8_t>>> pdo_copies;

      {
        std::lock_guard<std::mutex> lock(rt_context_mutex_);
        wkc_ = ecx_receive_processdata(&soem_->ctx, EC_TIMEOUTRET);

        if (wkc_ >= expectedWKC_ && inOP_) {
          rx_packets_++;
          // Every slave that produces a TxPDO carries its own feedback frame.
          // With a junction/splitter the hands sit at slaves 2..N while slave
          // 1 is the splitter itself, so collect one frame per slave instead
          // of only reading slavelist[1].
          for (const auto& hand : hands_) {
      int si = hand.slave_index;
            uint8_t* inputs = soem_->ctx.slavelist[si].inputs;
            size_t pdo_size = soem_->ctx.slavelist[si].Ibytes;
            if (inputs == nullptr || pdo_size == 0) {
              continue;
            }
            std::vector<uint8_t> copy(inputs, inputs + pdo_size);
            pdo_copies.emplace_back(static_cast<uint16_t>(si),
                                    std::move(copy));
          }
        } else {
          rx_errors_++;
        }
      }

      // Call callbacks outside the lock
      for (const auto& entry : pdo_copies) {
        received_frames_++;
        ParseAndNotify(entry.second.data(), entry.second.size(), entry.first);
      }

      if (wkc_ != expectedWKC_) {
        dowkccheck_++;
      } else {
        dowkccheck_ = 0;
      }

      {
        std::lock_guard<std::mutex> lock(rt_context_mutex_);
        for (const auto& hand : hands_) {
      int si = hand.slave_index;
          if (soem_->ctx.slavelist[si].outputs == nullptr) continue;
          size_t size = si < static_cast<int>(rxpdo_sizes_.size())
                            ? rxpdo_sizes_[si]
                            : 0;
          if (size == 0) continue;
          size_t copy_size =
              (std::min)(size,
                         static_cast<size_t>(soem_->ctx.slavelist[si].Obytes));
          memcpy(soem_->ctx.slavelist[si].outputs, rxpdo_buffers_[si].data(),
                 copy_size);
        }
        if (ecx_send_processdata(&soem_->ctx) > 0) {
          tx_packets_++;
        } else {
          tx_errors_++;
        }
      }
    }
  }
}

void EtherCATComm::Ecatcheck() {
  int slaveix;
  while (threads_started_) {
    osal_usleep(50000);

    if (inOP_ &&
        ((dowkccheck_ > 2) ||
         soem_->ctx.grouplist[currentgroup_].docheckstate)) {
      std::unique_lock<std::mutex> lock(rt_context_mutex_, std::try_to_lock);

      if (!lock.owns_lock()) {
        continue;
      }

      soem_->ctx.grouplist[currentgroup_].docheckstate = FALSE;
      ecx_readstate(&soem_->ctx);
      for (slaveix = 1; slaveix <= soem_->ctx.slavecount; slaveix++) {
        ec_slavet* slave = &soem_->ctx.slavelist[slaveix];

        if ((slave->group == currentgroup_) &&
            (slave->state != EC_STATE_OPERATIONAL)) {
          soem_->ctx.grouplist[currentgroup_].docheckstate = TRUE;
          if (slave->state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
            slave->state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
            ecx_writestate(&soem_->ctx, slaveix);
          } else if (slave->state == EC_STATE_SAFE_OP) {
            slave->state = EC_STATE_OPERATIONAL;
            if (slave->mbxhandlerstate == ECT_MBXH_LOST)
              slave->mbxhandlerstate = ECT_MBXH_CYCLIC;
            ecx_writestate(&soem_->ctx, slaveix);
          } else if (slave->state > EC_STATE_NONE) {
            if (ecx_reconfig_slave(&soem_->ctx, slaveix, kEcTimeoutMon) >=
                EC_STATE_PRE_OP) {
              slave->islost = FALSE;
            }
          } else if (!slave->islost) {
            ecx_statecheck(&soem_->ctx, slaveix, EC_STATE_OPERATIONAL,
                           EC_TIMEOUTRET);
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
            if (ecx_recover_slave(&soem_->ctx, slaveix, kEcTimeoutMon)) {
              slave->islost = FALSE;
              threads_started_ = true;
              is_connected_ = true;
              reconnects_++;
            }
          } else {
            slave->islost = FALSE;
            threads_started_ = true;
            is_connected_ = true;
            reconnects_++;
          }
        }
      }
      dowkccheck_ = 0;
    }
  }
}

bool EtherCATComm::InputBin(const char* fname, int* length) {
  FilePtr fp;
  int cc = 0, c;

#ifdef _WIN32
  FILE* raw_file = nullptr;
  errno_t err = fopen_s(&raw_file, fname, "rb");
  if (err != 0 || raw_file == nullptr) return false;
  fp.reset(raw_file);
#else
  fp.reset(fopen(fname, "rb"));
  if (fp == nullptr) return false;
#endif

  while (((c = fgetc(fp.get())) != EOF) &&
         (cc < static_cast<int>(kFirmwareBufferSize))) {
    file_buffer_[cc++] = (uint8_t)c;
  }

  *length = cc;
  return true;
}

FirmwareUpdateError EtherCATComm::CompleteFirmwareResetHandshake(uint16_t slave) {
  const uint16_t target_states[] = {
      EC_STATE_INIT,
      EC_STATE_PRE_OP,
      EC_STATE_SAFE_OP,
  };
  for (uint16_t target_state : target_states) {
    soem_->ctx.slavelist[slave].state = target_state;
    ecx_writestate(&soem_->ctx, slave);
    if (ecx_statecheck(&soem_->ctx, slave, target_state,
                       EC_TIMEOUTSTATE * 10) != target_state) {
      GHAND_LOG_ERROR("Failed to switch communication state after firmware FOE transfer");
      return FirmwareUpdateError::SWITCH_COMM_STATE_FAILED;
    }
  }

  dorun_ = 1;
  soem_->ctx.slavelist[slave].state = EC_STATE_OPERATIONAL;
  ecx_writestate(&soem_->ctx, slave);
  StartThreads();
  if (ecx_statecheck(&soem_->ctx, slave, EC_STATE_OPERATIONAL,
                     EC_TIMEOUTSTATE * 10) != EC_STATE_OPERATIONAL) {
    GHAND_LOG_ERROR("Failed to switch communication state after firmware FOE transfer");
    return FirmwareUpdateError::SWITCH_COMM_STATE_FAILED;
  }
  inOP_ = 1;

  const auto reset_ready_deadline =
      std::chrono::steady_clock::now() +
      std::chrono::milliseconds(kFirmwareResetReadyTimeoutMs);
  while (std::chrono::steady_clock::now() < reset_ready_deadline) {
    std::uint8_t command = 0x5B;
    int size = sizeof(std::uint8_t);
    if (SDOWrite(slave, kSdoFirmwareUpdate, 0x01, size, &command, EC_TIMEOUTRXM) <= 0) {
      // GHAND_LOG_ERROR("Failed to write firmware reset check command 0x5B");
      continue;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kSdoWriteSettleMs));

    std::uint8_t state = 0xFF;
    size = sizeof(std::uint8_t);
    if (SDORead(slave, kSdoFirmwareUpdate, 0x02, &size, &state, EC_TIMEOUTRXM) > 0 &&
        state == 1) {
      command = 0x5C;
      size = sizeof(std::uint8_t);
      if (SDOWrite(slave, kSdoFirmwareUpdate, 0x01, size, &command, EC_TIMEOUTRXM) <= 0) {
        GHAND_LOG_ERROR("Firmware reset command 0x5C failed");
        return FirmwareUpdateError::FIRMWARE_RESET_FAILED;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(kFirmwareResetDelayMs));
      return FirmwareUpdateError::SUCCESS;
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kFirmwareResetReadyPollMs));
  }

  GHAND_LOG_ERROR("Device firmware reset is unavailable");
  return FirmwareUpdateError::NOT_RESETTABLE;
}

FirmwareUpdateError EtherCATComm::BootUpdate(
    const std::string& file_path,
    std::function<void(int)> progress_callback) {
  auto hands = GetConnectedHands();
  if (hands.size() != 1 || hands.front().slave_index != 1) return FirmwareUpdateError::NOT_SUPPORTED;


  // Pre-check: write 0x5A to kSdoFirmwareUpdate:0x01
  std::uint8_t command = 0x5A;
  std::uint8_t state = 0xFF;
  int size = sizeof(std::uint8_t);
  std::uint8_t result = 0xFF;
  int ret = SDOWrite(1, kSdoFirmwareUpdate, 0x01, size, &command, EC_TIMEOUTRXM);
  if (ret > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    ret = SDORead(1, kSdoFirmwareUpdate, 0x02, &size, &state, EC_TIMEOUTRXM);
    if (ret > 0 && state == 0) {
      ret = SDORead(1, kSdoFirmwareUpdate, 0x03, &size, &result, EC_TIMEOUTRXM);
    }
  }
  if (result != 1) {
    return FirmwareUpdateError::PREPARE_COMMAND_FAILED;
  }

  int filesize = 0;
  int slave = 1;
  inOP_ = 0;
  dorun_ = 0;
  StopThreads();
  progress_callback_ = progress_callback;
  foe_instance_ = this;
  soem_->ctx.slavelist[0].state = EC_STATE_SAFE_OP;
  ecx_writestate(&soem_->ctx, 0);
  ecx_statecheck(&soem_->ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

  soem_->ctx.slavelist[0].state = EC_STATE_PRE_OP;
  ecx_writestate(&soem_->ctx, 0);
  ecx_statecheck(&soem_->ctx, 0, EC_STATE_PRE_OP, EC_TIMEOUTSTATE);

  soem_->ctx.slavelist[0].state = EC_STATE_INIT;
  ecx_writestate(&soem_->ctx, 0);
  ecx_statecheck(&soem_->ctx, 0, EC_STATE_INIT, EC_TIMEOUTSTATE);

  soem_->ctx.slavelist[slave].mbxhandlerstate = 0;

  ecx_FOEdefinehook(&soem_->ctx,
                    reinterpret_cast<void*>(EtherCATComm::FoeProgressHook));
  soem_->ctx.slavelist[slave].state = EC_STATE_BOOT;
  ecx_writestate(&soem_->ctx, slave);

  if (ecx_statecheck(&soem_->ctx, slave, EC_STATE_BOOT,
                     EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT) {
    if (InputBin(file_path.c_str(), &filesize)) {
      char file_name[] = "ECATFW__firmware";
      int update_result =
          ecx_FOEwrite(&soem_->ctx, slave, file_name, 0, filesize,
                       &file_buffer_, EC_TIMEOUTSTATE * 1000);

      FirmwareUpdateError reset_handshake_result =
          FirmwareUpdateError::FOE_TRANSFER_FAILED;
      if (update_result > 0) {
        reset_handshake_result = CompleteFirmwareResetHandshake(slave);
      }

      GHAND_LOG_DEBUG("Releasing device lock after firmware update (result: "
                      << update_result << ")");
      device_lock_.Release();

      foe_instance_ = nullptr;
      if (update_result > 0) {
        if (reset_handshake_result != FirmwareUpdateError::SUCCESS) {
          return reset_handshake_result;
        }
        return FirmwareUpdateError::SUCCESS;
      }
      return FirmwareUpdateError::FOE_TRANSFER_FAILED;
    } else {
      GHAND_LOG_DEBUG(
          "Releasing device lock after firmware file read failure");
      device_lock_.Release();
      foe_instance_ = nullptr;
      return FirmwareUpdateError::FOE_TRANSFER_FAILED;
    }
  } else {
    GHAND_LOG_DEBUG(
        "Releasing device lock after BOOT state transition failure");
    device_lock_.Release();
    foe_instance_ = nullptr;
    return FirmwareUpdateError::ENTER_BOOT_MODE_FAILED;
  }
}

void EtherCATComm::FoeProgressHook(uint16 slave, int32 packetnumber,
                                   int32 totalsize) {
  static int32 last_packet = -1;

  if (packetnumber != last_packet) {
    last_packet = packetnumber;

    if (foe_instance_ && slave > 0 &&
        slave <= foe_instance_->soem_->ctx.slavecount) {
      int maxdata = foe_instance_->soem_->ctx.slavelist[slave].mbx_l - 12;
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

// IComm business interface implementation

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

DeviceInfo EtherCATComm::GetDeviceInfo(uint16_t slave) {
  DeviceInfo info;
  uint8_t bytes[255] = {};
  int size = sizeof(bytes);
  if (SDORead(slave, kSdoDeviceName, 0, &size, bytes, EC_TIMEOUTRXM) > 0) {
    info.device_name = ByteArrayString(bytes, size);
  }
  size = 19;
  if (SDORead(slave, kSdoSerialNumber, 1, &size, bytes, EC_TIMEOUTRXM) > 0) {
    info.serial_number = ParseAsciiSerialNumber(bytes, size);
  }
  info.software_version = ReadFirmwareVersion(slave, 1);
  info.position_sensor_version = ReadFirmwareVersion(slave, 2);
  info.tactile_sensor_version = ReadFirmwareVersion(slave, 3);
  info.motor_driver_version = ReadFirmwareVersion(slave, 4);
  return info;
}

HandType EtherCATComm::GetHandType(uint16_t slave) {
  std::uint8_t value = 0;
  int size = sizeof(value);
  int result = SDORead(slave, kSdoHandType, 0x00, &size, &value, EC_TIMEOUTRXM);

  if (result == 1) {
    switch (value) {
      case 1:
        return HandType::LEFT;
      case 2:
        return HandType::RIGHT;
      default:
        return HandType::NONE;
    }
  }
  return HandType::NONE;
}

bool EtherCATComm::MoveJoints(uint16_t slave, const std::vector<JointCommand>& joints, ControlMode mode) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = MoveJoints(id, joints, mode) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || joints.empty()) return false;
  std::lock_guard<std::mutex> lock(command_mutex_);
  auto commands = joint_commands_[slave];
  for (auto joint : joints) {
    if (!config_.joint_limits.count(joint.id)) return false;
    if (joint.mode == ControlMode::INHERIT) {
      auto previous = commands.find(joint.id);
      joint.mode = mode == ControlMode::CUSTOM
          ? (previous == commands.end() ? ControlMode::POSITION : previous->second.mode) : mode;
    }
    commands[joint.id] = joint;
  }
  auto bytes = EncodeEthercatCommand(config_, mode, true, commands);
  if (SendRxPDO(slave, ECT_SDO_RXPDOASSIGN, static_cast<uint32_t>(bytes.size()), bytes.data()) <= 0) return false;
  joint_commands_[slave] = std::move(commands);
  hand_modes_[slave] = mode;
  return true;
}

bool EtherCATComm::Stop(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = Stop(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> lock(command_mutex_);
  for (JointId id : ControlledJoints(config_)) {
    auto& joint = joint_commands_[slave][id];
    if (joint.mode == ControlMode::INHERIT) joint.mode = ControlMode::POSITION;
    joint.stop = static_cast<uint8_t>(JointControlState::STOPPED);
  }
  auto bytes = EncodeEthercatCommand(config_, hand_modes_[slave], true, joint_commands_[slave]);
  return SendRxPDO(slave, ECT_SDO_RXPDOASSIGN, static_cast<uint32_t>(bytes.size()), bytes.data()) > 0;
}

bool EtherCATComm::DisableHand(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = DisableHand(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  if (!IsConnected()) return false;
  std::lock_guard<std::mutex> lock(command_mutex_);
  auto bytes = EncodeEthercatCommand(config_, hand_modes_[slave], false, joint_commands_[slave]);
  return SendRxPDO(slave, ECT_SDO_RXPDOASSIGN, static_cast<uint32_t>(bytes.size()), bytes.data()) > 0;
}

bool EtherCATComm::ClearFault(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = ClearFault(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  return IsConnected() && ExecuteSdoCommand(slave, kSdoFaultClear, 1);
}



bool EtherCATComm::OpenTactile(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = OpenTactile(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  return IsConnected() && ExecuteSdoCommand(slave, kSdoTactileControl, 1);
}

bool EtherCATComm::CloseTactile(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = CloseTactile(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  return IsConnected() && ExecuteSdoCommand(slave, kSdoTactileControl, 2);
}

bool EtherCATComm::ZeroTactile(uint16_t slave) {
  if (slave == 0) {
    auto ids = TargetSlaves(0); bool ok = !ids.empty();
    for (uint16_t id : ids) ok = ZeroTactile(id) && ok;
    return ok;
  }
  if (TargetSlaves(slave).empty()) return false;

  return IsConnected() && ExecuteSdoCommand(slave, kSdoTactileControl, 4);
}





// PDO parsing helper functions
namespace {

Force ParseResultantForce(const uint8_t* data) {
  Force force;
  force.x = static_cast<int16_t>(data[0] | (data[1] << 8)) / 10.0f;
  force.y = static_cast<int16_t>(data[2] | (data[3] << 8)) / 10.0f;
  force.z = static_cast<int16_t>(data[4] | (data[5] << 8)) / 10.0f;
  return force;
}

std::vector<Force> ParseSampleForces(const uint8_t* data, int sensor_count) {
  std::vector<Force> forces;
  forces.reserve(sensor_count);

  if (data != nullptr && sensor_count > 0) {
    for (int i = 0; i < sensor_count; i++) {
      Force sample_force;
      sample_force.x =
          static_cast<float>(static_cast<int8_t>(data[i * 3])) * 0.1f;
      sample_force.y =
          static_cast<float>(static_cast<int8_t>(data[i * 3 + 1])) * 0.1f;
      sample_force.z =
          static_cast<float>(static_cast<uint8_t>(data[i * 3 + 2])) * 0.1f;
      forces.push_back(sample_force);
    }
  }

  return forces;
}

}  // anonymous namespace

void EtherCATComm::ParseHandAndJoints(
    const uint8_t* data, size_t size, size_t* offset,
    std::vector<Joint>* parsed_joints,
  HandState* parsed_temperature) const {
  *parsed_temperature = HandState{};
  if (*offset + 28 > size) return;

  uint16_t hand_state;
  int16_t temperature;

  memcpy(&hand_state, data + *offset, sizeof(hand_state));
  *offset += sizeof(hand_state);

  memcpy(parsed_temperature->error_warn.data(), data + *offset,
         parsed_temperature->error_warn.size());
  *offset += parsed_temperature->error_warn.size();

  memcpy(parsed_temperature->error_fault.data(), data + *offset,
         parsed_temperature->error_fault.size());
  *offset += parsed_temperature->error_fault.size();

  memcpy(parsed_temperature->error_critical.data(), data + *offset,
         parsed_temperature->error_critical.size());
  *offset += parsed_temperature->error_critical.size();

  memcpy(&temperature, data + *offset, sizeof(temperature));
  *offset += sizeof(temperature);

  parsed_temperature->state = static_cast<State>(hand_state);
  parsed_temperature->temperature = temperature;

  parsed_joints->reserve(config_.valid_joints.size());
  for (const auto& joint_id : config_.valid_joints) {
    if (*offset + 10 > size) break;
    uint8_t joint_state, joint_error, joint_mode, joint_stop;
    int16_t joint_angle, joint_velocity, joint_current;

    memcpy(&joint_state, data + *offset, sizeof(joint_state));
    *offset += sizeof(joint_state);

    memcpy(&joint_error, data + *offset, sizeof(joint_error));
    *offset += sizeof(joint_error);

    memcpy(&joint_mode, data + *offset, sizeof(joint_mode));
    *offset += sizeof(joint_mode);

    memcpy(&joint_stop, data + *offset, sizeof(joint_stop));
    *offset += sizeof(joint_stop);

    memcpy(&joint_angle, data + *offset, sizeof(joint_angle));
    *offset += sizeof(joint_angle);

    memcpy(&joint_velocity, data + *offset, sizeof(joint_velocity));
    *offset += sizeof(joint_velocity);

    memcpy(&joint_current, data + *offset, sizeof(joint_current));
    *offset += sizeof(joint_current);

    Joint joint;
    joint.id = joint_id;
    joint.state = static_cast<JointStateCode>(joint_state);
    joint.error = static_cast<JointErrorCode>(joint_error);
    joint.mode = joint_mode;
    joint.stop = joint_stop;
    joint.angle = static_cast<float>(joint_angle) / 10.0f;
    joint.velocity = joint_velocity;
    joint.current = joint_current;
    parsed_joints->push_back(joint);
  }
}

TactileData EtherCATComm::ParseTactileData(const uint8_t* data, size_t size,
                                           size_t* offset, bool compat_thumb) const {
  TactileData tactile_data;

  if (*offset + 2 <= size) {
    tactile_data.sensor_state = data[*offset];
    tactile_data.sensor_error = data[*offset + 1];
    *offset += 2;
  }

  tactile_data.regions.reserve(config_.tactile_regions.size());
  for (size_t i = 0; i < config_.tactile_regions.size(); ++i) {
    const auto& rc = config_.tactile_regions[i];
    RegionTactile region;
    region.region_name = rc.id.c_str();
    region.state = (tactile_data.sensor_state & (1 << i)) != 0;

    if (*offset + 6 <= size) {
      region.resultant_force = ParseResultantForce(data + *offset);
      *offset += 6;
    }

    int count = compat_thumb && i == 0 ? 28 : rc.sensor_count;
    int sample_size = count * 3;
    if (count > 0 && *offset + sample_size <= size) {
      region.distributed_forces =
          ParseSampleForces(data + *offset, count);
      *offset += sample_size;
    }

    tactile_data.regions.push_back(std::move(region));
  }

  return tactile_data;
}

void EtherCATComm::ParseAndNotify(const uint8_t* data, size_t size, uint16_t slave) {
  if (data == nullptr || size == 0) {
    GHAND_LOG_WARNING("Received invalid data: null or empty");
    return;
  }

  size_t full = EthercatTxpdoSize(config_);
  size_t compat = EthercatTxpdoSize(config_, true);
  if (size != full && size != compat) { rx_errors_++; return; }
  size_t offset = 0;
  std::vector<Joint> parsed_joints;
  HandState parsed_temperature;
  ParseHandAndJoints(data, size, &offset, &parsed_joints,
                     &parsed_temperature);

  JointsCallback joints_cb;
  HandStateCallback hand_state_cb;
  TactileDataCallback tactile_cb;
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    joints_cb = joints_callback_;
    hand_state_cb = hand_state_callback_;
    tactile_cb = tactile_callback_;
  }

  bool dispatched = false;
  try {
    if (joints_cb) {
      joints_cb(slave, parsed_joints);
      dispatched = true;
    }
  } catch (const std::exception& e) {
    callback_errors_++;
    GHAND_LOG_ERROR("EtherCAT joints callback failed: " << e.what());
  } catch (...) {
    callback_errors_++;
    GHAND_LOG_ERROR("EtherCAT joints callback failed with unknown exception");
  }

  try {
    if (hand_state_cb) {
      hand_state_cb(slave, parsed_temperature);
      dispatched = true;
    }
  } catch (const std::exception& e) {
    callback_errors_++;
    GHAND_LOG_ERROR("EtherCAT hand-state callback failed: " << e.what());
  } catch (...) {
    callback_errors_++;
    GHAND_LOG_ERROR("EtherCAT hand-state callback failed with unknown exception");
  }

  if (config_.has_tactile && offset < size) {
    TactileData tactile_data = ParseTactileData(data, size, &offset, size == compat && compat != full);
    try {
      if (tactile_cb) {
        tactile_cb(slave, tactile_data);
        dispatched = true;
      }
    } catch (const std::exception& e) {
      callback_errors_++;
      GHAND_LOG_ERROR("EtherCAT tactile callback failed: " << e.what());
    } catch (...) {
      callback_errors_++;
      GHAND_LOG_ERROR("EtherCAT tactile callback failed with unknown exception");
    }
  }

  if (dispatched) {
    dispatched_frames_++;
  }
}

std::string EtherCATComm::ReadFirmwareVersion(uint16_t slave, uint8_t subindex) {
  uint8_t bytes[2] = {};
  int size = 2;
  if (SDORead(slave, kSdoFirmwareInfo, subindex, &size, bytes, EC_TIMEOUTRXM) <= 0 || size != 2) return "N/A";
  uint16_t value = static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
  return value ? ParsePackedFirmwareVersion(value) : "N/A";
}

bool EtherCATComm::QueryFirmwareUpdateResults(FirmwareUpdateResults* results) {
  auto read_result = [this](uint8_t cmd, uint8_t* out,
                            bool settle_on_zero) -> bool {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kFirmwareQueryTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
      int size = sizeof(std::uint8_t);
      if (SDOWrite(1, kSdoFirmwareUpdate, 0x01, size, &cmd, EC_TIMEOUTRXM) <= 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kFirmwareQueryPollMs));
        continue;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(kSdoWriteSettleMs));

      std::uint8_t ack = 0;
      size = sizeof(std::uint8_t);
      if (SDORead(1, kSdoFirmwareUpdate, 0x03, &size, &ack, EC_TIMEOUTRXM) <= 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kFirmwareQueryPollMs));
        continue;
      }
      if (ack == 2) {
        GHAND_LOG_ERROR("Firmware result query rejected (command 0x"
                        << std::hex << static_cast<int>(cmd) << std::dec
                        << ", result=2)");
        return false;
      }
      if (ack != 1) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kFirmwareQueryPollMs));
        continue;
      }

      std::uint8_t state = 0xFF;
      size = sizeof(std::uint8_t);
      if (SDORead(1, kSdoFirmwareUpdate, 0x02, &size, &state, EC_TIMEOUTRXM) <= 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kFirmwareQueryPollMs));
        continue;
      }
      if (state != 0 || !settle_on_zero) {
        *out = state;
        return true;
      }

      std::this_thread::sleep_for(
          std::chrono::milliseconds(kFirmwareQueryPollMs));
    }
    if (settle_on_zero) {
      GHAND_LOG_WARNING("Firmware main result still 0 after "
                        << kFirmwareQueryTimeoutMs
                        << " ms, reporting as failed/no result");
      *out = 0;
      return true;
    }
    GHAND_LOG_ERROR("Timed out waiting for firmware update result query "
                    "(command 0x" << std::hex << static_cast<int>(cmd)
                    << std::dec << ")");
    return false;
  };

  bool ok = true;
  ok = read_result(0xA1, &results->main, /*settle_on_zero=*/true) && ok;
  ok = read_result(0xA2, &results->position_sensor, false) && ok;
  ok = read_result(0xA3, &results->tactile_board, false) && ok;
  ok = read_result(0xA4, &results->motor_driver, false) && ok;
  ok = read_result(0xA5, &results->thumb_tactile, false) && ok;
  ok = read_result(0xA6, &results->finger_tactile, false) && ok;
  return ok;
}

bool EtherCATComm::ExecuteSdoCommand(uint16_t slave, uint16_t index, uint8_t command,
    uint8_t command_subindex, uint8_t state_subindex, uint8_t result_subindex) {
  if (SDOWrite(slave, index, command_subindex, 1, &command, EC_TIMEOUTRXM) <= 0) return false;
  std::this_thread::sleep_for(std::chrono::milliseconds(kSdoWriteSettleMs));
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < deadline) {
    uint8_t state = 0xFF, result = 0;
    int size = 1;
    if (SDORead(slave, index, state_subindex, &size, &state, EC_TIMEOUTRXM) <= 0) return false;
    if (state == 0) {
      size = 1;
      if (SDORead(slave, index, result_subindex, &size, &result, EC_TIMEOUTRXM) <= 0) return false;
      if (result) return result == 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

bool EtherCATComm::GetHandStateMachine(uint16_t slave, HandStateMachine* state) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || !state) return false;
  uint8_t value = 0;
  int size = 1;
  if (SDORead(slave, 0x200A, 1, &size, &value, EC_TIMEOUTRXM) <= 0 || size != 1) return false;
  *state = static_cast<HandStateMachine>(value);
  return true;
}

bool EtherCATComm::WriteSdoParameters(uint16_t slave, uint16_t index, const std::vector<uint8_t>& bytes) {
  auto payload = bytes;
  if (SDOWrite(slave, index, 1, static_cast<int>(payload.size()), payload.data(), EC_TIMEOUTRXM) <= 0) return false;
  std::vector<uint8_t> readback(bytes.size());
  int size = static_cast<int>(readback.size());
  if (SDORead(slave, index, 1, &size, readback.data(), EC_TIMEOUTRXM) <= 0 ||
      size != static_cast<int>(bytes.size()) || readback != bytes) return false;
  return ExecuteSdoCommand(slave, index, 0xA1, 2, 3, 4);
}

bool EtherCATComm::GetStallTolerances(uint16_t slave, std::map<JointId, uint8_t>* values) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || !values) return false;
  uint8_t bytes[13] = {};
  int size = 13;
  if (SDORead(slave, 0x2003, 1, &size, bytes, EC_TIMEOUTRXM) <= 0 || size != 13) return false;
  values->clear();
  for (size_t i = 0; i < kStallToleranceJoints.size(); ++i)
    (*values)[kStallToleranceJoints[i]] = bytes[i];
  return true;
}

bool EtherCATComm::SetStallTolerances(uint16_t slave, const std::map<JointId, uint8_t>& values) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || values.size() != 13) return false;
  std::vector<uint8_t> bytes;
  for (JointId id : kStallToleranceJoints) {
    auto it = values.find(id);
    if (it == values.end()) return false;
    bytes.push_back(it->second);
  }
  return WriteSdoParameters(slave, 0x2003, bytes);
}

bool EtherCATComm::GetPidParameters(uint16_t slave, std::vector<uint16_t>* values) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || !values) return false;
  uint8_t bytes[78] = {};
  int size = 78;
  if (SDORead(slave, 0x2008, 1, &size, bytes, EC_TIMEOUTRXM) <= 0 || size != 78) return false;
  values->clear();
  for (size_t i = 0; i < 78; i += 2) values->push_back(static_cast<uint16_t>(bytes[i] | (bytes[i + 1] << 8)));
  return true;
}

bool EtherCATComm::SetPidParameters(uint16_t slave, const std::vector<uint16_t>& values) {
  if (slave == 0 || TargetSlaves(slave).empty()) return false;

  if (!IsConnected() || values.size() != 39) return false;
  std::vector<uint8_t> bytes;
  for (uint16_t value : values) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
  }
  return WriteSdoParameters(slave, 0x2008, bytes);
}

std::vector<ConnectedHand> EtherCATComm::GetConnectedHands() {
  std::lock_guard<std::mutex> lock(context_mutex_);
  return hands_;
}
std::vector<uint16_t> EtherCATComm::TargetSlaves(uint16_t slave) {
  std::lock_guard<std::mutex> lock(context_mutex_);
  std::vector<uint16_t> result;
  for (const auto& hand : hands_) if (slave == 0 || hand.slave_index == slave) result.push_back(hand.slave_index);
  return result;
}
}  // namespace internal
}  // namespace ghand
