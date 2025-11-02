/**
 * @author damachine (christkue79@gmail.com)
 * @Maintainer: damachine <christkue79@gmail.com>
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 *    This software is provided "as is", without warranty of any kind, express or implied.
 *    I do not guarantee that it will work as intended on your system.
 */

/**
 * @brief LCD rendering and image upload implementation for CoolerDash.
 * @details Provides functions for rendering the LCD display based on sensor data and configuration.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "config.h"
#include "display.h"
#include "coolercontrol.h"
#include "monitor.h"

// Define mathematical constants if not defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef DISPLAY_M_PI
#define DISPLAY_M_PI M_PI
#endif
#ifndef DISPLAY_M_PI_2
#define DISPLAY_M_PI_2 (M_PI / 2.0)
#endif

// Internal display positioning constants (base values for 240x240 resolution)
// These are scaled dynamically based on actual display_width and display_height
#define DISPLAY_LABEL_Y_OFFSET_1 8
#define DISPLAY_LABEL_Y_OFFSET_2 16
#define DISPLAY_TEMP_DISPLAY_X_OFFSET 26
#define DISPLAY_TEMP_DISPLAY_Y_OFFSET 26
#define DISPLAY_TEMP_VERTICAL_ADJUSTMENT_TOP 4
#define DISPLAY_TEMP_VERTICAL_ADJUSTMENT_BOTTOM -6

/**
 * @brief Convert color component to cairo format.
 * @details Converts 8-bit color component (0-255) to cairo's double format (0.0-1.0) for rendering operations.
 */
static inline double cairo_color_convert(uint8_t color_component)
{
    return color_component / 255.0;
}

/**
 * @brief Set cairo color from Color structure
 * @details Helper function to set cairo source color from Color struct in one call
 */
static inline void set_cairo_color(cairo_t *cr, const Color *color)
{
    cairo_set_source_rgb(cr,
                         cairo_color_convert(color->r),
                         cairo_color_convert(color->g),
                         cairo_color_convert(color->b));
}

/**
 * @brief Calculate temperature fill width with bounds checking
 * @details Simple helper to calculate bar fill width with safety checks
 */
static inline int calculate_temp_fill_width(float temp_value, int max_width)
{
    if (temp_value <= 0.0f)
        return 0;

    const float ratio = fminf(temp_value / 105.0f, 1.0f);
    return (int)(ratio * max_width);
}

/**
 * @brief Dynamic scaling parameters structure.
 * @details Holds calculated scaling factors and offsets based on display dimensions.
 */
typedef struct
{
    double scale_x;                  // Horizontal scaling factor
    double scale_y;                  // Vertical scaling factor
    double label_y_offset_1;         // First label Y offset (scaled)
    double label_y_offset_2;         // Second label Y offset (scaled)
    double temp_display_x_offset;    // Temperature X offset (scaled)
    double temp_display_y_offset;    // Temperature Y offset (scaled)
    double temp_vertical_adj_top;    // Top temperature vertical adjustment (scaled)
    double temp_vertical_adj_bottom; // Bottom temperature vertical adjustment (scaled)
    double corner_radius;            // Rounded corner radius (scaled)
} ScalingParams;

/**
 * @brief Calculate dynamic scaling parameters based on display dimensions.
 * @details Computes scaling factors relative to base resolution (240x240).
 */
static void calculate_scaling_params(const struct Config *config, ScalingParams *params)
{
    // Base resolution for reference (original design)
    const double base_width = 240.0;
    const double base_height = 240.0;

    // Calculate scaling factors
    params->scale_x = config->display_width / base_width;
    params->scale_y = config->display_height / base_height;

    // Use average scaling for uniform elements
    const double scale_avg = (params->scale_x + params->scale_y) / 2.0;

    // Scale offsets proportionally
    params->label_y_offset_1 = DISPLAY_LABEL_Y_OFFSET_1 * params->scale_y;
    params->label_y_offset_2 = DISPLAY_LABEL_Y_OFFSET_2 * params->scale_y;
    params->temp_display_x_offset = DISPLAY_TEMP_DISPLAY_X_OFFSET * params->scale_x;
    params->temp_display_y_offset = DISPLAY_TEMP_DISPLAY_Y_OFFSET * params->scale_y;
    params->temp_vertical_adj_top = DISPLAY_TEMP_VERTICAL_ADJUSTMENT_TOP * params->scale_y;
    params->temp_vertical_adj_bottom = DISPLAY_TEMP_VERTICAL_ADJUSTMENT_BOTTOM * params->scale_y;
    params->corner_radius = 8.0 * scale_avg;
}

/**
 * @brief Forward declarations for internal display rendering functions.
 * @details Function prototypes for internal display rendering helpers and utility functions used by the main rendering pipeline.
 */
static inline void draw_temp(cairo_t *cr, const struct Config *config, const ScalingParams *params, double temp_value, double y_offset);
static void draw_temperature_displays(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params);
static void draw_temperature_bars(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params);
static void draw_single_temperature_bar(cairo_t *cr, const struct Config *config, const ScalingParams *params, float temp_value, int bar_x, int bar_y);
static void draw_labels(cairo_t *cr, const struct Config *config, const ScalingParams *params);
static Color get_temperature_bar_color(const struct Config *config, float val);
static void draw_rounded_rectangle_path(cairo_t *cr, int x, int y, int width, int height, double radius);

