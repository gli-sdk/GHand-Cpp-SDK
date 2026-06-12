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
#include <cstdint>
#include <cstdio>
#include <cstring>
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

constexpr double kPi = 3.14159265358979323846;
constexpr int kEcTimeoutMon = 500;

namespace ghand {
namespace internal {

// Joint data size per joint: float(4B) + uint8 velocity(1B) + uint8 torque(1B)
constexpr size_t kEthercatJointDataSize = 6;

namespace {

std::string ByteArrayString(const std::uint8_t* data, size_t size) {
  std::string result;
  for (size_t i = 0; i < size && data[i] != 0; ++i) {
    result.push_back(static_cast<char>(data[i]));
  }
  return result;
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
    : soem_(std::make_unique<SoemState>()), config_(config) {}

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

int EtherCATComm::Connect(const std::string& device_name) {
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
  rxpdo_buffer_[1] = {0x01};

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

  mappingdone_ = 1;
  dorun_ = 1;
  expectedWKC_ = (group->outputsWKC * 2) + group->inputsWKC;

  ecx_configdc(&soem_->ctx);

  for (int si = 1; si <= soem_->ctx.slavecount; si++) {
    ec_slavet* slave = &soem_->ctx.slavelist[si];
    if (slave->CoEdetails > 0) {
      ecx_slavembxcyclic(&soem_->ctx, si);
    }
  }

  uint16_t safe_op_state =
      ecx_statecheck(&soem_->ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

  if (safe_op_state != EC_STATE_SAFE_OP) {
    device_lock_.Release();
    return -5;
  }

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
  rxpdo_buffer_[1] = {0x01};
  if (soem_->ctx.slavecount > 0) {
    soem_->ctx.slavelist[0].state = EC_STATE_INIT;
    ecx_writestate(&soem_->ctx, 0);
  }

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
  if (!data || data_size <= 0 || slave <= 0 || slave > soem_->ctx.slavecount) {
    return -1;
  }

  if (soem_->ctx.slavelist[slave].Obytes < data_size ||
      data_size > sizeof(rxpdo_buffer_)) {
    return -1;
  }

  memcpy(rxpdo_buffer_, data, data_size);

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
    memcpy(soem_->ctx.slavelist[1].outputs, rxpdo_buffer_,
           sizeof(rxpdo_buffer_));
    ecx_send_processdata(&soem_->ctx);
  }

  while (threads_started_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (dorun_ > 0) {
      cycle_++;

      // Receive process data
      std::vector<uint8_t> pdo_copy;

      {
        std::lock_guard<std::mutex> lock(rt_context_mutex_);
        wkc_ = ecx_receive_processdata(&soem_->ctx, EC_TIMEOUTRET);

        if (wkc_ >= expectedWKC_ && inOP_) {
          uint8_t* inputs = soem_->ctx.slavelist[1].inputs;
          size_t pdo_size = soem_->ctx.slavelist[1].Ibytes;

          if (pdo_size > 0) {
            pdo_copy.assign(inputs, inputs + pdo_size);
          }
        }
      }

      // Call callbacks outside the lock
      if (!pdo_copy.empty()) {
        ParseAndNotify(pdo_copy.data(), pdo_copy.size());
      }

      if (wkc_ != expectedWKC_) {
        dowkccheck_++;
      } else {
        dowkccheck_ = 0;
      }

      {
        std::lock_guard<std::mutex> lock(rt_context_mutex_);
        memcpy(soem_->ctx.slavelist[1].outputs, rxpdo_buffer_,
               sizeof(rxpdo_buffer_));
        ecx_send_processdata(&soem_->ctx);
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
  std::unique_ptr<FILE, decltype(&fclose)> fp(nullptr, &fclose);
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

FirmwareUpdateError EtherCATComm::BootUpdate(
    const std::string& file_path,
    std::function<void(int)> progress_callback) {

  // Pre-check: write 0x5A to 0x2005:0x01
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
                       &file_buffer_, EC_TIMEOUTSTATE * 10);

      GHAND_LOG_DEBUG("Releasing device lock after firmware update (result: "
                      << update_result << ")");
      device_lock_.Release();

      foe_instance_ = nullptr;
      if (update_result > 0) {
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

DeviceInfo EtherCATComm::GetDeviceInfo() {
  DeviceInfo info;
  std::uint8_t value[255] = {0};
  int size = sizeof(value);
  int result = -1;

  result = SDORead(1, 0x1008, 0x00, &size, &value, EC_TIMEOUTRXM);
  if (result == 1) {
    info.device_name = ByteArrayString(value, sizeof(value));
  }

  size = sizeof(value);
  memset(value, 0, sizeof(value));
  result = SDORead(1, 0x1009, 0x00, &size, &value, EC_TIMEOUTRXM);
  if (result == 1) {
    info.hardware_version = ByteArrayString(value, sizeof(value));
  }

  size = sizeof(value);
  memset(value, 0, sizeof(value));
  result = SDORead(1, 0x100A, 0x00, &size, &value, EC_TIMEOUTRXM);
  if (result == 1) {
    info.software_version = ByteArrayString(value, sizeof(value));
  }

  size = sizeof(value);
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

  info.software_version = ReadFirmwareVersion(0x01);
  info.position_sensor_version = ReadFirmwareVersion(0x02);
  info.tactile_sensor_version = ReadFirmwareVersion(0x03);
  info.motor_driver_version = ReadFirmwareVersion(0x04);
  info.firmware_package_version = ReadFirmwareVersion(0x05);

  return info;
}

HandType EtherCATComm::GetHandType() {
  std::uint8_t value = 0;
  int size = sizeof(value);
  int result = SDORead(1, 0x2001, 0x00, &size, &value, EC_TIMEOUTRXM);

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

bool EtherCATComm::MoveJoints(const std::vector<JointCommand>& joints,
                              ControlMode mode) {
  if (!IsConnected()) {
    GHAND_LOG_ERROR("Cannot move joints: device not connected");
    return false;
  }
  if (joints.empty()) {
    GHAND_LOG_WARNING("MoveJoints called with empty joint list");
    return false;
  }

  // EtherCAT PDO requires fixed length; fill missing joints in joint_limits
  // order.
  std::map<JointId, JointCommand> joint_map;
  for (const auto& joint : joints) {
    joint_map[joint.id] = joint;
  }

  std::vector<uint8_t> buffer(
      config_.joint_limits.size() * kEthercatJointDataSize + 2, 0);
  size_t offset = 0;

  buffer[offset++] = static_cast<uint8_t>(mode);
  buffer[offset++] = 0;  // stop

  for (const auto& kv : config_.joint_limits) {
    auto joint_id = kv.first;
    float angle = 0.0f;
    uint8_t velocity = 0;
    uint8_t torque = 0;
    auto it = joint_map.find(joint_id);
    if (it != joint_map.end()) {
      angle = it->second.angle * (static_cast<float>(kPi) / 180.0f);
      velocity = it->second.velocity;
      torque = it->second.torque;
    }
    memcpy(buffer.data() + offset, &angle, sizeof(angle));
    offset += sizeof(angle);
    buffer[offset++] = velocity;
    buffer[offset++] = torque;
  }

  GHAND_LOG_INFO("Sending PDO data");
  SendRxPDO(1, ECT_SDO_RXPDOASSIGN, static_cast<uint32_t>(buffer.size()),
            buffer.data());
  return true;
}

void EtherCATComm::Stop() {
  GHAND_LOG_INFO("Sending stop command");
  std::vector<uint8_t> buffer(
      config_.joint_limits.size() * kEthercatJointDataSize + 2, 0);
  if (buffer.size() > 1) {
    buffer[1] = 1;
  }
  SendRxPDO(1, ECT_SDO_RXPDOASSIGN, static_cast<uint32_t>(buffer.size()),
            buffer.data());
}

bool EtherCATComm::ClearFault() {
  GHAND_LOG_INFO("Clearing device fault");

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
  GHAND_LOG_ERROR("Clear fault operation timed out");
  return false;
}

bool EtherCATComm::InitJoint() {
  GHAND_LOG_INFO("Initializing joint positions");

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
  GHAND_LOG_ERROR("Joint initialization timed out");
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

// PDO parsing helper functions
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
    resultant.z =
        static_cast<float>(static_cast<uint8_t>(raw_z & 0xFF)) * 0.1f;
  }

  return resultant;
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
    const uint8_t* data, size_t* offset,
    std::vector<Joint>* parsed_joints,
    HandState* parsed_temperature) const {
  uint8_t hand_state, hand_error;
  int16_t temperature;

  memcpy(&hand_state, data + *offset, sizeof(hand_state));
  *offset += sizeof(hand_state);

  memcpy(&hand_error, data + *offset, sizeof(hand_error));
  *offset += sizeof(hand_error);

  memcpy(&temperature, data + *offset, sizeof(temperature));
  *offset += sizeof(temperature);

  parsed_temperature->state = static_cast<State>(hand_state);
  parsed_temperature->error = static_cast<ErrorCode>(hand_error);
  parsed_temperature->temperature = temperature;

  parsed_joints->reserve(config_.valid_joints.size());
  for (const auto& joint_id : config_.valid_joints) {
    uint8_t joint_state, joint_error;
    float joint_angle;
    uint8_t joint_velocity, joint_torque;

    memcpy(&joint_state, data + *offset, sizeof(joint_state));
    *offset += sizeof(joint_state);

    memcpy(&joint_error, data + *offset, sizeof(joint_error));
    *offset += sizeof(joint_error);

    memcpy(&joint_angle, data + *offset, sizeof(joint_angle));
    *offset += sizeof(joint_angle);

    memcpy(&joint_velocity, data + *offset, sizeof(joint_velocity));
    *offset += sizeof(joint_velocity);

    memcpy(&joint_torque, data + *offset, sizeof(joint_torque));
    *offset += sizeof(joint_torque);

    Joint joint;
    joint.id = joint_id;
    joint.state = static_cast<State>(joint_state);
    joint.error = static_cast<ErrorCode>(joint_error);
    joint.angle = joint_angle * (180.0f / static_cast<float>(kPi));
    joint.velocity = joint_velocity;
    joint.torque = joint_torque;
    parsed_joints->push_back(joint);
  }
}

TactileData EtherCATComm::ParseTactileData(const uint8_t* data, size_t size,
                                           size_t* offset) const {
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
    region.region_name = rc.name.c_str();
    region.state = (tactile_data.sensor_state & (1 << i)) != 0;

    if (*offset + 6 <= size) {
      region.resultant_force = ParseResultantForce(data + *offset);
      *offset += 6;
    }

    int sample_size = rc.sensor_count * 3;
    if (rc.sensor_count > 0 && *offset + sample_size <= size) {
      region.distributed_forces =
          ParseSampleForces(data + *offset, rc.sensor_count);
      *offset += sample_size;
    }

    tactile_data.regions.push_back(std::move(region));
  }

  return tactile_data;
}

void EtherCATComm::ParseAndNotify(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    GHAND_LOG_WARNING("Received invalid data: null or empty");
    return;
  }

  size_t offset = 0;
  std::vector<Joint> parsed_joints;
  HandState parsed_temperature;
  ParseHandAndJoints(data, &offset, &parsed_joints, &parsed_temperature);

  std::lock_guard<std::mutex> lock(callback_mutex_);
  if (joints_callback_) {
    joints_callback_(parsed_joints);
  }
  if (hand_state_callback_) {
    hand_state_callback_(parsed_temperature);
  }

  if (config_.has_tactile && offset < size) {
    TactileData tactile_data = ParseTactileData(data, size, &offset);
    if (tactile_callback_) {
      tactile_callback_(tactile_data);
    }
  }
}

std::string EtherCATComm::ReadFirmwareVersion(uint8_t mcu_id) {
  std::uint8_t cmd = mcu_id;
  int size = sizeof(std::uint8_t);
  if (SDOWrite(1, 0x2007, 0x01, size, &cmd, EC_TIMEOUTRXM) <= 0) {
    return "";
  }

  std::uint8_t version_high = 0;
  size = sizeof(std::uint8_t);
  if (SDORead(1, 0x2007, 0x02, &size, &version_high, EC_TIMEOUTRXM) <= 0) {
    return "";
  }

  std::uint8_t version_low = 0;
  size = sizeof(std::uint8_t);
  if (SDORead(1, 0x2007, 0x03, &size, &version_low, EC_TIMEOUTRXM) <= 0) {
    return "";
  }

  uint8_t major = (version_high >> 5) & 0x07;
  uint8_t minor = version_high & 0x1F;
  uint8_t patch = (version_low >> 4) & 0x0F;

  return std::to_string(major) + "." + std::to_string(minor) + "." +
         std::to_string(patch);
}

bool EtherCATComm::QueryFirmwareUpdateResults(uint8_t* main_result,
                                               uint8_t* pos_result,
                                               uint8_t* tac_result,
                                               uint8_t* motor_result) {
  auto read_result = [this](uint8_t cmd, uint8_t* out) -> bool {
    int size = sizeof(std::uint8_t);
    if (SDOWrite(1, 0x2005, 0x01, size, &cmd, EC_TIMEOUTRXM) <= 0) {
      return false;
    }
    std::uint8_t state = 0xFF;
    size = sizeof(std::uint8_t);
    if (SDORead(1, 0x2005, 0x02, &size, &state, EC_TIMEOUTRXM) <= 0) {
      return false;
    }
    std::uint8_t result = 0xFF;
    size = sizeof(std::uint8_t);
    if (SDORead(1, 0x2005, 0x03, &size, &result, EC_TIMEOUTRXM) <= 0) {
      return false;
    }
    if (state == 0) {
      return false;
    }
    *out = result;
    return true;
  };

  bool ok = true;
  ok = read_result(0xA1, main_result) && ok;
  ok = read_result(0xA2, pos_result) && ok;
  ok = read_result(0xA3, tac_result) && ok;
  ok = read_result(0xA4, motor_result) && ok;
  return ok;
}

}  // namespace internal
}  // namespace ghand
