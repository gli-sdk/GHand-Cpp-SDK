#include "canfd_driver.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cfgmgr32.h>
#include <devguid.h>
#ifdef __MINGW32__
#include <ntdef.h>
#endif
#include <ntddser.h>
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#endif

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// ZLG CAN secondary development library header
#include "zlgcan.h"

#include "ghand/logging.h"
#include "logging_macros.h"

static const std::map<std::string, int>& GetZLGDeviceTypeMap() {
  static const std::map<std::string, int> kMap = {
#include "zlgcan_device_types.inc"
  };
  return kMap;
}

// Known ZLG CANFD device type list
static const int kCommonZLGDeviceTypes[] = {
    ZCAN_USBCANFD_100U,
    ZCAN_USBCANFD_200U,
    ZCAN_USBCANFD_400U,
    ZCAN_USBCANFD_MINI,
    ZCAN_USBCANFD_800U,
    ZCAN_PCIE_CANFD_100U,
    ZCAN_PCIE_CANFD_200U,
    ZCAN_PCIE_CANFD_400U,
    ZCAN_PCIE_CANFD_100U_EX,
    ZCAN_PCIE_CANFD_400U_EX,
    ZCAN_PCIE_CANFD_200U_MINI,
    ZCAN_PCIE_CANFD_200U_M2,
    ZCAN_CANFDCOM_100IE,
};

namespace ghand {
namespace internal {

namespace {

std::string ByteArrayString(const BYTE* data, size_t size) {
  std::string result;
  for (size_t i = 0; i < size && data[i] != 0; ++i) {
    result.push_back(static_cast<char>(data[i]));
  }
  return result;
}

}  // namespace

/**
 * @brief ZLG CANFD driver implementation
 *
 * Wraps the ZLG zlgcan.dll C API to provide a cross-platform unified CANFD
 * interface.
 *
 * Open() name parameter format: "DeviceType:DeviceIndex:CanIndex"
 *  DeviceType is the numeric value of the macro defined in zlgcan.h
 *  (e.g., "42" for ZCAN_USBCANFD_100U). Example: "42:0:0"
 */
static std::string FrameTypeString(canfd::FrameType ft) {
  switch (ft) {
    case canfd::BROADCAST:
      return "BC";
    case canfd::COMMAND:
      return "CMD";
    case canfd::RESPONSE:
      return "RESP";
    case canfd::ACTIVE_REPORT:
      return "ARPT";
    default:
      return "?";
  }
}

static std::string HexDump(const uint8_t* data, uint8_t len) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (uint8_t i = 0; i < len; ++i) {
    if (i > 0) oss << ' ';
    oss << std::setw(2) << static_cast<int>(data[i]);
  }
  return oss.str();
}

static bool ParseInt(const std::string& text, int* value) {
  if (value == nullptr) return false;

  errno = 0;
  char* end = nullptr;
  long parsed = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0' || errno == ERANGE ||
      parsed < (std::numeric_limits<int>::min)() ||
      parsed > (std::numeric_limits<int>::max)()) {
    return false;
  }

  *value = static_cast<int>(parsed);
  return true;
}

struct ZLGOpenParams {
  int device_type = 48;
  int device_index = 0;
  int can_index = 0;
  int abit_sample_point = 75;
  int dbit_sample_point = 80;
};

static std::vector<std::string> SplitColon(const std::string& text) {
  std::istringstream stream(text);
  std::string part;
  std::vector<std::string> parts;
  while (std::getline(stream, part, ':')) {
    parts.push_back(part);
  }
  return parts;
}

static bool ParseZLGOpenParams(const std::string& name,
                               ZLGOpenParams* params) {
  std::vector<std::string> parts = SplitColon(name);
  if (parts.size() >= 1) {
    const auto& type_map = GetZLGDeviceTypeMap();
    auto it = type_map.find(parts[0]);
    if (it != type_map.end()) {
      params->device_type = it->second;
    } else if (!ParseInt(parts[0], &params->device_type)) {
      GHAND_LOG_ERROR("Invalid ZLG device type: "
                      << parts[0]
                      << ". Please use the macro name defined in zlgcan.h "
                      << "(e.g., \"ZCAN_USBCANFD_100U:0:0\") "
                      << "or the numeric value (e.g., \"42:0:0\").");
      return false;
    }
  }
  if (parts.size() >= 2) {
    ParseInt(parts[1], &params->device_index);
  }
  if (parts.size() >= 3) {
    ParseInt(parts[2], &params->can_index);
  }
  if (parts.size() >= 4) {
    ParseInt(parts[3], &params->abit_sample_point);
  }
  if (parts.size() >= 5) {
    ParseInt(parts[4], &params->dbit_sample_point);
  }
  return true;
}

