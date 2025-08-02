/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

 /**
  * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
  * @details Provides functions to initialize the monitor subsystem and read CPU/GPU temperature values from the API.
  * @example
  *     #include "monitor.h"
  *
  *     int main() {
  *         struct Config config;
  *         // Load or set your configuration here...
  *
  *         float cpuTemp, gpuTemp;
  *         if (get_temperature_data(&config, &cpuTemp, &gpuTemp)) {
  *             printf("CPU Temp: %.2f°C\n", cpuTemp);
  *             printf("GPU Temp: %.2f°C\n", gpuTemp);
  *         } else {
  *             fprintf(stderr, "Failed to retrieve temperature data.\n");
  */

// Include necessary headers
#ifndef MONITOR_H
#define MONITOR_H

// Include minimal necessary headers
#include <stddef.h>
#include <stdint.h>

// Forward declaration to avoid circular dependency
struct Config;

// Temperature validation constants
#define MONITOR_TEMP_MIN -50.0f    // Minimum valid temperature (°C)
#define MONITOR_TEMP_MAX 150.0f    // Maximum valid temperature (°C)
#define MONITOR_TEMP_INVALID -999.0f // Invalid temperature indicator

/**
 * @brief Structure to hold temperature sensor data.
 * @details Used to aggregate temperature values from monitoring sensors. Optimized for cache alignment and minimal memory footprint with built-in validation.
 * @example
 *     monitor_sensor_data_t data;
 *     if (monitor_get_temperature_data(&config, &data)) { ... }
 */
typedef struct __attribute__((aligned(8))) {  // 8-byte alignment for optimal cache performance
    float temp_1;                    // Temperature 1 in degrees Celsius (4 bytes)
    float temp_2;                    // Temperature 2 in degrees Celsius (4 bytes)
    // Total: 8 bytes (cache-friendly size, aligned to 8-byte boundary)
} monitor_sensor_data_t;

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Reads the current CPU and GPU temperatures via API. Returns 1 on success, 0 on failure. Enhanced with input validation and buffer overflow protection.
 * @example
 *     float temp_1, temp_2;
 *     if (get_temperature_data(&config, &temp_1, &temp_2)) { ... }
 */
int get_temperature_data(const struct Config *config, float *temp_1, float *temp_2);

/**
 * @brief Get temperature data into structure.
 * @details Optimized version that fills a structure instead of separate parameters.
 * Enhanced with input validation and error handling.
 * @example
 *     monitor_sensor_data_t data;
 *     if (monitor_get_temperature_data(&config, &data)) { ... }
 */
int monitor_get_temperature_data(const struct Config *config, monitor_sensor_data_t *data);

/**
 * @brief Initialize the monitor subsystem.
 * @details Sets up monitoring resources and validates configuration.
 * @example
 *     if (monitor_init(&config)) { ... }
 */
int monitor_init(const struct Config *config);

/**
 * @brief Cleanup monitor resources.
 * @details Frees any resources allocated by the monitor subsystem.
 * @example
 *     monitor_cleanup();
 */
void monitor_cleanup(void);

// Inline helper functions for performance-critical temperature operations

/**
 * @brief Validate temperature value is within reasonable range.
 * @details Checks if temperature is within valid sensor range.
 * @example
 *     if (monitor_is_temp_valid(temp)) { ... }
 */
static inline int monitor_is_temp_valid(float temperature) {
    return (temperature >= MONITOR_TEMP_MIN && 
            temperature <= MONITOR_TEMP_MAX &&
            temperature != MONITOR_TEMP_INVALID);
}

/**
 * @brief Initialize sensor data structure with safe defaults.
 * @details Sets all temperature values to invalid state for safety.
 * @example
 *     monitor_sensor_data_t data;
 *     monitor_init_sensor_data(&data);
 */
static inline void monitor_init_sensor_data(monitor_sensor_data_t *data) {
    if (data) {
        data->temp_1 = MONITOR_TEMP_INVALID;
        data->temp_2 = MONITOR_TEMP_INVALID;
    }
}

/**
 * @brief Check if sensor data contains valid temperatures.
 * @details Validates both temperature readings in the structure.
 * @example
 *     if (monitor_has_valid_data(&data)) { ... }
 */
static inline int monitor_has_valid_data(const monitor_sensor_data_t *data) {
    return (data && 
            monitor_is_temp_valid(data->temp_1) && 
            monitor_is_temp_valid(data->temp_2));
}

/**
 * @brief Get temperature difference between two sensors.
 * @details Calculates absolute temperature difference with validation.
 * @example
 *     float diff = monitor_get_temp_diff(&data);
 */
static inline float monitor_get_temp_diff(const monitor_sensor_data_t *data) {
    if (!data || !monitor_has_valid_data(data)) {
        return MONITOR_TEMP_INVALID;
    }
    
    float diff = data->temp_1 - data->temp_2;
    return (diff < 0) ? -diff : diff; // Return absolute difference
}

#endif // MONITOR_H
