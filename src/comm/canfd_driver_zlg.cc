#include "canfd_driver.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <ntddser.h>
#pragma comment(lib, "setupapi.lib")
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "ghand/logging.h"
#include "logging_macros.h"

namespace ghand {
namespace internal {
namespace {

const uint8_t kConfigHead[] = {0x49, 0x3B};
const uint8_t kConfigTail[] = {0x45, 0x2E};
const uint8_t kCanfdHead = 0x5A;
const uint8_t kCanfdTail = 0xA5;
const uint8_t kHeartbeat12Channel = 0xFF;
const uint8_t kHeartbeat4Channel = 0xFE;
const int kDefaultSerialBaudrate = 2000000;

std::map<uint32_t, uint8_t> CommonBitrateCodes() {
  return {{1000000, 0x0}, {800000, 0x1}, {500000, 0x2}, {400000, 0x3},
          {250000, 0x4},  {200000, 0x5}, {125000, 0x6}, {100000, 0x7},
          {50000, 0x8},   {40000, 0x9},  {25000, 0xA},  {20000, 0xB},
          {15000, 0xC},   {10000, 0xD},  {5000, 0xE}};
}

std::map<uint32_t, uint8_t> CommonDataBitrateCodes() {
  return {{5000000, 0x0}, {4000000, 0x1}, {2000000, 0x2}, {1000000, 0x3},
          {800000, 0x4},  {500000, 0x5},  {400000, 0x6},  {250000, 0x7},
          {200000, 0x8},  {125000, 0x9},  {100000, 0xA}};
}

uint8_t DlcLength(uint8_t len) {
  static const uint8_t kLengths[] = {0,  1,  2,  3,  4,  5,  6,  7,
                                     8,  12, 16, 20, 24, 32, 48, 64};
  for (uint8_t value : kLengths) {
    if (value >= len) return value;
  }
  return 64;
}

int PayloadLengthFromDlc(uint8_t dlc) {
  if (dlc <= 8) return dlc;
  switch (dlc) {
    case 12:
    case 16:
    case 20:
    case 24:
    case 32:
    case 48:
    case 64:
      return dlc;
    default:
      return -1;
  }
}

void AddUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) return;
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

#ifdef _WIN32
std::string NormalizeWindowsComPort(const std::string& name) {
  if (name.size() > 3 &&
      (name[0] == 'C' || name[0] == 'c') &&
      (name[1] == 'O' || name[1] == 'o') &&
      (name[2] == 'M' || name[2] == 'm')) {
    int port_number = std::atoi(name.c_str() + 3);
    if (port_number >= 10) return "\\\\.\\" + name;
  }
  return name;
}

std::string ByteArrayString(const BYTE* data, size_t size) {
  std::string result;
  for (size_t i = 0; i < size && data[i] != 0; ++i) {
    result.push_back(static_cast<char>(data[i]));
  }
  return result;
}

bool IsZqwlVidPid(const std::string& hardware_id) {
  std::string upper = hardware_id;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  return upper.find("VID_3562") != std::string::npos &&
         (upper.find("PID_0100") != std::string::npos ||
          upper.find("PID_0101") != std::string::npos ||
          upper.find("PID_0105") != std::string::npos);
}
#else
speed_t TermiosBaud(int baudrate) {
  switch (baudrate) {
    case 9600:
      return B9600;
    case 115200:
      return B115200;
#ifdef B1000000
    case 1000000:
      return B1000000;
#endif
#ifdef B2000000
    case 2000000:
      return B2000000;
#endif
    default:
#ifdef B2000000
      return B2000000;
#else
      return B115200;
#endif
  }
}

void GlobSimple(const std::string& dir_path, const std::string& prefix,
                std::vector<std::string>* values) {
  DIR* dir = opendir(dir_path.c_str());
  if (!dir) return;
  struct dirent* entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name(entry->d_name);
    if (name.find(prefix) == 0) {
      AddUnique(values, dir_path + "/" + name);
    }
  }
  closedir(dir);
}

void AddSerialByIdZqwl(std::vector<std::string>* values) {
  const std::string dir_path = "/dev/serial/by-id";
  DIR* dir = opendir(dir_path.c_str());
  if (!dir) return;
  struct dirent* entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name(entry->d_name);
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper.find("ZQWL") != std::string::npos) {
      AddUnique(values, dir_path + "/" + name);
    }
  }
  closedir(dir);
}
#endif

}  // namespace

