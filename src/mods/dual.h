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
 * @brief Render dual-sensor display (CPU+GPU simultaneously).
 * @details Low-level function that creates PNG image using Cairo graphics library based on temperature
 *          sensor data and configuration settings, then uploads to LCD device. Shows both sensors at once.
 */
int render_dual_display(const struct Config *config, const monitor_sensor_data_t *data, const char *device_name);

/**
 * @brief Main dual mode entry point.
 * @details Collects sensor data, renders dual display using render_dual_display(), and uploads to LCD.
 *          This is the high-level function called by the dispatcher to show CPU+GPU simultaneously.
 */
void draw_dual_image(const struct Config *config);

#endif // DISPLAY_H
