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
 * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
 * @details Provides functions for initializing the monitor subsystem and retrieving temperature sensor data from CoolerControl REST API endpoints.
 */

// Include necessary headers
#ifndef MONITOR_H
#define MONITOR_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
// cppcheck-suppress-end missingIncludeSystem

// Forward declaration
struct Config;

/**
 * @brief Structure to hold temperature sensor data.
 * @details Contains two temperature values (temp_cpu and temp_gpu) representing CPU and GPU temperatures in degrees Celsius.
 */
typedef struct
{
    float temp_cpu;
    float temp_gpu;
} monitor_sensor_data_t;

/**
 * @brief Get temperature data into structure.
 * @details High-level convenience function that retrieves temperature data and populates a monitor_sensor_data_t structure with the values.
 */
int get_temperature_monitor_data(const struct Config *config, monitor_sensor_data_t *data);

#endif // MONITOR_H