class ZqwlSerialCanfdDriver : public CANFDDriver {
 public:
  ZqwlSerialCanfdDriver() = default;
  ~ZqwlSerialCanfdDriver() override { Close(); }

  int Open(const std::string& name, uint32_t bitrate,
           uint32_t dbitrate) override {
    Close();
    can_index_ = 0;
    std::string port_name = name;
    size_t colon = port_name.find(':');
    if (colon != std::string::npos) {
      std::string suffix = port_name.substr(colon + 1);
      port_name = port_name.substr(0, colon);
      if (!suffix.empty()) can_index_ = std::atoi(suffix.c_str());
    }
    if (port_name.empty()) {
      auto adapters = EnumerateAdapters();
      if (adapters.empty()) {
        GHAND_LOG_ERROR("No ZQWL CANFD CDC serial adapters found");
        return -1;
      }
      port_name = adapters.begin()->first;
    }

    if (!OpenSerial(port_name)) return -1;
    if (!ConfigureChannel(bitrate, dbitrate)) {
      Close();
      return -2;
    }
    port_name_ = port_name;
    GHAND_LOG_INFO("ZQWL CANFD serial opened: " << port_name
                                               << " channel=" << can_index_
                                               << " bitrate=" << bitrate
                                               << " dbitrate=" << dbitrate);
    return 0;
  }

