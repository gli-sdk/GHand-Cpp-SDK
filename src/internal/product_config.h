// Copyright 2025 Glitech.

#ifndef SRC_INTERNAL_PRODUCT_CONFIG_H_
#define SRC_INTERNAL_PRODUCT_CONFIG_H_

#include <map>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ghand/types.h"

namespace ghand {
namespace internal {

/**
 * @brief Configuration for a single tactile region
 */
struct TactileRegionConfig {
  std::string id;
  int sensor_count = 0;
};

/**
 * @brief Product hardware configuration
 *
 * Per-product differentiated parameters loaded from a JSON configuration file.
 */
struct ProductConfig {
  ProductType product_type = ProductType::AUTO;
  std::string model;
  std::string name;
  std::vector<std::string> aliases;
  std::vector<JointId> valid_joints;
  std::map<JointId, std::pair<float, float>> joint_limits;
  bool has_tactile = false;
  std::vector<TactileRegionConfig> tactile_regions;

  std::map<JointId, uint16_t> joint_input_registers;
  std::map<JointId, uint16_t> joint_control_registers;
  int mode_register = 0x0010;
  int stop_register = 0x0010;
  int tactile_control_register = 0x002B;
  int canfd_connection_timer_register = 0x0031;
  int canfd_connection_timer_count = 1;
  std::vector<uint16_t> canfd_connection_timer_values;
  int canfd_connection_delete_register = 0x0030;
  int canfd_connection_delete_count = 2;
  std::vector<uint16_t> canfd_connection_delete_values;

};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_PRODUCT_CONFIG_H_
