/**
 * @author damachine (christkue79@gmail.com)
 * @Maintainer: damachine <christkue79@gmail.com>
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 *    This software is provided "as is", without warranty of any kind, express or implied.
 *    I do not guarantee that it will work as intended on your system.
 */

/**
 * @brief CoolerControl configuration - Device cache and display detection.
 * @details Provides functions for device information caching and circular display detection.
 */

#ifndef CC_CONF_H
#define CC_CONF_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <jansson.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/sys.h"

// Basic constants
#define CC_NAME_SIZE 128

// Forward declarations
struct Config;

/**
 * @brief Secure string copy with bounds checking.
 * @details Performs safe string copying with buffer overflow protection and null termination guarantee.
 */
int cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src);

/**
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from cache.
 * @details Reads all LCD device information from cache (no API call).
 */
int get_liquidctl_data(const struct Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height);

/**
 * @brief Initialize device information cache.
 * @details Fetches and caches device information once at startup for better performance.
 */
int init_device_cache(const struct Config *config);

/**
 * @brief Update config with device screen dimensions (only if not set in config.ini).
 * @details Reads screen dimensions from device cache and updates config ONLY if config.ini values are commented out (= 0).
 */
int update_config_from_device(struct Config *config);

/**
 * @brief Extract device type from JSON device object.
 * @details Common helper function to extract device type string from JSON device object.
 */
const char *extract_device_type_from_json(const json_t *dev);

/**
 * @brief Check if a device has a circular display based on device name/type.
 * @details Returns true if the device is known to have a circular/round LCD display.
 *          For NZXT Kraken devices, resolution determines shape:
 *          - 240x240 or smaller = rectangular
 *          - Larger than 240x240 = circular
 */
int is_circular_display_device(const char *device_name, int screen_width, int screen_height);

#endif // CC_CONF_H
