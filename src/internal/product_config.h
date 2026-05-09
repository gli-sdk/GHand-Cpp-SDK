#ifndef SRC_INTERNAL_PRODUCT_CONFIG_H_
#define SRC_INTERNAL_PRODUCT_CONFIG_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "xiaoyao/types.h"

namespace xiaoyao {
namespace internal {

/**
 * @brief 产品硬件配置
 *
 * 从 JSON 配置文件加载的每产品差异化参数。
 */
struct ProductConfig {
    std::string name;
    std::vector<JointId> valid_joints;
    std::map<JointId, std::pair<float, float>> joint_limits;
    bool has_tactile = false;
    std::map<FingerType, int> tactile_sensor_counts;
    size_t protocol_joint_data_size = 0;
};

}  // namespace internal
}  // namespace xiaoyao

#endif  // SRC_INTERNAL_PRODUCT_CONFIG_H_
