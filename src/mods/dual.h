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
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <stdint.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../srv/cc_sensor.h"

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

/**
 * @brief Collects sensor data and renders display based on selected mode.
 * @details High-level entry point function that retrieves temperature data from monitoring subsystem
 *          and triggers display rendering (dual or circle mode) with current configuration.
 */
void draw_combined_image(const struct Config *config);

/**
 * @brief Render dual-sensor display (CPU+GPU simultaneously).
 * @details Low-level function that creates PNG image using Cairo graphics library based on temperature
 *          sensor data and configuration settings, then uploads to LCD device. Shows both sensors at once.
 */
int render_dual_display(const struct Config *config, const monitor_sensor_data_t *data, const char *device_name);

/**
 * @brief Collects sensor data and renders dual-sensor display.
 * @details High-level entry point for dual mode - retrieves temperature data and triggers dual display rendering.
 */
void draw_dual_image(const struct Config *config);

#endif // DISPLAY_H
