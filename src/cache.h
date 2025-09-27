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
 * @brief Device cache for CoolerDash.
 * @details Caches device information retrieved from Liquidctl to minimize redundant API calls.
 */

// Include necessary headers
#ifndef CACHE_H
#define CACHE_H

// Include project headers
#include "config.h"

/**
 * @brief Initialize the device cache.
 * @details Populates the device cache by querying Liquidctl for connected devices.
 */
int init_device_cache(const Config *config);
int get_liquidctl_data(const Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height);

#endif // CACHE_H
