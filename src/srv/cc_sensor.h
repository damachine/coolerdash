/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief CPU/GPU temperature monitoring via CoolerControl API.
 * @details Reads sensor data from /status endpoint.
 */

// Include necessary headers
#ifndef CC_SENSOR_H
#define CC_SENSOR_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
// cppcheck-suppress-end missingIncludeSystem

// Forward declaration
struct Config;

/**
 * @brief Structure to hold temperature sensor data.
 * @details Contains temperature values (CPU, GPU, and Liquid/Coolant)
 * representing temperatures in degrees Celsius.
 */
typedef struct
{
    float temp_cpu;
    float temp_gpu;
    float temp_liquid; // Liquid/Coolant temperature from Liquidctl device
} monitor_sensor_data_t;

/**
 * @brief Get temperature data into structure.
 * @details High-level convenience function that retrieves temperature data and
 * populates a monitor_sensor_data_t structure with the values.
 */
int get_temperature_monitor_data(const struct Config *config,
                                 monitor_sensor_data_t *data);

/**
 * @brief Cleanup cached sensor CURL handle.
 * @details Called during daemon shutdown to free resources.
 */
void cleanup_sensor_curl_handle(void);

#endif // CC_SENSOR_H
