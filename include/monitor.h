/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

#ifndef MONITOR_H
#define MONITOR_H

#include "config.h"

// Forward declaration to avoid circular dependency
struct Config;

/**
 * @brief Structure to hold temperature sensor data.
 * @details Used to aggregate temperature values from monitoring sensors.
 * Optimized for cache alignment and minimal memory footprint.
 * @example
 *     monitor_sensor_data_t data;
 *     if (monitor_get_temperature_data(&config, &data)) { ... }
 */
typedef struct {
    float temp_1;                    // temperature 1 in degrees Celsius (4 bytes)
    float temp_2;                    // temperature 2 in degrees Celsius (4 bytes)
    // Total: 8 bytes (cache-friendly size, naturally aligned)
} monitor_sensor_data_t;

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Reads the current CPU and GPU temperatures via API. Returns 1 on success, 0 on failure.
 * @example
 *     float temp_1, temp_2;
 *     if (get_temperature_data(&config, &temp_1, &temp_2)) { ... }
 */
int get_temperature_data(const Config *config, float *temp_1, float *temp_2);

/**
 * @brief Get temperature data into structure.
 * @details Optimized version that fills a structure instead of separate parameters.
 * @example
 *     monitor_sensor_data_t data;
 *     if (monitor_get_temperature_data(&config, &data)) { ... }
 */
int monitor_get_temperature_data(const Config *config, monitor_sensor_data_t *data);

#endif // MONITOR_H
