/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
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
#include "display.h"

// Forward declarations
struct Config;

int paint_dual_overlay(cairo_t *cr, const struct Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params);

/** Render dual mode to config->paths_image_coolerdash without uploading it. */
int render_dual_preview(const struct Config *config,
                        const monitor_sensor_data_t *data,
                        const char *device_name);

#endif // DUAL_H
