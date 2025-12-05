/**
 * @author damachine (christkue79@gmail.com)
 * @Maintainer: damachine <christkue79@gmail.com>
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 2.0
 *    This software is provided "as is", without warranty of any kind, express or implied.
 *    I do not guarantee that it will work as intended on your system.
 */

/**
 * @brief Display mode dispatcher for CoolerDash.
 * @details Provides mode-agnostic dispatcher that routes to appropriate rendering modules (dual or circle mode).
 */

#ifndef DISPLAY_DISPATCHER_H
#define DISPLAY_DISPATCHER_H

// Forward declarations
struct Config;

/**
 * @brief Main display dispatcher - routes to appropriate rendering mode.
 * @details High-level entry point that examines configuration and dispatches to either
 *          dual mode (CPU+GPU simultaneously) or circle mode (alternating display).
 *          This is the primary interface called by the main daemon loop.
 * @param config Configuration containing display mode and rendering parameters
 */
void draw_display_image(const struct Config *config);

#endif // DISPLAY_DISPATCHER_H
