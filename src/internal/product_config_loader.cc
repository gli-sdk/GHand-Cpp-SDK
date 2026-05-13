#include "product_config_loader.h"
#include "logger.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <limits.h>
#endif

namespace ghand {
namespace internal {

std::string ProductTypeToFileName(ProductType type) {
    switch (type) {
        case ProductType::G5:      return "G5.json";
        default:                      return "";
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
        if (!in_string && c == '/' && i + 1 < content.size() && content[i + 1] == '/') {
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
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCSTR)&GetSdkRootFromModule,
            &hMod)) {
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
    if (dladdr((void*)&GetSdkRootFromModule, &info) == 0 || !info.dli_fname) {
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

std::vector<std::string> GetConfigSearchPaths() {
    std::vector<std::string> paths;

    const char* env_path = std::getenv("GHAND_SDK_CONFIG");
    if (env_path) {
        std::string path(env_path);
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
    const char* program_data = std::getenv("PROGRAMDATA");
    if (program_data) {
        paths.emplace_back(std::string(program_data) + "\\ghand-sdk\\config\\");
    }
#else
    paths.emplace_back("/usr/share/ghand-sdk/config/");
    paths.emplace_back("/usr/local/share/ghand-sdk/config/");
#endif

    return paths;
}

JointId JointIdFromString(const std::string& name) {
    if (name == "THUMB_DIP")       return JointId::THUMB_DIP;
    if (name == "THUMB_PIP")       return JointId::THUMB_PIP;
    if (name == "THUMB_MCP")       return JointId::THUMB_MCP;
    if (name == "THUMB_SWING")     return JointId::THUMB_SWING;
    if (name == "THUMB_ROTATION")  return JointId::THUMB_ROTATION;
    if (name == "FF_DIP")          return JointId::FF_DIP;
    if (name == "FF_PIP")          return JointId::FF_PIP;
    if (name == "FF_MCP")          return JointId::FF_MCP;
    if (name == "FF_SWING")        return JointId::FF_SWING;
    if (name == "MF_DIP")          return JointId::MF_DIP;
    if (name == "MF_PIP")          return JointId::MF_PIP;
    if (name == "MF_MCP")          return JointId::MF_MCP;
    if (name == "RF_DIP")          return JointId::RF_DIP;
    if (name == "RF_PIP")          return JointId::RF_PIP;
    if (name == "RF_MCP")          return JointId::RF_MCP;
    if (name == "LF_DIP")          return JointId::LF_DIP;
    if (name == "LF_PIP")          return JointId::LF_PIP;
    if (name == "LF_MCP")          return JointId::LF_MCP;
    LOG_WARNING("Unknown joint name: " << name);
    return JointId::NUM_JOINTS;
}

ProductConfig LoadProductConfig(ProductType product) {
    ProductConfig config;
    std::string file_name = ProductTypeToFileName(product);
    if (file_name.empty()) {
        LOG_ERROR("Unknown ProductType: " << static_cast<int>(product));
        return config;
    }

    std::vector<std::string> search_paths = GetConfigSearchPaths();
    std::string found_path;
    for (const auto& dir : search_paths) {
        std::string full_path = dir + file_name;
        std::ifstream test(full_path);
        if (test.good()) {
            found_path = full_path;
            break;
        }
    }

    if (found_path.empty()) {
        LOG_ERROR("Product config file not found: " << file_name
                  << ". Searched in: ");
        for (const auto& dir : search_paths) {
            LOG_ERROR("  " << dir);
        }
        return config;
    }

    std::ifstream ifs(found_path);
    if (!ifs) {
        LOG_ERROR("Failed to open product config: " << found_path);
        return config;
    }

    std::string raw_content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
    std::string json_content = StripComments(raw_content);

    try {
        nlohmann::json j = nlohmann::json::parse(json_content);

        config.name = j.value("name", "");

        if (j.contains("joints") && j["joints"].is_array()) {
            for (const auto& item : j["joints"]) {
                if (!item.contains("id") || !item["id"].is_string()) continue;
                JointId id = JointIdFromString(item["id"].get<std::string>());
                if (id == JointId::NUM_JOINTS) continue;
                config.valid_joints.push_back(id);
                if (item.contains("min") && item.contains("max")
                    && item["min"].is_number() && item["max"].is_number()) {
                    float min_val = item["min"].get<float>();
                    float max_val = item["max"].get<float>();
                    if (min_val > max_val) std::swap(min_val, max_val);
                    config.joint_limits[id] = {min_val, max_val};
                }
            }
        }

        config.has_tactile = j.value("has_tactile", false);

        if (j.contains("tactile_regions") && j["tactile_regions"].is_array()) {
            for (const auto& item : j["tactile_regions"]) {
                TactileRegionConfig region;
                region.name = item.value("name", "");
                region.sensor_count = item.value("count", 0);
                if (!region.name.empty() && region.sensor_count > 0) {
                    config.tactile_regions.push_back(region);
                }
            }
        }

        if (config.name.empty() || config.valid_joints.empty()) {
            LOG_ERROR("Product config missing required fields in " << found_path);
            return ProductConfig();
        }

        LOG_INFO("Loaded product config: " << config.name << " from " << found_path);
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to parse product config " << found_path << ": " << e.what());
        config = ProductConfig();
    }

    return config;
}

}  // namespace internal
}  // namespace ghand
