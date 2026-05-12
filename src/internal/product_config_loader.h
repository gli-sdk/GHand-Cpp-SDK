#ifndef SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_
#define SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_

#include "product_config.h"
#include "ghand/types.h"

namespace xiaoyao {
namespace internal {

/**
 * @brief 根据产品类型加载对应的 JSON 配置文件
 * @param product 产品类型
 * @return 加载成功返回 ProductConfig，失败返回空配置（name 为空）
 */
ProductConfig LoadProductConfig(ProductType product);

}  // namespace internal
}  // namespace xiaoyao

#endif  // SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_