/**
 * @brief Draw rounded rectangle path for temperature bars.
 * @details Helper function to create a rounded rectangle path with dynamic corner radius.
 */
static void draw_rounded_rectangle_path(cairo_t *cr, int x, int y, int width, int height, double radius)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -DISPLAY_M_PI_2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, DISPLAY_M_PI_2);
    cairo_arc(cr, x + radius, y + height - radius, radius, DISPLAY_M_PI_2, DISPLAY_M_PI);
    cairo_arc(cr, x + radius, y + radius, radius, DISPLAY_M_PI, 1.5 * DISPLAY_M_PI);
    cairo_close_path(cr);
}

/**
 * @brief Calculate color gradient for temperature bars (green → orange → hot orange → red).
 * @details Determines the RGB color for a given temperature value according to the defined thresholds from config.
 */
static Color get_temperature_bar_color(const struct Config *config, float val)
{
    // Temperature threshold and color mapping table
    const struct
    {
        float threshold;
        Color color;
    } temp_ranges[] = {
        {config->temp_threshold_1, config->temp_threshold_1_bar},
        {config->temp_threshold_2, config->temp_threshold_2_bar},
        {config->temp_threshold_3, config->temp_threshold_3_bar},
        {INFINITY, config->temp_threshold_4_bar}};

    // Find the appropriate color range for the given temperature
    for (size_t i = 0; i < sizeof(temp_ranges) / sizeof(temp_ranges[0]); i++)
    {
        if (val <= temp_ranges[i].threshold)
        {
            return temp_ranges[i].color;
        }
    }

    // Fallback to red if no match found
    return config->temp_threshold_4_bar;
}

/**
 * @brief Draw a single temperature value.
 * @details Helper function that renders a temperature value as text with proper positioning and formatting.
 */
static inline void draw_temp(cairo_t *cr, const struct Config *config, const ScalingParams *params, double temp_value, double y_offset)
{
    // Input validation with early return
    char temp_str[16];
    cairo_text_extents_t ext;

    // Format temperature string
    snprintf(temp_str, sizeof(temp_str), "%d°", (int)temp_value);

    // Calculate text extents and position
    cairo_text_extents(cr, temp_str, &ext);
    const double x = (config->layout_box_width - ext.width) / 2 + params->temp_display_x_offset;
    const double vertical_adjustment = (y_offset < 0) ? params->temp_vertical_adj_top : params->temp_vertical_adj_bottom;
    const double y = y_offset + (config->layout_box_height + ext.height) / 2 + vertical_adjustment;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, temp_str);
}

/**
 * @brief Draw temperature displays with enhanced positioning and validation.
 * @details Draws the temperature values for CPU and GPU in their respective boxes with improved accuracy and safety checks.
 */
static void draw_temperature_displays(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params)
{
    // Input validation with early return
    if (!cr || !data || !config || !params)
        return;

    // temp_cpu display (CPU temperature) with validation
    draw_temp(cr, config, params, data->temp_cpu, -params->temp_display_y_offset);

    // temp_gpu display (GPU temperature) with validation
    draw_temp(cr, config, params, data->temp_gpu, config->layout_box_height + params->temp_display_y_offset);
}

/**
 * @brief Draw a single temperature bar with enhanced safety and optimizations.
 * @details Helper function that draws background, fill, and border for a temperature bar with rounded corners.
 */
static void draw_single_temperature_bar(cairo_t *cr, const struct Config *config, const ScalingParams *params, float temp_value, int bar_x, int bar_y)
{
    if (!cr || !config || !params)
        return;

    // Calculate fill width
    const int fill_width = calculate_temp_fill_width(temp_value, config->layout_bar_width);

    // Draw background
    set_cairo_color(cr, &config->layout_bar_color_background);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, config->layout_bar_width, config->layout_bar_height, params->corner_radius);
    cairo_fill(cr);

    // Draw fill if needed
    if (fill_width > 0)
    {
        Color fill_color = get_temperature_bar_color(config, temp_value);
        set_cairo_color(cr, &fill_color);

        if (fill_width >= 16)
        {
            // Use rounded rectangle for wider fills
            draw_rounded_rectangle_path(cr, bar_x, bar_y, fill_width, config->layout_bar_height, params->corner_radius);
        }
        else
        {
            // Use simple rectangle for narrow fills
            cairo_rectangle(cr, bar_x, bar_y, fill_width, config->layout_bar_height);
        }
        cairo_fill(cr);
    }

    // Draw border
    cairo_set_line_width(cr, config->layout_bar_border_width);
    set_cairo_color(cr, &config->layout_bar_color_border);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, config->layout_bar_width, config->layout_bar_height, params->corner_radius);
    cairo_stroke(cr);
}

/**
 * @brief Draw temperature bars with simplified positioning.
 * @details Draws horizontal bars representing CPU and GPU temperatures with centered positioning.
 */
