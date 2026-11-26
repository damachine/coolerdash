/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Split mode rendering (two bar-free sensor columns).
 */

#ifndef SPLIT_MODE_H
#define SPLIT_MODE_H

#include "display.h"

int paint_split_overlay(cairo_t *cr, const struct Config *config,
                        const monitor_sensor_data_t *data,
                        const ScalingParams *params);

/** Render split mode to config->paths_image_coolerdash without uploading it. */
int render_split_preview(const struct Config *config,
                         const monitor_sensor_data_t *data,
                         const char *device_name);

#endif // SPLIT_MODE_H