#ifdef _WIN32
struct ComPortEntry {
  std::string port_name;      // e.g., "COM3"
  // Friendly name, e.g., "ZLG USBCANFD-100U (COM3)".
  std::string friendly_name;
  std::string device_desc;    // device description, usually contains model
  std::string serial_number;
  int vid = 0;
  int pid = 0;
};

static std::vector<ComPortEntry> EnumerateComPorts() {
  std::vector<ComPortEntry> result;
  HDEVINFO hDevInfo =
      SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (hDevInfo == INVALID_HANDLE_VALUE) return result;

  SP_DEVINFO_DATA devInfoData;
  devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

  for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
    BYTE hwId[256] = {};
    DWORD dataType = 0, reqSize = 0;
    if (!SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_HARDWAREID, &dataType, hwId,
            sizeof(hwId), &reqSize)) {
      continue;
    }

    std::string hardware_id = ByteArrayString(hwId, sizeof(hwId));
    int vid = 0, pid = 0;
    const char* vid_pos = strstr(hardware_id.c_str(), "VID_");
    const char* pid_pos = strstr(hardware_id.c_str(), "PID_");
    if (vid_pos && pid_pos) {
      vid = static_cast<int>(strtol(vid_pos + 4, nullptr, 16));
      pid = static_cast<int>(strtol(pid_pos + 4, nullptr, 16));
    }
    if (vid == 0) continue;

    BYTE friendly[256] = {};
    reqSize = 0;
    SetupDiGetDeviceRegistryPropertyA(
        hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, &dataType, friendly,
        sizeof(friendly), &reqSize);

    BYTE device_desc[256] = {};
    reqSize = 0;
    SetupDiGetDeviceRegistryPropertyA(
        hDevInfo, &devInfoData, SPDRP_DEVICEDESC, &dataType, device_desc,
        sizeof(device_desc), &reqSize);

    std::string port_name;
    std::string friendly_str = ByteArrayString(friendly, sizeof(friendly));
    size_t lp = friendly_str.find('(');
    size_t rp = friendly_str.find(')', lp);
    if (lp != std::string::npos && rp != std::string::npos && rp > lp + 1) {
      port_name = friendly_str.substr(lp + 1, rp - lp - 1);
    }
    if (port_name.empty() || port_name.substr(0, 3) != "COM") continue;

    char instanceId[256] = {};
    if (CM_Get_Device_IDA(devInfoData.DevInst, instanceId, sizeof(instanceId),
                          0) == CR_SUCCESS) {
      char* last_slash = strrchr(instanceId, '\\');
      if (last_slash && strlen(last_slash + 1) > 0) {
        ComPortEntry entry;
        entry.port_name = port_name;
        entry.friendly_name = friendly_str;
        entry.device_desc = ByteArrayString(device_desc, sizeof(device_desc));
        entry.serial_number = last_slash + 1;
        entry.vid = vid;
        entry.pid = pid;
        result.push_back(entry);
      }
    }
  }

  SetupDiDestroyDeviceInfoList(hDevInfo);
  return result;
}

static bool IsZLGVID(int vid) { return vid == 0x3562 || vid == 0x0483; }

/**
 * @brief Determine ZLG device_type from USB VID/PID
 *
 * The mapping table can be extended based on actual connected ZLG devices.
 * Common USBCANFD series VID is 0x3562, different models correspond to
 * different PIDs.
 */
static int GetZLGDeviceTypeFromVIDPID(int vid, int pid) {
  static const std::map<std::pair<int, int>, int> kVIDPIDToType = {
      // ZLG USBCANFD series (VID=0x3562)
      {{0x3562, 0x0101}, ZCAN_USBCANFD_100U},
      {{0x3562, 0x0102}, ZCAN_USBCANFD_200U},
      {{0x3562, 0x0103}, ZCAN_USBCANFD_400U},
      {{0x3562, 0x0104}, ZCAN_USBCANFD_MINI},
      {{0x3562, 0x0105}, ZCAN_USBCANFD_800U},
      // Add other VID/PID combinations here if needed
  };
  auto it = kVIDPIDToType.find({vid, pid});
  return (it != kVIDPIDToType.end()) ? it->second : 0;
}

