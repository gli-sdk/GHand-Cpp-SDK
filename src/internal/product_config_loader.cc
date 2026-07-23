#include "product_config_loader.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <sys/stat.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "ghand/logging.h"
#include "logging_macros.h"

namespace ghand {
namespace internal {

std::string ProductTypeToFileName(ProductType type) {
  switch (type) {
    case ProductType::G5:
      return "xiaoyao_hand.json";
    case ProductType::L1:
      return "l1_hand.json";
    case ProductType::AUTO:
      return "";  // AUTO mode defers config loading
    default:
      return "";
  }
}

std::string StripComments(const std::string& content) {
  std::string result;
  result.reserve(content.size());
  bool in_string = false;
  bool escaped = false;
  for (size_t i = 0; i < content.size(); ++i) {
    char c = content[i];
    if (escaped) {
      escaped = false;
      result.push_back(c);
      continue;
    }
    if (c == '\\') {
      escaped = true;
      result.push_back(c);
      continue;
    }
    if (c == '"') {
      in_string = !in_string;
      result.push_back(c);
      continue;
    }
    if (!in_string && c == '/' && i + 1 < content.size() &&
        content[i + 1] == '/') {
      while (i < content.size() && content[i] != '\n') {
        ++i;
      }
      if (i < content.size()) {
        result.push_back('\n');
      }
    } else {
      result.push_back(c);
    }
  }
  return result;
}

std::string GetSdkRootFromModule() {
#ifdef _WIN32
  HMODULE hMod = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          (LPCSTR)&GetSdkRootFromModule, &hMod)) {
    return "";
  }
  char path[MAX_PATH];
  if (GetModuleFileNameA(hMod, path, MAX_PATH) == 0) {
    return "";
  }
  std::string dll_path(path);
  size_t last_slash = dll_path.find_last_of("\\/");
  if (last_slash != std::string::npos) {
    std::string lib_dir = dll_path.substr(0, last_slash);
    size_t parent_slash = lib_dir.find_last_of("\\/");
    if (parent_slash != std::string::npos) {
      return lib_dir.substr(0, parent_slash + 1);
    }
  }
#else
  Dl_info info;
  if (dladdr(reinterpret_cast<void*>(&GetSdkRootFromModule), &info) == 0 ||
      !info.dli_fname) {
    return "";
  }
  char resolved[PATH_MAX];
  if (realpath(info.dli_fname, resolved)) {
    std::string so_path(resolved);
    size_t last_slash = so_path.find_last_of('/');
    if (last_slash != std::string::npos) {
      std::string lib_dir = so_path.substr(0, last_slash);
      size_t parent_slash = lib_dir.find_last_of('/');
      if (parent_slash != std::string::npos) {
        return lib_dir.substr(0, parent_slash + 1);
      }
    }
  }
#endif
  return "";
}

std::string GetEnvValue(const char* name) {
#ifdef _WIN32
  char* value = nullptr;
  size_t value_size = 0;
  errno_t err = _dupenv_s(&value, &value_size, name);
  if (err != 0 || value == nullptr) {
    return "";
  }

  std::string result(value);
  std::free(value);
  return result;
#else
  const char* value = std::getenv(name);
  return value == nullptr ? "" : std::string(value);
#endif
}

std::vector<std::string> GetConfigSearchPaths() {
  std::vector<std::string> paths;

  std::string path = GetEnvValue("GHAND_SDK_CONFIG");
  if (!path.empty()) {
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
#ifdef _WIN32
      path += '\\';
#else
      path += '/';
#endif
    }
    paths.emplace_back(path);
  }

  std::string sdk_root = GetSdkRootFromModule();
  if (!sdk_root.empty()) {
    paths.emplace_back(sdk_root + "config\\");
  }

  paths.emplace_back("./config/");

#ifdef _WIN32
  std::string program_data = GetEnvValue("PROGRAMDATA");
  if (!program_data.empty()) {
    paths.emplace_back(program_data + "\\ghand-sdk\\config\\");
  }
#else
  paths.emplace_back("/usr/share/ghand-sdk/config/");
  paths.emplace_back("/usr/local/share/ghand-sdk/config/");
#endif

  return paths;
}

