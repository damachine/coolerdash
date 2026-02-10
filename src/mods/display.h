/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Display mode dispatcher and shared rendering utilities.
 * @details Routes to dual or circle rendering module, provides common Cairo
 * helpers and sensor slot functions shared by all display modes.
 */

#ifndef DISPLAY_DISPATCHER_H
#define DISPLAY_DISPATCHER_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <stddef.h>
#include <stdint.h>
// cppcheck-suppress-end missingIncludeSystem

// Include for Config and Color types
#include "../device/config.h"
// Include for monitor_sensor_data_t
#include "../srv/cc_sensor.h"

// ============================================================================
// Mathematical Constants for Cairo Graphics
// ============================================================================

#ifndef M_PI
#define DISPLAY_M_PI 3.14159265358979323846
#else
#define DISPLAY_M_PI M_PI
#endif
#ifndef M_PI_2
#define DISPLAY_M_PI_2 1.57079632679489661923
#else
#define DISPLAY_M_PI_2 M_PI_2
#endif

// ============================================================================
// Shared Rendering Types
// ============================================================================

/**
 * @brief Dynamic scaling parameters for display rendering.
 * @details Calculated once per frame based on display dimensions and device type.
 */
typedef struct
{
    double scale_x;
    double scale_y;
    double corner_radius;
    double inscribe_factor;     /**< 1.0 for rectangular, M_SQRT1_2 for circular */
    int safe_bar_width;         /**< Safe bar width for circular displays */
    double safe_content_margin; /**< Horizontal margin for safe content area */
    int is_circular;            /**< 1 if circular display, 0 if rectangular */
} ScalingParams;

/**
 * @brief Main display dispatcher - routes to appropriate rendering mode.
 * @details High-level entry point that examines configuration and dispatches to
 * either dual mode (CPU+GPU simultaneously) or circle mode (alternating
 * display). This is the primary interface called by the main daemon loop.
 * @param config Configuration containing display mode and rendering parameters
 */
void draw_display_image(const struct Config *config);

// ============================================================================
// Shared Cairo Rendering Helpers
// ============================================================================

/**
 * @brief Calculate dynamic scaling parameters based on display dimensions.
 * @details Detects circular/rectangular displays and computes safe content area.
 * @param config Configuration with display dimensions
 * @param params Output scaling parameters
 * @param device_name Device name for circular display detection
 */
void calculate_scaling_params(const struct Config *config,
                              ScalingParams *params, const char *device_name);

/**
 * @brief Create cairo surface and context for display rendering.
 * @details Caller must destroy both context and surface after use.
 * @param config Configuration with display dimensions
 * @param surface Output pointer to created surface
 * @return Cairo context, or NULL on failure
 */
cairo_t *create_cairo_context(const struct Config *config,
                              cairo_surface_t **surface);

/**
 * @brief Convert color component from 0-255 to cairo 0.0-1.0 range.
 */
double cairo_color_convert(uint8_t color_component);

/**
 * @brief Set cairo source color from Color structure.
 */
void set_cairo_color(cairo_t *cr, const Color *color);

/**
 * @brief Calculate temperature fill width with bounds checking.
 * @param temp_value Current temperature value
 * @param max_width Maximum width of the bar in pixels
 * @param max_temp Maximum temperature scale
 * @return Fill width in pixels
 */
int calculate_temp_fill_width(float temp_value, int max_width, float max_temp);

/**
 * @brief Draw rounded rectangle path for temperature bars.
 */
void draw_rounded_rectangle_path(cairo_t *cr, int x, int y, int width,
                                 int height, double radius);

/**
 * @brief Draw degree symbol at calculated position with proper font scaling.
 */
void draw_degree_symbol(cairo_t *cr, double x, double y,
                        const struct Config *config);

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
