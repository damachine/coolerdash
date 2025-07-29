/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief LCD rendering and image upload interface for CoolerDash.
 * @details Interface for all display rendering logic, including temperature bars, labels, and image upload.
 * @example
 *     See function documentation for usage examples.
 */

// Function prototypes
#ifndef DISPLAY_H
#define DISPLAY_H

// Include project headers
#include "config.h"
#include "coolercontrol.h"

/**
 * @brief Sensor data structure for display rendering.
 * @details Holds temperature values for CPU and GPU. Reserved fields are for future multi-mode support.
 * @example
 *     sensor_data_t data = { .cpu_temp = 55.0f, .gpu_temp = 48.0f };
 */
typedef struct {
    float temp_1; // formerly cpu_temp, temperature in degrees Celsius
    float temp_2; // formerly gpu_temp, temperature in degrees Celsius
} sensor_data_t;

/**
 * @brief Render display based on sensor data and configuration (only default mode).
 * @details Renders the LCD display image using the provided sensor data and configuration. Handles drawing, saving, and uploading the image. Returns 1 on success, 0 on error. Always check the return value.
 * @example
 *     int result = render_display(&config, &sensor_data);
 */
int render_display(const Config *config, const sensor_data_t *data);

/**
 * @brief Collects sensor data and renders display (default mode only).
 * @details Reads all relevant sensor data (temperatures) and renders the display image. Also uploads the image to the device if available. Handles errors silently and frees all resources. No return value.
 * @example
 *     draw_combined_image(&config);
 */
void draw_combined_image(const Config *config);

#endif // DISPLAY_H