  void Close() override {
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }
#endif
  }

  int Send(const canfd::Frame& frame) override {
    if (!IsOpen() || frame.len > 64) return -1;
    uint8_t info1 = DlcLength(frame.len) & 0x7F;
    info1 |= static_cast<uint8_t>((can_index_ & 0x01) << 7);
    uint8_t info2 = 0x01;  // BRS enabled.
    info2 |= static_cast<uint8_t>(((can_index_ >> 1) & 0x03) << 3);
    info2 |= 0x04;  // Extended frame.
    uint32_t frame_id = (frame.id & 0x1FFFFFFF) | 0x80000000;

    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(frame.len) + 8);
    raw.push_back(kCanfdHead);
    raw.push_back(info1);
    raw.push_back(info2);
    raw.push_back(static_cast<uint8_t>((frame_id >> 24) & 0xFF));
    raw.push_back(static_cast<uint8_t>((frame_id >> 16) & 0xFF));
    raw.push_back(static_cast<uint8_t>((frame_id >> 8) & 0xFF));
    raw.push_back(static_cast<uint8_t>(frame_id & 0xFF));
    raw.insert(raw.end(), frame.data, frame.data + frame.len);
    raw.push_back(kCanfdTail);
    return WriteAll(raw.data(), raw.size()) ? 0 : -1;
  }

  int Receive(canfd::Frame* frame, int timeout_ms) override {
    if (!IsOpen() || frame == nullptr) return -1;
    uint8_t byte = 0;
    while (ReadByte(&byte, timeout_ms)) {
      if (byte != kCanfdHead) continue;

      uint8_t info1 = 0;
      if (!ReadByte(&info1, timeout_ms)) continue;
      if (info1 == kHeartbeat12Channel || info1 == kHeartbeat4Channel) {
        int heartbeat_len = info1 == kHeartbeat12Channel ? 15 : 30;
        std::vector<uint8_t> ignored(static_cast<size_t>(heartbeat_len));
        ReadBytes(ignored.data(), ignored.size(), timeout_ms);
        continue;
      }

      uint8_t info2_and_id[5] = {0};
      if (!ReadBytes(info2_and_id, sizeof(info2_and_id), timeout_ms)) continue;
      int payload_len = PayloadLengthFromDlc(info1 & 0x7F);
      if (payload_len < 0 || payload_len > 64) continue;

      std::vector<uint8_t> payload(static_cast<size_t>(payload_len) + 1);
      if (!ReadBytes(payload.data(), payload.size(), timeout_ms)) continue;
      if (payload.back() != kCanfdTail) continue;

      uint8_t info2 = info2_and_id[0];
      uint32_t frame_id = (static_cast<uint32_t>(info2_and_id[1]) << 24) |
                          (static_cast<uint32_t>(info2_and_id[2]) << 16) |
                          (static_cast<uint32_t>(info2_and_id[3]) << 8) |
                          static_cast<uint32_t>(info2_and_id[4]);
      bool is_canfd = (frame_id & 0x80000000) != 0;
      bool is_extended = (info2 & 0x04) != 0;
      if (!is_canfd || !is_extended) continue;

      frame->id = frame_id & 0x1FFFFFFF;
      frame->len = static_cast<uint8_t>(payload_len);
      memcpy(frame->data, payload.data(), static_cast<size_t>(payload_len));
      frame->is_fd = true;
      frame->is_extended = true;
      return 0;
    }
    return -1;
  }

  std::map<std::string, std::string> EnumerateAdapters() override {
    std::vector<std::string> ports;
#ifdef _WIN32
    HDEVINFO hDevInfo =
        SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo != INVALID_HANDLE_VALUE) {
      SP_DEVINFO_DATA devInfoData;
      devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
      for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
        BYTE hardware[256] = {};
        DWORD dataType = 0, reqSize = 0;
        if (!SetupDiGetDeviceRegistryPropertyA(
                hDevInfo, &devInfoData, SPDRP_HARDWAREID, &dataType,
                hardware, sizeof(hardware), &reqSize)) {
          continue;
        }
        if (!IsZqwlVidPid(ByteArrayString(hardware, sizeof(hardware)))) {
          continue;
        }
        BYTE friendly[256] = {};
        SetupDiGetDeviceRegistryPropertyA(
            hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, &dataType,
            friendly, sizeof(friendly), &reqSize);
        std::string text = ByteArrayString(friendly, sizeof(friendly));
        size_t lp = text.find('(');
        size_t rp = text.find(')', lp);
        if (lp != std::string::npos && rp != std::string::npos && rp > lp + 1) {
          AddUnique(&ports, text.substr(lp + 1, rp - lp - 1));
        }
      }
      SetupDiDestroyDeviceInfoList(hDevInfo);
    }
#else
    AddSerialByIdZqwl(&ports);
    GlobSimple("/dev", "ttyACM", &ports);
#endif

    std::map<std::string, std::string> adapters;
    for (const auto& port : ports) {
      adapters[port + ":0"] = "ZQWL CANFD CDC Ch0";
    }
    return adapters;
  }

  bool IsOpen() const override {
#ifdef _WIN32
    return handle_ != INVALID_HANDLE_VALUE;
#else
    return fd_ >= 0;
#endif
  }

 private:
  bool OpenSerial(const std::string& port_name) {
#ifdef _WIN32
    std::string normalized = NormalizeWindowsComPort(port_name);
    handle_ = CreateFileA(normalized.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                          nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
      GHAND_LOG_ERROR("Failed to open CANFD serial port: " << port_name);
      return false;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) return false;
    dcb.BaudRate = kDefaultSerialBaudrate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(handle_, &dcb)) return false;

    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 20;
    timeouts.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(handle_, &timeouts);
    return true;
#else
    fd_ = open(port_name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
      GHAND_LOG_ERROR("Failed to open CANFD serial port "
                      << port_name << ": " << strerror(errno));
      return false;
    }
    termios tio;
    memset(&tio, 0, sizeof(tio));
    if (tcgetattr(fd_, &tio) != 0) return false;
    cfmakeraw(&tio);
    speed_t baud = TermiosBaud(kDefaultSerialBaudrate);
    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    if (tcsetattr(fd_, TCSANOW, &tio) != 0) return false;
    tcflush(fd_, TCIOFLUSH);
    return true;
