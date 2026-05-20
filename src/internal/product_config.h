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
 * @brief 单个触觉区域的配置
 */
struct TactileRegionConfig {
  std::string name;
  int sensor_count = 0;
};

/**
 * @brief 产品硬件配置
 *
 * 从 JSON 配置文件加载的每产品差异化参数。
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