static void draw_temperature_bars(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params)
{
    if (!cr || !data || !config || !params)
        return;

    // Calculate centered horizontal position
    const int bar_x = (config->display_width - config->layout_bar_width) / 2;

    // Calculate vertical positions for CPU and GPU bars
    const int total_height = 2 * config->layout_bar_height + config->layout_bar_gap;
    const int start_y = (config->display_height - total_height) / 2;

    const int cpu_bar_y = start_y;
    const int gpu_bar_y = start_y + config->layout_bar_height + config->layout_bar_gap;

    // Draw bars
    draw_single_temperature_bar(cr, config, params, data->temp_cpu, bar_x, cpu_bar_y);
    draw_single_temperature_bar(cr, config, params, data->temp_gpu, bar_x, gpu_bar_y);
}

/**
 * @brief Draw CPU/GPU labels with enhanced positioning and validation.
 * @details Draws text labels for CPU and GPU with optimized positioning calculations and comprehensive input validation. Font and color are set by the main rendering pipeline.
 */
static void draw_labels(cairo_t *cr, const struct Config *config, const ScalingParams *params)
{
    // Input validation with early return
    if (!cr || !config || !params)
        return;

    // Positioning values
    const double box_center_y = config->layout_box_height / 2.0;
    const double font_half_height = config->font_size_labels / 2.0;

    // CPU label
    const double cpu_label_y = box_center_y + font_half_height + params->label_y_offset_1;
    cairo_move_to(cr, 0, cpu_label_y);
    cairo_show_text(cr, "CPU");

    // GPU label
    const double gpu_label_y = config->layout_box_height + box_center_y + font_half_height - params->label_y_offset_2;
    cairo_move_to(cr, 0, gpu_label_y);
    cairo_show_text(cr, "GPU");
}

/**
 * @brief Display rendering.
 * @details Creates cairo surface and context, renders temperature displays and bars, then saves the result as PNG image.
 */
int render_display(const struct Config *config, const monitor_sensor_data_t *data)
{
    if (!data || !config)
    {
        log_message(LOG_ERROR, "Invalid parameters for render_display");
        return 0;
    }

    // Calculate scaling parameters
    ScalingParams scaling_params;
    calculate_scaling_params(config, &scaling_params);

    // Create cairo surface and context
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                          config->display_width,
                                                          config->display_height);
    if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to create cairo surface");
        if (surface)
            cairo_surface_destroy(surface);
        return 0;
    }

    cairo_t *cr = cairo_create(surface);
    if (!cr)
    {
        log_message(LOG_ERROR, "Failed to create cairo context");
        cairo_surface_destroy(surface);
        return 0;
    }

    // Fill background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    // Configure font and color for temperature values
    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    set_cairo_color(cr, &config->font_color_temp);

    // Render temperature displays and bars
    draw_temperature_displays(cr, data, config, &scaling_params);
    draw_temperature_bars(cr, data, config, &scaling_params);

    // Render labels only if temperatures are below 99°C
    if (data->temp_cpu < 99.0 && data->temp_gpu < 99.0)
    {
        cairo_set_font_size(cr, config->font_size_labels);
        set_cairo_color(cr, &config->font_color_label);
        draw_labels(cr, config, &scaling_params);
    }

    // Flush and check for errors
    cairo_surface_flush(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Cairo drawing error: %s",
                    cairo_status_to_string(cairo_status(cr)));
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    // Save PNG image
    cairo_status_t write_status = cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    int success = (write_status == CAIRO_STATUS_SUCCESS);

    if (!success)
    {
        log_message(LOG_ERROR, "Failed to write PNG image: %s", config->paths_image_coolerdash);
    }

    // Clean up
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return success;
}

/**
 * @brief Main entry point for display updates.
 * @details Collects sensor data, renders display, and sends to LCD device.
 */
void draw_combined_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config parameter");
        return;
    }

    // Get sensor data
    monitor_sensor_data_t sensor_data = {.temp_cpu = 0.0f, .temp_gpu = 0.0f};
    if (!get_temperature_monitor_data(config, &sensor_data))
    {
        log_message(LOG_WARNING, "Failed to retrieve temperature data");
        return;
    }

    // Get LCD device info
    char device_uid[128] = {0};
    char device_name[256] = {0};
    int screen_width = 0, screen_height = 0;

    const bool device_available = get_liquidctl_data(config, device_uid, sizeof(device_uid),
                                                     device_name, sizeof(device_name),
                                                     &screen_width, &screen_height);

    // Render display
    if (!render_display(config, &sensor_data))
    {
        log_message(LOG_ERROR, "Display rendering failed");
        return;
    }

    // Send to LCD if available
    if (is_session_initialized() && device_available && device_uid[0] != '\0')
    {
        const char *name = (device_name[0] != '\0') ? device_name : "Unknown Device";
        log_message(LOG_INFO, "Sending image to LCD: %s [%s]", name, device_uid);

        // Send image to LCD device
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);

        log_message(LOG_INFO, "LCD image uploaded successfully");
    }
    else
    {
        log_message(LOG_WARNING, "Skipping LCD upload - device not available");
    }
}