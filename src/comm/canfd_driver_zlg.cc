#include <algorithm>
#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "canfd_driver.h"
#include "ghand/logging.h"
#include "logging_macros.h"

#ifdef _WIN32
#include <cfgmgr32.h>
#include <devguid.h>
#include <setupapi.h>
#include <windows.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#endif

// ZLG CAN 二次开发库头文件
#include "zlgcan.h"

// 利用编译时自动生成的 zlgcan_device_types.inc 构建宏名到数值的映射表，
// 无需在代码中手动维护映射关系。
static const std::map<std::string, int>& GetZLGDeviceTypeMap() {
  static const std::map<std::string, int> kMap = [] {
    std::map<std::string, int> m;
#define XX(name) m[#name] = name;
#include "zlgcan_device_types.inc"
#undef XX
    return m;
  }();
  return kMap;
}

// 已知 ZLG CANFD 设备类型列表
static const int kCommonZLGDeviceTypes[] = {
    ZCAN_USBCANFD_100U,      ZCAN_USBCANFD_200U,        ZCAN_USBCANFD_400U,
    ZCAN_USBCANFD_MINI,      ZCAN_USBCANFD_800U,        ZCAN_PCIE_CANFD_100U,
    ZCAN_PCIE_CANFD_200U,    ZCAN_PCIE_CANFD_400U,      ZCAN_PCIE_CANFD_100U_EX,
    ZCAN_PCIE_CANFD_400U_EX, ZCAN_PCIE_CANFD_200U_MINI, ZCAN_PCIE_CANFD_200U_M2,
    ZCAN_CANFDCOM_100IE,
};

namespace ghand {
namespace internal {

/**
 * @brief ZLG CANFD 驱动实现
 *
 * 封装 ZLG zlgcan.dll 的 C API，提供跨平台统一的 CANFD 操作接口。
 *
 * Open() 的 name 参数格式："DeviceType:DeviceIndex:CanIndex"
 *  DeviceType 为 zlgcan.h 中宏定义对应的数字（如 "42" 对应
 * ZCAN_USBCANFD_100U）。 例如："42:0:0"
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

#ifdef _WIN32
struct ComPortEntry {
  std::string port_name;      // 如 "COM3"
  std::string friendly_name;  // 友好名称，如 "ZLG USBCANFD-100U (COM3)"
  std::string device_desc;    // 设备描述，通常含型号
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
    char hwId[256] = {};
    DWORD dataType = 0, reqSize = 0;
    if (!SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_HARDWAREID, &dataType,
            reinterpret_cast<PBYTE>(hwId), sizeof(hwId), &reqSize)) {
      continue;
    }

    int vid = 0, pid = 0;
    char* vid_pos = strstr(hwId, "VID_");
    char* pid_pos = strstr(hwId, "PID_");
    if (vid_pos && pid_pos) {
      vid = static_cast<int>(strtol(vid_pos + 4, nullptr, 16));
      pid = static_cast<int>(strtol(pid_pos + 4, nullptr, 16));
    }
    if (vid == 0) continue;

    char friendly[256] = {};
    reqSize = 0;
    SetupDiGetDeviceRegistryPropertyA(
        hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, &dataType,
        reinterpret_cast<PBYTE>(friendly), sizeof(friendly), &reqSize);

    char device_desc[256] = {};
    reqSize = 0;
    SetupDiGetDeviceRegistryPropertyA(
        hDevInfo, &devInfoData, SPDRP_DEVICEDESC, &dataType,
        reinterpret_cast<PBYTE>(device_desc), sizeof(device_desc), &reqSize);

    std::string port_name;
    std::string friendly_str(friendly);
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
        entry.device_desc = device_desc;
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
 * @brief 根据 USB VID/PID 确定 ZLG device_type
 *
 * 映射表可根据实际连接的 ZLG 设备补充。
 * 常见 USBCANFD 系列 VID 为 0x3562，不同型号对应不同 PID。
 */
static int GetZLGDeviceTypeFromVIDPID(int vid, int pid) {
  static const std::map<std::pair<int, int>, int> kVIDPIDToType = {
      // ZLG USBCANFD 系列 (VID=0x3562)
      {{0x3562, 0x0101}, ZCAN_USBCANFD_100U},
      {{0x3562, 0x0102}, ZCAN_USBCANFD_200U},
      {{0x3562, 0x0103}, ZCAN_USBCANFD_400U},
      {{0x3562, 0x0104}, ZCAN_USBCANFD_MINI},
      {{0x3562, 0x0105}, ZCAN_USBCANFD_800U},
      // 如有其他 VID/PID 组合请在此补充
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
    LOG_ERROR("COM port not found: " << port_name);
    return false;
  }
  if (!IsZLGVID(target_vid)) {
    LOG_WARNING("COM port " << port_name << " VID=0x" << std::hex << target_vid
                            << " does not look like a ZLG device");
  }

  // 辅助 lambda：尝试某一 device_type 的 0~7 索引，找到第一个能打开即返回
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

  // 2. 名称未识别，尝试 VID/PID 映射表
  int vidpid_type = GetZLGDeviceTypeFromVIDPID(target_vid, target_pid);
  if (vidpid_type != 0 && try_open(vidpid_type)) {
    LOG_INFO("Resolved " << port_name << " -> ZLG device type="
                         << *out_device_type << " index=" << *out_device_index
                         << " (from VID/PID)");
    return true;
  }
  LOG_ERROR("Failed to find online ZLG device for " << port_name);
  return false;
}
#endif  // _WIN32

class ZLGDriver : public CANFDDriver {
 public:
  ZLGDriver() = default;
  ~ZLGDriver() override { Close(); }

