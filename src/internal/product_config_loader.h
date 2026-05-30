#ifndef SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_
#define SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_

#include "ghand/types.h"
#include "product_config.h"

namespace ghand {
namespace internal {

/**
 * @brief Load the corresponding JSON configuration file based on product type
 * @param product Product type
 * @return ProductConfig on success, empty config (name empty) on failure
 */
ProductConfig LoadProductConfig(ProductType product);

/**
 * @brief Automatically scan and load the configuration file based on device name
 *
 * Traverses all .json files under the configuration search paths, finds the one
 * whose name field exactly matches (case-insensitive) device_name, and loads it.
 *
 * @param device_name Device-reported device name (e.g., "XIAOYAO-Hand")
 * @return ProductConfig on match, empty config (name empty) on failure
 */
ProductConfig FindConfigByName(const std::string& device_name);

}  // namespace internal
}  // namespace ghand

#endif  // SRC_INTERNAL_PRODUCT_CONFIG_LOADER_H_
