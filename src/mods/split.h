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

struct Config;

/** @brief Collect sensor data, render split mode, and upload it to the LCD. */
void draw_split_image(const struct Config *config);

/** Render split mode to config->paths_image_coolerdash without uploading it. */
int render_split_preview(const struct Config *config,
                         const monitor_sensor_data_t *data,
                         const char *device_name);

#endif // SPLIT_MODE_H