static bool ResolveComPortToZLGDevice(const std::string& port_name,
                                      int* out_device_type,
                                      int* out_device_index) {
  std::string target_port = port_name;
  std::transform(target_port.begin(), target_port.end(), target_port.begin(),
                 ::toupper);

  auto ports = EnumerateComPorts();
  std::string target_friendly;
  std::string target_desc;
  int target_vid = 0, target_pid = 0;
  for (const auto& p : ports) {
    if (p.port_name == target_port) {
      target_friendly = p.friendly_name;
      target_desc = p.device_desc;
      target_vid = p.vid;
      target_pid = p.pid;
      break;
    }
  }
  if (target_vid == 0) {
    GHAND_LOG_ERROR("COM port not found: " << port_name);
    return false;
  }
  if (!IsZLGVID(target_vid)) {
    GHAND_LOG_WARNING("COM port " << port_name << " VID=0x" << std::hex
                                  << target_vid
                                  << " does not look like a ZLG device");
  }

  // Helper lambda: try device indices 0~7 for a given device_type, return on
  // first success.
  auto try_open = [&out_device_type, &out_device_index](int dev_type) -> bool {
    for (int dev_index = 0; dev_index < 8; ++dev_index) {
      long handle = ZCAN_OpenDevice(dev_type, dev_index, 0);
      if (handle == INVALID_DEVICE_HANDLE) continue;
      *out_device_type = dev_type;
      *out_device_index = dev_index;
      ZCAN_CloseDevice(handle);
      return true;
    }
    return false;
  };

  // 2. Name not recognized, try VID/PID mapping table
  int vidpid_type = GetZLGDeviceTypeFromVIDPID(target_vid, target_pid);
  if (vidpid_type != 0 && try_open(vidpid_type)) {
    GHAND_LOG_INFO("Resolved " << port_name << " -> ZLG device type="
                         << *out_device_type << " index=" << *out_device_index
                         << " (from VID/PID)");
    return true;
  }
  GHAND_LOG_ERROR("Failed to find online ZLG device for " << port_name);
  return false;
}

static bool IsComPortName(const std::string& name) {
  if (name.size() < 3) return false;
  std::string prefix = name.substr(0, 3);
  std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::toupper);
  return prefix == "COM";
}

static void ParseComPortSuffix(const std::string& name,
                               ZLGOpenParams* params,
                               std::string* com_name) {
  size_t colon_pos = name.find(':');
  if (colon_pos == std::string::npos) return;

  *com_name = name.substr(0, colon_pos);
  std::vector<std::string> suffix_parts =
      SplitColon(name.substr(colon_pos + 1));
  if (!suffix_parts.empty()) {
    ParseInt(suffix_parts[0], &params->can_index);
  }
  if (suffix_parts.size() >= 2) {
    ParseInt(suffix_parts[1], &params->abit_sample_point);
  }
  if (suffix_parts.size() >= 3) {
    ParseInt(suffix_parts[2], &params->dbit_sample_point);
  }
}

static bool ResolveComPortOpenParams(const std::string& name,
                                     ZLGOpenParams* params,
                                     std::string* resolved_name) {
  std::string com_name = name;
  ParseComPortSuffix(name, params, &com_name);

  int resolved_type = 0, resolved_index = 0;
  if (!ResolveComPortToZLGDevice(com_name, &resolved_type, &resolved_index)) {
    GHAND_LOG_ERROR("Failed to resolve COM port: " << name);
    return false;
  }

  *resolved_name = std::to_string(resolved_type) + ":" +
                   std::to_string(resolved_index) + ":" +
                   std::to_string(params->can_index) + ":" +
                   std::to_string(params->abit_sample_point) + ":" +
                   std::to_string(params->dbit_sample_point);
  GHAND_LOG_INFO("Resolved " << name << " to ZLG device " << *resolved_name);
  return true;
}
#endif  // _WIN32

class ZLGDriver : public CANFDDriver {
 public:
  ZLGDriver() = default;
  ~ZLGDriver() override { Close(); }

  ZLGDriver(const ZLGDriver&) = delete;
  ZLGDriver& operator=(const ZLGDriver&) = delete;
  ZLGDriver(ZLGDriver&&) = delete;
  ZLGDriver& operator=(ZLGDriver&&) = delete;