JointId JointIdFromString(const std::string& name) {
  if (name == "THUMB_IP") return JointId::THUMB_IP;
  if (name == "THUMB_MCP") return JointId::THUMB_MCP;
  if (name == "THUMB_TMC_FE") return JointId::THUMB_TMC_FE;
  if (name == "THUMB_TMC_AA") return JointId::THUMB_TMC_AA;
  if (name == "THUMB_TMC_PS") return JointId::THUMB_TMC_PS;
  if (name == "FF_DIP") return JointId::FF_DIP;
  if (name == "FF_PIP") return JointId::FF_PIP;
  if (name == "FF_MCP") return JointId::FF_MCP;
  if (name == "FF_MCP_AA") return JointId::FF_MCP_AA;
  if (name == "MF_DIP") return JointId::MF_DIP;
  if (name == "MF_PIP") return JointId::MF_PIP;
  if (name == "MF_MCP") return JointId::MF_MCP;
  if (name == "RF_DIP") return JointId::RF_DIP;
  if (name == "RF_PIP") return JointId::RF_PIP;
  if (name == "RF_MCP") return JointId::RF_MCP;
  if (name == "LF_DIP") return JointId::LF_DIP;
  if (name == "LF_PIP") return JointId::LF_PIP;
  if (name == "LF_MCP") return JointId::LF_MCP;
  GHAND_LOG_WARNING("Unknown joint name: " << name);
  return JointId::NUM_JOINTS;
}

std::string FindConfigFilePath(
    const std::string& file_name,
    const std::vector<std::string>& search_paths) {
  for (const auto& dir : search_paths) {
    std::string full_path = dir + file_name;
    std::ifstream test(full_path);
    if (test.good()) {
      return full_path;
    }
  }
  return "";
}