#endif
  }

  bool ConfigureChannel(uint32_t bitrate, uint32_t dbitrate) {
    auto bitrates = CommonBitrateCodes();
    auto dbitrates = CommonDataBitrateCodes();
    auto abit_it = bitrates.find(bitrate);
    auto dbit_it = dbitrates.find(dbitrate);
    if (abit_it == bitrates.end() || dbit_it == dbitrates.end()) {
      GHAND_LOG_ERROR("Unsupported ZQWL CANFD bitrate pair");
      return false;
    }
    uint8_t bitrate_code =
        static_cast<uint8_t>((abit_it->second << 4) | dbit_it->second);
    uint8_t params[16] = {0};
    params[0] = static_cast<uint8_t>(can_index_ & 0xFF);
    params[1] = 0x00;
    params[2] = bitrate_code;
    if (!WriteConfig(0x42, params, sizeof(params))) return false;

    uint8_t control[16] = {0};
    control[0] = 0x01;
    if (can_index_ == 0) {
      control[2] = 0x01;
    } else if (can_index_ == 1) {
      control[3] = 0x01;
    } else {
      GHAND_LOG_ERROR("ZQWL serial backend supports CAN0/CAN1 only");
      return false;
    }
    return WriteConfig(0x44, control, sizeof(control));
  }

  bool WriteConfig(uint8_t func_code, const uint8_t* payload, size_t size) {
    uint8_t cmd[22] = {0};
    cmd[0] = kConfigHead[0];
    cmd[1] = kConfigHead[1];
    cmd[2] = func_code;
    cmd[3] = 0x57;
    size_t copy_size = size < 16 ? size : 16;
    memcpy(cmd + 4, payload, copy_size);
    cmd[20] = kConfigTail[0];
    cmd[21] = kConfigTail[1];
    return WriteAll(cmd, sizeof(cmd));
  }

  bool WriteAll(const uint8_t* data, size_t size) {
#ifdef _WIN32
    size_t written_total = 0;
    while (written_total < size) {
      DWORD written = 0;
      if (!WriteFile(handle_, data + written_total,
                     static_cast<DWORD>(size - written_total), &written,
                     nullptr) ||
          written == 0) {
        return false;
      }
      written_total += written;
    }
    return true;
#else
    size_t written_total = 0;
    while (written_total < size) {
      ssize_t written = write(fd_, data + written_total, size - written_total);
      if (written < 0) {
        if (errno == EINTR || errno == EAGAIN) continue;
        return false;
      }
      if (written == 0) return false;
      written_total += static_cast<size_t>(written);
    }
    return true;
#endif
  }

  bool ReadByte(uint8_t* byte, int timeout_ms) {
    return ReadBytes(byte, 1, timeout_ms);
  }

  bool ReadBytes(uint8_t* data, size_t size, int timeout_ms) {
    size_t read_total = 0;
    while (read_total < size) {
#ifdef _WIN32
      DWORD read_count = 0;
      if (!ReadFile(handle_, data + read_total,
                    static_cast<DWORD>(size - read_total), &read_count,
                    nullptr)) {
        return false;
      }
      if (read_count == 0) return false;
      read_total += read_count;
#else
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(fd_, &readfds);
      timeval tv;
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      int ready = select(fd_ + 1, &readfds, nullptr, nullptr, &tv);
      if (ready <= 0) return false;
      ssize_t count = read(fd_, data + read_total, size - read_total);
      if (count < 0) {
        if (errno == EINTR || errno == EAGAIN) continue;
        return false;
      }
      if (count == 0) return false;
      read_total += static_cast<size_t>(count);
#endif
    }
    return true;
  }

  std::string port_name_;
  int can_index_ = 0;
#ifdef _WIN32
  HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
  int fd_ = -1;
#endif
};

std::unique_ptr<CANFDDriver> CreateCANFDDriver() {
  return std::unique_ptr<CANFDDriver>(new ZqwlSerialCanfdDriver());
}

}  // namespace internal
}  // namespace ghand
