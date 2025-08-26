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
 * @brief LCD rendering and image upload interface for CoolerDash.
 * @details Provides functions for rendering temperature data to LCD displays, including Cairo-based image generation and upload to CoolerControl devices.
 */

// Include necessary headers
#ifndef DISPLAY_H
#define DISPLAY_H

// Include necessary headers
#include <stddef.h>
#include <stdint.h>
#include "monitor.h"

// Forward declarations
struct Config;

// Mathematical constants for Cairo graphics operations
#ifndef M_PI
#define DISPLAY_M_PI 3.14159265358979323846
#else
#define DISPLAY_M_PI M_PI
#endif
#ifndef M_PI_2  
#define DISPLAY_M_PI_2 1.57079632679489661923
#else
#define DISPLAY_M_PI_2 M_PI_2
#endif

// Display positioning constants for LCD layout
#define DISPLAY_LABEL_Y_OFFSET_1 10
#define DISPLAY_LABEL_Y_OFFSET_2 15
#define DISPLAY_TEMP_DISPLAY_X_OFFSET 22
#define DISPLAY_TEMP_DISPLAY_Y_OFFSET 22
#define DISPLAY_TEMP_VERTICAL_ADJUSTMENT_TOP 2
#define DISPLAY_TEMP_VERTICAL_ADJUSTMENT_BOTTOM -2

/**
 * @brief Collects sensor data and renders display.
 * @details High-level entry point function that retrieves temperature data from monitoring subsystem and triggers display rendering with current configuration.
 */
void draw_combined_image(const struct Config *config);

/**
 * @brief Render display based on sensor data and configuration.
 * @details Low-level function that creates PNG image using Cairo graphics library based on temperature sensor data and configuration settings, then uploads to LCD device.
 */
int render_display(const struct Config *config, const monitor_sensor_data_t *data);

#endif // DISPLAY_H