std::string ReadTextFile(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    return "";
  }

  std::string raw_content((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
  return StripComments(raw_content);
}

bool ParseJsonContent(const std::string& content, nlohmann::json* json) {
  if (json == nullptr) return false;
  nlohmann::json parsed = nlohmann::json::parse(content, nullptr, false);
  if (parsed.is_discarded()) return false;
  *json = parsed;
  return true;
}

std::string JsonStringOrEmpty(const nlohmann::json& j, const char* key) {
  if (!j.contains(key) || !j[key].is_string()) return "";
  return j[key].get<std::string>();
}

bool JsonBoolOrFalse(const nlohmann::json& j, const char* key) {
  if (!j.contains(key) || !j[key].is_boolean()) return false;
  return j[key].get<bool>();
}

int JsonIntOrDefault(const nlohmann::json& j, const char* key,
                     int default_value) {
  if (!j.contains(key) || !j[key].is_number_integer()) return default_value;
  return j[key].get<int>();
}

std::vector<std::string> JsonStringArray(const nlohmann::json& j,
                                         const char* key) {
  std::vector<std::string> values;
  if (!j.contains(key) || !j[key].is_array()) return values;
  for (const auto& item : j[key]) {
    if (item.is_string()) values.push_back(item.get<std::string>());
  }
  return values;
}

std::vector<int> JsonIntArray(const nlohmann::json& j, const char* key) {
  std::vector<int> values;
  if (!j.contains(key) || !j[key].is_array()) return values;
  for (const auto& item : j[key]) {
    if (item.is_number_integer()) values.push_back(item.get<int>());
  }
  return values;
}

void ApplyG5ModbusProfile(ProductConfig* config) {
  config->joint_input_registers.clear();
  for (int id = 0; id < static_cast<int>(JointId::NUM_JOINTS); ++id) {
    config->joint_input_registers[static_cast<JointId>(id)] =
        static_cast<uint16_t>(0x1023 + id * 3);
  }

  config->joint_control_registers = {
      {JointId::THUMB_MCP, 0x0011},
      {JointId::THUMB_TMC_FE, 0x0013},
      {JointId::THUMB_TMC_AA, 0x0015},
      {JointId::THUMB_TMC_PS, 0x0017},
      {JointId::FF_PIP, 0x0019},
      {JointId::FF_MCP, 0x001B},
      {JointId::FF_MCP_AA, 0x001D},
      {JointId::MF_PIP, 0x001F},
      {JointId::MF_MCP, 0x0021},
      {JointId::RF_PIP, 0x0023},
      {JointId::RF_MCP, 0x0025},
      {JointId::LF_PIP, 0x0027},
      {JointId::LF_MCP, 0x0029},
  };

  config->per_joint_mode_control = false;
  config->mode_register = 0x0010;
  config->stop_register = 0x0010;
  config->tactile_control_register = 0x002B;
  config->canfd_connection_timer_register = 0x0031;
  config->canfd_connection_timer_count = 1;
  config->canfd_connection_timer_values = {0x0000};
  config->canfd_connection_delete_register = 0x0030;
  config->canfd_connection_delete_count = 2;
  config->canfd_connection_delete_values = {0x0000, 0x0000};
}

void ApplyL1ModbusProfile(ProductConfig* config) {
  config->joint_input_registers = {
      {JointId::THUMB_MCP, 0x1023},
      {JointId::THUMB_TMC_FE, 0x1026},
      {JointId::THUMB_TMC_AA, 0x1029},
      {JointId::FF_PIP, 0x102C},
      {JointId::FF_MCP, 0x102F},
      {JointId::MF_PIP, 0x1032},
      {JointId::MF_MCP, 0x1035},
      {JointId::RF_PIP, 0x1038},
      {JointId::RF_MCP, 0x103B},
      {JointId::LF_PIP, 0x103E},
      {JointId::LF_MCP, 0x1041},
  };

  config->joint_control_registers = {
      {JointId::THUMB_MCP, 0x0010},
      {JointId::THUMB_TMC_FE, 0x0013},
      {JointId::THUMB_TMC_AA, 0x0016},
      {JointId::FF_PIP, 0x0019},
      {JointId::FF_MCP, 0x001C},
      {JointId::MF_PIP, 0x001F},
      {JointId::MF_MCP, 0x0022},
      {JointId::RF_PIP, 0x0025},
      {JointId::RF_MCP, 0x0028},
      {JointId::LF_PIP, 0x002B},
      {JointId::LF_MCP, 0x002E},
  };

  config->per_joint_mode_control = true;
  config->mode_register = -1;
  config->stop_register = -1;
  config->tactile_control_register = 0x0031;
  config->canfd_connection_timer_register = 0x0037;
  config->canfd_connection_timer_count = 2;
  config->canfd_connection_timer_values = {0x0000, 0x0000};
  config->canfd_connection_delete_register = 0x0036;
  config->canfd_connection_delete_count = 3;
  config->canfd_connection_delete_values = {0x0000, 0x0000, 0x0000};
}

void ApplyProtocolDefaults(ProductConfig* config) {
  if (config == nullptr) return;
  std::string profile = config->modbus_profile;
  std::transform(profile.begin(), profile.end(), profile.begin(), ::tolower);
  if (profile == "l1") {
    ApplyL1ModbusProfile(config);
  } else {
    ApplyG5ModbusProfile(config);
  }
}

void LoadJointConfig(const nlohmann::json& j, ProductConfig* config) {
  if (!j.contains("joints") || !j["joints"].is_array()) return;

  for (const auto& item : j["joints"]) {
    if (!item.contains("id") || !item["id"].is_string()) continue;
    JointId id = JointIdFromString(item["id"].get<std::string>());
    if (id == JointId::NUM_JOINTS) continue;
    config->valid_joints.push_back(id);
    if (item.contains("min") && item.contains("max") &&
        item["min"].is_number() && item["max"].is_number()) {
      float min_val = item["min"].get<float>();
      float max_val = item["max"].get<float>();
      if (min_val > max_val) std::swap(min_val, max_val);
      config->joint_limits[id] = {min_val, max_val};
    }
  }
}

void LoadTactileConfig(const nlohmann::json& j, ProductConfig* config) {
  if (!j.contains("tactile_regions") ||
      !j["tactile_regions"].is_array()) {
    return;
  }

  for (const auto& item : j["tactile_regions"]) {
    TactileRegionConfig region;
    if (item.contains("name") && item["name"].is_string()) {
      region.name = item["name"].get<std::string>();
    }
    if (item.contains("count") && item["count"].is_number_integer()) {
      region.sensor_count = item["count"].get<int>();
    }
    if (!region.name.empty() && region.sensor_count > 0) {
      config->tactile_regions.push_back(region);
    }
  }
}

ProductConfig ParseProductConfigJson(const nlohmann::json& j) {
  ProductConfig config;
  config.model = JsonStringOrEmpty(j, "model");
  config.name = JsonStringOrEmpty(j, "name");
  config.aliases = JsonStringArray(j, "aliases");
  config.slave_id =
      static_cast<uint8_t>(JsonIntOrDefault(j, "slave_id", 0x31));
  std::string modbus_profile = JsonStringOrEmpty(j, "modbus_profile");
  if (!modbus_profile.empty()) config.modbus_profile = modbus_profile;
  config.ethercat_input_sizes = JsonIntArray(j, "ethercat_input_sizes");
  config.ethercat_output_size =
      JsonIntOrDefault(j, "ethercat_output_size", 0);
  std::string rpdo_layout = JsonStringOrEmpty(j, "ethercat_rpdo_layout");
  if (!rpdo_layout.empty()) config.ethercat_rpdo_layout = rpdo_layout;
  std::string tpdo_layout = JsonStringOrEmpty(j, "ethercat_tpdo_layout");
  if (!tpdo_layout.empty()) config.ethercat_tpdo_layout = tpdo_layout;
  LoadJointConfig(j, &config);
  config.has_tactile = JsonBoolOrFalse(j, "has_tactile");
  LoadTactileConfig(j, &config);
  ApplyProtocolDefaults(&config);
  return config;
}

std::vector<std::string> ListJsonFiles(const std::string& search_dir) {
  std::vector<std::string> files;
#ifdef _WIN32
  std::string search_pattern = search_dir + "*.json";
  WIN32_FIND_DATAA fd;
  HANDLE hFind = FindFirstFileA(search_pattern.c_str(), &fd);
  if (hFind == INVALID_HANDLE_VALUE) return files;
  do {
    files.push_back(search_dir + fd.cFileName);
  } while (FindNextFileA(hFind, &fd));
  FindClose(hFind);
#else
  DIR* dir = opendir(search_dir.c_str());
  if (!dir) return files;
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name(entry->d_name);
    if (name.size() < 6 || name.substr(name.size() - 5) != ".json") continue;
    std::string file_path = search_dir + name;
    struct stat st;
    if (stat(file_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
    files.push_back(file_path);
  }
  closedir(dir);
#endif
  return files;
}

bool NameMatches(const std::string& device_name,
                 const std::string& config_name);

bool TryLoadMatchingConfig(const std::string& file_path,
                           const std::string& device_name,
                           ProductConfig* config) {
  std::string json_content = ReadTextFile(file_path);
  if (json_content.empty()) return false;

  nlohmann::json j;
  if (!ParseJsonContent(json_content, &j)) return false;
  std::vector<std::string> names = JsonStringArray(j, "aliases");
  names.push_back(JsonStringOrEmpty(j, "name"));
  bool matched = false;
  for (const auto& name : names) {
    if (NameMatches(device_name, name)) {
      matched = true;
      break;
    }
  }
  if (!matched) return false;
  *config = ParseProductConfigJson(j);
  return true;
}

ProductConfig LoadProductConfig(ProductType product) {
  ProductConfig config;
  std::string file_name = ProductTypeToFileName(product);
  if (file_name.empty()) {
    GHAND_LOG_ERROR("Unknown ProductType: " << static_cast<int>(product));
    return config;
  }

  std::vector<std::string> search_paths = GetConfigSearchPaths();
  std::string found_path = FindConfigFilePath(file_name, search_paths);
  if (found_path.empty()) {
    GHAND_LOG_ERROR("Product config file not found: " << file_name
                                                      << ". Searched in: ");
    for (const auto& dir : search_paths) {
      GHAND_LOG_ERROR("  " << dir);
    }
    return config;
  }

  std::string json_content = ReadTextFile(found_path);
  if (json_content.empty()) {
    GHAND_LOG_ERROR("Failed to open product config: " << found_path);
    return config;
  }

  nlohmann::json j;
  if (!ParseJsonContent(json_content, &j)) {
    GHAND_LOG_ERROR("Failed to parse product config " << found_path);
    return ProductConfig();
  }

  config = ParseProductConfigJson(j);
  if (config.name.empty() || config.valid_joints.empty()) {
    GHAND_LOG_ERROR("Product config missing required fields in "
                    << found_path);
    return ProductConfig();
  }

  GHAND_LOG_INFO("Loaded product config: " << config.name << " from "
                                           << found_path);
  return config;
}

bool NameMatches(const std::string& device_name,
                 const std::string& config_name) {
  if (config_name.empty() || device_name.empty()) return false;
  if (device_name.size() != config_name.size()) return false;
  auto ci_equal = [](char a, char b) {
    return std::tolower(a) == std::tolower(b);
  };
  return std::equal(config_name.begin(), config_name.end(),
                    device_name.begin(), ci_equal);
}

ProductConfig FindConfigByName(const std::string& device_name) {
  if (device_name.empty()) return ProductConfig();

  std::vector<std::string> search_paths = GetConfigSearchPaths();
  for (const auto& search_dir : search_paths) {
    for (const auto& file_path : ListJsonFiles(search_dir)) {
      ProductConfig config;
      if (!TryLoadMatchingConfig(file_path, device_name, &config)) {
        continue;
      }
      GHAND_LOG_INFO("Auto-detected product config: "
                     << config.name << " from " << file_path);
      return config;
    }
  }

  GHAND_LOG_ERROR(
      "No matching product config found for device model: " << device_name);
  return ProductConfig();
}

}  // namespace internal
}  // namespace ghand