  int Open(const std::string& name, uint32_t bitrate,
           uint32_t dbitrate) override {
    Close();

    ZLGOpenParams params;
    std::string resolved_name = name;
#ifdef _WIN32
    if (IsComPortName(name) &&
        !ResolveComPortOpenParams(name, &params, &resolved_name)) {
      return -1;
    }
#endif

    if (!ParseZLGOpenParams(resolved_name, &params)) return -1;
    can_index_ = params.can_index;

    device_handle_ =
        ZCAN_OpenDevice(params.device_type, params.device_index, 0);
    if (device_handle_ == INVALID_DEVICE_HANDLE) {
      GHAND_LOG_ERROR("ZCAN_OpenDevice failed: type="
                      << params.device_type << " index=" << params.device_index
                      << " err=" << GetLastError());
      return -1;
    }

    int configure_result =
        ConfigureChannelProperties(params, bitrate, dbitrate);
    if (configure_result != 0) return configure_result;

    int init_result = InitializeChannel(params);
    if (init_result != 0) return init_result;

    GHAND_LOG_INFO("ZLG CANFD opened: type="
                   << params.device_type << " index=" << params.device_index
                   << " channel=" << params.can_index << " bitrate="
                   << bitrate << " dbitrate=" << dbitrate);
    return 0;
  }

  void Close() override {
    if (channel_handle_ != nullptr) {
      ZCAN_ResetCAN(*channel_handle_);
      channel_handle_ = nullptr;
    }
    if (device_handle_ != INVALID_DEVICE_HANDLE) {
      ZCAN_CloseDevice(device_handle_);
      device_handle_ = INVALID_DEVICE_HANDLE;
    }
  }

  int Send(const canfd::Frame& frame) override {
    if (channel_handle_ == nullptr) return -1;

    ZCAN_TransmitFD_Data canfd_data;
    memset(&canfd_data, 0, sizeof(canfd_data));

    // Build CAN ID: extended frame + 29-bit ID
    canfd_data.frame.can_id = MAKE_CAN_ID(frame.id, 1, 0, 0);
    canfd_data.frame.len = frame.len;
    // Enable data phase baud rate switching.
    canfd_data.frame.flags = CANFD_BRS;
    memcpy(canfd_data.frame.data, frame.data, frame.len);
    canfd_data.transmit_type = 0;  // normal transmission

    canfd::ArbitrationId arb(frame.id);
    GHAND_LOG_INFO("CH" << can_index_ << " TX "
                  << "ID=0x" << std::hex << frame.id << std::dec << " "
                  << FrameTypeString(arb.frame_type()) << " EXT"
                  << " DLC=" << static_cast<int>(frame.len)
                  << " DATA=" << HexDump(frame.data, frame.len));

    UINT sent = ZCAN_TransmitFD(*channel_handle_, &canfd_data, 1);
    return (sent == 1) ? 0 : -1;
  }

  int Receive(canfd::Frame* frame, int timeout_ms) override {
    if (channel_handle_ == nullptr) return -1;

    ZCAN_ReceiveFD_Data rx;
    memset(&rx, 0, sizeof(rx));

    // wait_time: -1 means blocking, 0 means non-blocking, >0 means timeout in
    // milliseconds.
    int zlg_wait = timeout_ms;
    if (timeout_ms < 0) zlg_wait = -1;

    UINT received = ZCAN_ReceiveFD(*channel_handle_, &rx, 1, zlg_wait);
    if (received == 0) return -1;

    frame->id = GET_ID(rx.frame.can_id);
    frame->len = rx.frame.len;
    memcpy(frame->data, rx.frame.data, rx.frame.len);
    frame->is_fd = true;
    frame->is_extended = IS_EFF(rx.frame.can_id);

    canfd::ArbitrationId arb(frame->id);
    // GHAND_LOG_INFO("CH" << can_index_ << " RX "
    //           << "ID=0x" << std::hex << frame->id << std::dec
    //           << " " << FrameTypeString(arb.frame_type())
    //           << (frame->is_extended ? " EXT" : " STD")
    //           << " DLC=" << static_cast<int>(frame->len)
    //           << " DATA=" << HexDump(frame->data, frame->len));

    return 0;
  }

  std::map<std::string, std::string> EnumerateAdapters() override {
    std::map<std::string, std::string> adapters;

#ifdef _WIN32
    // Prefer enumerating ZLG devices via COM ports
    auto ports = EnumerateComPorts();
    for (const auto& port : ports) {
      if (!IsZLGVID(port.vid)) continue;
      std::string desc = "ZLG CANFD";
      desc += " (" + port.port_name + ")";
      adapters[port.port_name + ":0"] = desc + " Ch0";
      adapters[port.port_name + ":1"] = desc + " Ch1";
    }
    if (!adapters.empty()) {
      return adapters;
    }
#endif

    // Fallback: enumerate actually connected ZLG CANFD devices directly
    for (int dev_type : kCommonZLGDeviceTypes) {
      for (int dev_index = 0; dev_index < 8; ++dev_index) {
        long handle = ZCAN_OpenDevice(dev_type, dev_index, 0);
        if (handle == INVALID_DEVICE_HANDLE) continue;

        if (ZCAN_IsDeviceOnLine(handle) != STATUS_ONLINE) {
          ZCAN_CloseDevice(handle);
          continue;
        }

        ZCAN_DEVICE_INFO info;
        memset(&info, 0, sizeof(info));
        if (ZCAN_GetDeviceInf(handle, info) == STATUS_OK) {
          std::string hw_type =
              ByteArrayString(info.str_hw_Type, sizeof(info.str_hw_Type));
          std::string serial =
              ByteArrayString(info.str_Serial_Num,
                              sizeof(info.str_Serial_Num));
          for (int ch = 0; ch < info.can_Num; ++ch) {
            std::string key = std::to_string(dev_type) + ":" +
                              std::to_string(dev_index) + ":" +
                              std::to_string(ch);
            std::string desc = hw_type + " [Dev" + std::to_string(dev_index) +
                               " Ch" + std::to_string(ch) + " S/N:" + serial +
                               "]";
            adapters[key] = desc;
          }
        }
        ZCAN_CloseDevice(handle);
      }
    }
    return adapters;
  }

