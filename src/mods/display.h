/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Display mode dispatcher for CoolerDash.
 * @details Provides mode-agnostic dispatcher that routes to appropriate
 * rendering modules (dual or circle mode).
 */

#ifndef DISPLAY_DISPATCHER_H
#define DISPLAY_DISPATCHER_H

// Include for Config and Color types
#include "../device/config.h"
// Include for monitor_sensor_data_t
#include "../srv/cc_sensor.h"

/**
 * @brief Main display dispatcher - routes to appropriate rendering mode.
 * @details High-level entry point that examines configuration and dispatches to
 * either dual mode (CPU+GPU simultaneously) or circle mode (alternating
 * display). This is the primary interface called by the main daemon loop.
 * @param config Configuration containing display mode and rendering parameters
 */
void draw_display_image(const struct Config *config);

// ============================================================================
// Sensor Slot Helper Functions
// ============================================================================

/**
 * @brief Check if a sensor slot is active (not "none")
 * @param slot_value Slot configuration value ("cpu", "gpu", "liquid", "none")
 * @return 1 if active, 0 if "none" or invalid
 */
int slot_is_active(const char *slot_value);

/**
 * @brief Get temperature value for a sensor slot
 * @param data Sensor data structure with CPU, GPU, and liquid temperatures
 * @param slot_value Slot configuration value ("cpu", "gpu", "liquid")
 * @return Temperature in Celsius, or 0.0 if slot is "none" or invalid
 */
float get_slot_temperature(const monitor_sensor_data_t *data, const char *slot_value);

/**
 * @brief Get display label for a sensor slot
 * @param slot_value Slot configuration value ("cpu", "gpu", "liquid", "none")
 * @return Label string ("CPU", "GPU", "LIQ") or NULL if "none"
 */
const char *get_slot_label(const char *slot_value);

/**
 * @brief Get bar color for a sensor slot based on temperature
 * @param config Configuration with threshold colors
 * @param slot_value Slot configuration value (determines which thresholds to use)
 * @param temperature Current temperature value
 * @return Color based on temperature thresholds
 */
Color get_slot_bar_color(const struct Config *config, const char *slot_value, float temperature);

/**
 * @brief Get maximum scale for a sensor slot
 * @param config Configuration with max scale values
 * @param slot_value Slot configuration value
 * @return Maximum temperature scale (liquid uses different max)
 */
float get_slot_max_scale(const struct Config *config, const char *slot_value);

/**
 * @brief Get bar height for a specific slot
 * @param config Configuration with bar height values
 * @param slot_name Slot name: "up", "mid", or "down"
 * @return Bar height in pixels for the specified slot
 */
uint16_t get_slot_bar_height(const struct Config *config, const char *slot_name);

#endif // DISPLAY_DISPATCHER_H
