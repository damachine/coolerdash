/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Dual mode rendering (CPU+GPU side by side).
 * @details Cairo-based LCD image generation for dual sensor display.
 */

// Include necessary headers
#ifndef DUAL_H
#define DUAL_H

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
 * @brief Main dual mode entry point.
 * @details Collects sensor data, renders dual display using
 * render_dual_display(), and uploads to LCD. This is the high-level function
 * called by the dispatcher to show CPU+GPU simultaneously.
 */
void draw_dual_image(const struct Config *config);

#endif // DUAL_H