  bool IsOpen() const override {
    return device_handle_ != INVALID_DEVICE_HANDLE &&
           channel_handle_ != nullptr;
  }

 private:
  int ConfigureChannelProperties(const ZLGOpenParams& params, uint32_t bitrate,
                                 uint32_t dbitrate) {
    IProperty* prop = GetIProperty(device_handle_);
    if (prop == nullptr) return 0;

    std::string abit = std::to_string(bitrate);
    std::string dbit = std::to_string(dbitrate);
    std::string ch_str = std::to_string(params.can_index);

    std::string abit_path = ch_str + "/canfd_abit_baud_rate";
    std::string dbit_path = ch_str + "/canfd_dbit_baud_rate";
    if (1 != prop->SetValue(abit_path.c_str(), abit.c_str())) {
      return -3;
    }
    if (1 != prop->SetValue(dbit_path.c_str(), dbit.c_str())) {
      return -4;
    }

    if (params.abit_sample_point > 0) {
      std::string abit_sp_path = ch_str + "/canfd_abit_sample_point";
      prop->SetValue(abit_sp_path.c_str(),
                     std::to_string(params.abit_sample_point).c_str());
    }
    if (params.dbit_sample_point > 0) {
      std::string dbit_sp_path = ch_str + "/canfd_dbit_sample_point";
      prop->SetValue(dbit_sp_path.c_str(),
                     std::to_string(params.dbit_sample_point).c_str());
    }

    std::string res_path = ch_str + "/initenal_resistance";
    if (1 != prop->SetValue(res_path.c_str(), "1")) {
      GHAND_LOG_WARNING("Failed to set initenal_resistance for channel "
                        << params.can_index << ", try internal_resistance");
      res_path = ch_str + "/internal_resistance";
      if (1 != prop->SetValue(res_path.c_str(), "1")) {
        GHAND_LOG_WARNING("Failed to set internal_resistance for channel "
                          << params.can_index);
      }
    }
    return 0;
  }

  int InitializeChannel(const ZLGOpenParams& params) {
    ZCAN_CHANNEL_INIT_CONFIG config;
    memset(&config, 0, sizeof(config));
    config.can_type = TYPE_CANFD;
    config.canfd.abit_timing = 0;
    config.canfd.dbit_timing = 0;
    config.canfd.filter = 0;  // receive all frames
    config.canfd.mode = 0;    // normal mode

    channel_handle_ =
        ZCAN_InitCAN(device_handle_, static_cast<UINT>(params.can_index),
                     config);
    if (channel_handle_ == nullptr) {
      GHAND_LOG_ERROR("ZCAN_InitCAN failed for channel " << params.can_index);
      ZCAN_CloseDevice(device_handle_);
      device_handle_ = INVALID_DEVICE_HANDLE;
      return -2;
    }

    if (ZCAN_StartCAN(*channel_handle_) != STATUS_OK) {
      GHAND_LOG_ERROR("ZCAN_StartCAN failed, error=" << GetLastError());
      ZCAN_ResetCAN(*channel_handle_);
      ZCAN_CloseDevice(device_handle_);
      device_handle_ = INVALID_DEVICE_HANDLE;
      channel_handle_ = nullptr;
      return -3;
    }
    return 0;
  }
  long device_handle_ = INVALID_DEVICE_HANDLE;
  Handle_chl* channel_handle_ = nullptr;
  int can_index_ = 0;
};

std::unique_ptr<CANFDDriver> CreateZLGDriver() {
  return std::unique_ptr<CANFDDriver>(new ZLGDriver());
}

}  // namespace internal
}  // namespace ghand