  int Open(const std::string& name, uint32_t bitrate,
           uint32_t dbitrate) override {
    Close();

    // 解析 name:
    // "DeviceType:DeviceIndex:CanIndex[:AbitSamplePoint:DbitSamplePoint]" 或
    // "COM3" 等 COM 端口名（Windows 下自动解析为 ZLG 设备参数）
    int device_type = 48;  // 默认值
    int device_index = 0;
    int can_index = 0;
    int abit_sample_point = 75;  // 0 表示不设置，使用设备默认
    int dbit_sample_point = 80;

    std::string resolved_name = name;

#ifdef _WIN32
    // 检测 COM 端口名（如 "COM3" 或 "com3"）
    if (name.size() >= 3) {
      std::string prefix = name.substr(0, 3);
      std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::toupper);
      if (prefix == "COM") {
        int resolved_type = 0, resolved_index = 0;
        if (ResolveComPortToZLGDevice(name, &resolved_type, &resolved_index)) {
          resolved_name = std::to_string(resolved_type) + ":" +
                          std::to_string(resolved_index) + ":0";
          LOG_INFO("Resolved " << name << " to ZLG device " << resolved_name);
        } else {
          LOG_ERROR("Failed to resolve COM port: " << name);
          return -1;
        }
      }
    }
#endif

    std::istringstream iss(resolved_name);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(iss, part, ':')) {
      parts.push_back(part);
    }

    if (parts.size() >= 1) {
      const auto& type_map = GetZLGDeviceTypeMap();
      auto it = type_map.find(parts[0]);
      if (it != type_map.end()) {
        device_type = it->second;
      } else {
        try {
          device_type = std::stoi(parts[0]);
        } catch (...) {
          LOG_ERROR("Invalid ZLG device type: "
                    << parts[0]
                    << ". Please use the macro name defined in zlgcan.h (e.g., "
                       "\"ZCAN_USBCANFD_100U:0:0\") "
                    << "or the numeric value (e.g., \"42:0:0\").");
          return -1;
        }
      }
    }
    if (parts.size() >= 2) {
      try {
        device_index = std::stoi(parts[1]);
      } catch (...) {
      }
    }
    if (parts.size() >= 3) {
      try {
        can_index = std::stoi(parts[2]);
      } catch (...) {
      }
    }
    if (parts.size() >= 4) {
      try {
        abit_sample_point = std::stoi(parts[3]);
      } catch (...) {
      }
    }
    if (parts.size() >= 5) {
      try {
        dbit_sample_point = std::stoi(parts[4]);
      } catch (...) {
      }
    }
    can_index_ = can_index;

    // 1.打开设备
    device_handle_ = ZCAN_OpenDevice(device_type, device_index, 0);
    if (device_handle_ == INVALID_DEVICE_HANDLE) {
      LOG_ERROR("ZCAN_OpenDevice failed: type=" << device_type
                                                << " index=" << device_index
                                                << " err=" << GetLastError());
      return -1;
    }

