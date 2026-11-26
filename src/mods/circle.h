/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
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
#include "display.h"

// Forward declarations
struct Config;

int paint_circle_overlay(cairo_t *cr, const struct Config *config,
                         const monitor_sensor_data_t *data,
                         const ScalingParams *params);

/** Render circle mode to config->paths_image_coolerdash without uploading it. */
int render_circle_preview(const struct Config *config,
                          const monitor_sensor_data_t *data,
                          const char *device_name);

/**
 * @brief Resets circle mode state for config reload (SIGHUP).
 * @details Resets slot cycling to the first sensor and clears the switch timer.
 */
void reset_circle_state(void);

#endif // CIRCLE_H
