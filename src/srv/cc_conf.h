/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Device cache and display detection.
 * @details Caches device UID, detects LCD type (circular vs rectangular).
 */

#ifndef CC_CONF_H
#define CC_CONF_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <jansson.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"

// Basic constants
#define CC_NAME_SIZE 128
#define MAX_DEVICE_NAME_CACHE 32

// Forward declarations
struct Config;

/**
 * @brief Secure string copy with bounds checking.
 * @details Performs safe string copying with buffer overflow protection and
 * null termination guarantee.
 */
int cc_safe_strcpy(char *restrict dest, size_t dest_size,
                   const char *restrict src);

/**
 * @brief Get complete Liquidctl device information (UID, name, screen
 * dimensions) from cache.
 * @details Reads all LCD device information from cache (no API call).
 */
int get_liquidctl_data(const struct Config *config, char *device_uid,
                       size_t uid_size, char *device_name, size_t name_size,
                       int *screen_width, int *screen_height);

/**
 * @brief Initialize device information cache.
 * @details Fetches and caches device information once at startup for better
 * performance.
 */
int init_device_cache(const struct Config *config);

/**
 * @brief Update config with device screen dimensions (only if not set in
 * config.json).
 * @details Reads screen dimensions from device cache and updates config ONLY if
 * config.json values are not set (= 0).
 */
int update_config_from_device(struct Config *config);

/**
 * @brief Check if a device has a circular display based on device name/type.
 * @details Returns true if the device is known to have a circular/round LCD
 * display. For NZXT Kraken devices, resolution determines shape:
 *          - 240x240 or smaller = rectangular
 *          - Larger than 240x240 = circular
 */
int is_circular_display_device(const char *device_name, int screen_width,
                               int screen_height);

/**
 * @brief Get device display name by UID.
 * @details Retrieves the cached device name for a given UID.
 * Cache is populated during init_device_cache().
 * @param device_uid Device UID to look up
 * @return Device name string, or empty string if not found
 */
const char *get_device_name_by_uid(const char *device_uid);

/**
 * @brief Get device type string by UID.
 * @param device_uid Device UID to look up
 * @return Device type string ("CPU","GPU","Liquidctl","Hwmon","CustomSensors"),
 *         or NULL if not found
 */
const char *get_device_type_by_uid(const char *device_uid);

/**
 * @brief Extract device type from JSON device object.
 * @details Checks "type" field first, falls back to "d_type" (used in /status).
 * @param dev JSON device object
 * @return Device type string, or NULL if not found
 */
const char *extract_device_type_from_json(const json_t *dev);

#endif // CC_CONF_H