    // 使用属性表设置波特率与采样点（部分 OEM 设备必须通过属性表配置）
    IProperty* prop = GetIProperty(device_handle_);
    if (prop) {
      // 2.设置CANFD标准
      std::string abit = std::to_string(bitrate);
      std::string dbit = std::to_string(dbitrate);
      std::string ch_str = std::to_string(can_index);

      // 路径格式: "<channel>/canfd_abit_baud_rate"
      std::string abit_path = ch_str + "/canfd_abit_baud_rate";
      std::string dbit_path = ch_str + "/canfd_dbit_baud_rate";

      if (1 != prop->SetValue(abit_path.c_str(), abit.c_str())) {
        return -3;
      }
      if (1 != prop->SetValue(dbit_path.c_str(), dbit.c_str())) {
        return -4;
      }

      // 3.设置采样点
      if (abit_sample_point > 0) {
        std::string abit_sp_path = ch_str + "/canfd_abit_sample_point";
        int c = prop->SetValue(abit_sp_path.c_str(),
                               std::to_string(abit_sample_point).c_str());
      }
      if (dbit_sample_point > 0) {
        std::string dbit_sp_path = ch_str + "/canfd_dbit_sample_point";
        int d = prop->SetValue(dbit_sp_path.c_str(),
                               std::to_string(dbit_sample_point).c_str());
      }

      // 4.设置终端电阻（不阻塞启动，失败后仅记录日志）
      std::string res_path = ch_str + "/initenal_resistance";
      if (1 != prop->SetValue(res_path.c_str(), "0")) {
        LOG_WARNING("Failed to set initenal_resistance for channel "
                    << can_index << ", try internal_resistance");
        res_path = ch_str + "/internal_resistance";
        if (1 != prop->SetValue(res_path.c_str(), "0")) {
          LOG_WARNING("Failed to set internal_resistance for channel "
                      << can_index);
        }
      }
    }

    // 5.初始化通道
    ZCAN_CHANNEL_INIT_CONFIG config;
    memset(&config, 0, sizeof(config));
    config.can_type = TYPE_CANFD;
    // abit_timing/dbit_timing 已经在属性表中配置，此处填 0 避免冲突
    config.canfd.abit_timing = 0;
    config.canfd.dbit_timing = 0;
    config.canfd.filter = 0;  // 接收所有帧
    config.canfd.mode = 0;    // 正常模式

    channel_handle_ =
        ZCAN_InitCAN(device_handle_, static_cast<UINT>(can_index), config);
    if (channel_handle_ == nullptr) {
      LOG_ERROR("ZCAN_InitCAN failed for channel " << can_index);
      ZCAN_CloseDevice(device_handle_);
      device_handle_ = INVALID_DEVICE_HANDLE;
      return -2;
    }

    // 6.启动通道
    if (ZCAN_StartCAN(*channel_handle_) != STATUS_OK) {
      LOG_ERROR("ZCAN_StartCAN failed, error=" << GetLastError());
      ZCAN_ResetCAN(*channel_handle_);
      ZCAN_CloseDevice(device_handle_);
      device_handle_ = INVALID_DEVICE_HANDLE;
      channel_handle_ = nullptr;
      return -3;
    }

    LOG_INFO("ZLG CANFD opened: type="
             << device_type << " index=" << device_index
             << " channel=" << can_index << " bitrate=" << bitrate
             << " dbitrate=" << dbitrate);
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

    // 构建 CAN ID：扩展帧 + 29-bit ID
    canfd_data.frame.can_id = MAKE_CAN_ID(frame.id, 1, 0, 0);
    canfd_data.frame.len = frame.len;
    canfd_data.frame.flags = CANFD_BRS;  // 启用数据段波特率切换
    memcpy(canfd_data.frame.data, frame.data, frame.len);
    canfd_data.transmit_type = 0;  // 正常发送

    canfd::ArbitrationId arb(frame.id);
    LOG_INFO("CH" << can_index_ << " TX "
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

    // wait_time: -1 表示阻塞等待，0 表示非阻塞，>0 表示超时毫秒
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
    // LOG_INFO("CH" << can_index_ << " RX "
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
    // 优先通过 COM 端口枚举 ZLG 设备
    auto ports = EnumerateComPorts();
    for (const auto& port : ports) {
      if (!IsZLGVID(port.vid)) continue;
      std::string desc = "ZLG CANFD";
      desc += " (" + port.port_name + ")";
      adapters[port.port_name] = desc;
    }
    if (!adapters.empty()) {
      return adapters;
    }
#endif

    // Fallback：直接枚举实际连接的 ZLG CANFD 设备
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
          std::string hw_type(reinterpret_cast<char*>(info.str_hw_Type));
          std::string serial(reinterpret_cast<char*>(info.str_Serial_Num));
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
  long device_handle_ = INVALID_DEVICE_HANDLE;
  Handle_chl* channel_handle_ = nullptr;
  int can_index_ = 0;
};

std::unique_ptr<CANFDDriver> CreateZLGDriver() {
  return std::unique_ptr<CANFDDriver>(new ZLGDriver());
}

}  // namespace internal
}  // namespace ghand
