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

#endif // SPLIT_MODE_H
