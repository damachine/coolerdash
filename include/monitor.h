/**
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
 * @details Provides functions for initializing the monitor subsystem and retrieving temperature sensor data from CoolerControl REST API endpoints.
 */

// Include necessary headers
#ifndef MONITOR_H
#define MONITOR_H

// Include necessary headers
#include <stddef.h>

// Forward declaration
struct Config;

/**
 * @brief Structure to hold temperature sensor data.
 * @details Contains two temperature values (temp_1 and temp_2) representing CPU and GPU temperatures in degrees Celsius.
 */
typedef struct {
    float temp_1;
    float temp_2;
} monitor_sensor_data_t;

/**
 * @brief Initialize the monitor subsystem.
 * @details Performs any necessary initialization for the monitoring functionality, preparing it for temperature data retrieval. Must be called before using other monitor functions.
 */
int monitor_init(const struct Config *config);

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Low-level function that retrieves temperature values via HTTP request to CoolerControl daemon and stores them in the provided float pointers.
 */
int get_temperature_data(const struct Config *config, float *temp_1, float *temp_2);

/**
 * @brief Get temperature data into structure.
 * @details High-level convenience function that retrieves temperature data and populates a monitor_sensor_data_t structure with the values.
 */
int monitor_get_temperature_data(const struct Config *config, monitor_sensor_data_t *data);

#endif // MONITOR_H
