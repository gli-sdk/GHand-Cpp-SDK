#ifndef SRC_INTERNAL_PRODUCT_CONFIG_H_
#define SRC_INTERNAL_PRODUCT_CONFIG_H_

#include <map>
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
  std::string name;
  int sensor_count = 0;
};

/**
 * @brief Product hardware configuration
 *
 * Per-product differentiated parameters loaded from a JSON configuration file.
 */
struct ProductConfig {
  std::string model;
  std::string name;
  std::vector<JointId> valid_joints;
  std::map<JointId, std::pair<float, float>> joint_limits;
  bool has_tactile = false;
  std::vector<TactileRegionConfig> tactile_regions;
};

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_PRODUCT_CONFIG_H_
