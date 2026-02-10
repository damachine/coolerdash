/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Circle mode rendering (alternating CPU/GPU).
 * @details Single sensor display with automatic 2.5s switching.
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

/**
 * @brief Collects sensor data and renders circle mode display.
 * @details High-level entry point function that retrieves temperature data and
 * triggers alternating single-sensor display rendering.
 */
void draw_circle_image(const struct Config *config);

#endif // CIRCLE_H
