#ifndef SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_
#define SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

/**
 * @brief 根据产品类型加载对应的 JSON 配置文件
 * @param product 产品类型
 * @return 加载成功返回 ProductConfig，失败返回空配置（name 为空）
 */
ProductConfig LoadProductConfig(ProductType product);

/**
 * @brief 根据设备名自动扫描并加载配置文件
 *
 * 遍历所有配置搜索路径下的 .json 文件，找到 name 字段与
 * device_name 精确匹配（大小写不敏感）的配置并加载。
 *
 * @param device_name 设备回报的设备名（如 "XIAOYAO-Hand"）
 * @return 匹配成功返回 ProductConfig，失败返回空配置（name 为空）
 */
ProductConfig FindConfigByName(const std::string& device_name);

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_
