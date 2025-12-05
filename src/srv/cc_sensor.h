/**
* -----------------------------------------------------------------------------
* Created by: damachine (christkue79 at gmail dot com)
* Website: https://github.com/damachine/coolerdash
* -----------------------------------------------------------------------------
 */

/**
 * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
 * @details Provides functions for initializing the monitor subsystem and retrieving temperature sensor data from CoolerControl REST API endpoints.
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
 * @details Contains temperature values (CPU, GPU, and Liquid/Coolant) representing temperatures in degrees Celsius.
 */
typedef struct
{
    float temp_cpu;
    float temp_gpu;
    float temp_liquid; // Liquid/Coolant temperature from Liquidctl device
} monitor_sensor_data_t;

/**
 * @brief Get temperature data into structure.
 * @details High-level convenience function that retrieves temperature data and populates a monitor_sensor_data_t structure with the values.
 */
int get_temperature_monitor_data(const struct Config *config, monitor_sensor_data_t *data);

#endif // CC_SENSOR_H
