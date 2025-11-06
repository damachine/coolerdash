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
 * @brief Circle mode LCD rendering - Single sensor alternating display.
 * @details Provides functions for rendering single CPU or GPU sensor with automatic switching every 2.5 seconds.
 */

#ifndef CIRCLE_H
#define CIRCLE_H

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
#define CIRCLE_M_PI 3.14159265358979323846
#else
#define CIRCLE_M_PI M_PI
#endif
#ifndef M_PI_2
#define CIRCLE_M_PI_2 1.57079632679489661923
#else
#define CIRCLE_M_PI_2 M_PI_2
#endif

/**
 * @brief Collects sensor data and renders circle mode display.
 * @details High-level entry point function that retrieves temperature data and triggers alternating single-sensor display rendering.
 */
void draw_circle_image(const struct Config *config);

/**
 * @brief Render single sensor display based on current mode (CPU or GPU).
 * @details Creates PNG image using Cairo graphics library showing either CPU or GPU temperature, alternates every 2.5 seconds.
 */
int render_circle_display(const struct Config *config, const monitor_sensor_data_t *data, const char *device_name);

#endif // CIRCLE_H
