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

// Circle inscribe factor for circular displays (1/√2 ≈ 0.7071)
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.7071067811865476
#endif

// Dynamic positioning factors
#define LABEL_MARGIN_FACTOR 0.02
#define TEMP_SPACING_FACTOR 0.04
#define LABEL_Y_FINE_TUNE_FACTOR 0.02
#define CONTENT_SCALE_FACTOR 0.98
#define TEMP_EDGE_MARGIN_FACTOR 0.02

/**
 * @brief Convert color component to cairo format (0-255 to 0.0-1.0)
 */
static inline double cairo_color_convert(uint8_t color_component)
{
    return color_component / 255.0;
}

/**
 * @brief Set cairo color from Color structure
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
 */
static inline int calculate_temp_fill_width(float temp_value, int max_width)
{
    if (temp_value <= 0.0f)
        return 0;

    const float ratio = fminf(temp_value / 105.0f, 1.0f);
    return (int)(ratio * max_width);
}

/**
 * @brief Dynamic scaling parameters structure
 */
typedef struct
{
    double scale_x;
    double scale_y;
    double label_y_offset_1;
    double label_y_offset_2;
    double temp_display_x_offset;
    double temp_display_y_offset;
    double temp_vertical_adj_top;
    double temp_vertical_adj_bottom;
    double corner_radius;
    double inscribe_factor;     // 1.0 for rectangular, M_SQRT1_2 for circular
    int safe_bar_width;         // Safe bar width for circular displays
    double safe_content_margin; // Horizontal margin for safe content area
    int is_circular;            // 1 if circular display, 0 if rectangular
} ScalingParams;

/**
 * @brief Calculate dynamic scaling parameters based on display dimensions
 * @details Display shape detection: NZXT Kraken 240x240=rect, >240=circular
 */
static void calculate_scaling_params(const struct Config *config, ScalingParams *params, const char *device_name)
{
    const double base_width = 240.0;
    const double base_height = 240.0;

    params->scale_x = config->display_width / base_width;
    params->scale_y = config->display_height / base_height;
    const double scale_avg = (params->scale_x + params->scale_y) / 2.0;

    // Detect circular displays using device database with resolution info
    int is_circular_by_device = is_circular_display_device(device_name,
                                                           config->display_width,
                                                           config->display_height);
    params->is_circular = is_circular_by_device;
    params->inscribe_factor = params->is_circular ? M_SQRT1_2 : 1.0;

    // Calculate safe area width
    const double safe_area_width = config->display_width * params->inscribe_factor;
    params->safe_bar_width = (int)(safe_area_width * CONTENT_SCALE_FACTOR);
    params->safe_content_margin = (config->display_width - params->safe_bar_width) / 2.0;

    params->label_y_offset_1 = config->display_height * LABEL_Y_FINE_TUNE_FACTOR;
    params->label_y_offset_2 = config->display_height * LABEL_Y_FINE_TUNE_FACTOR;
    params->temp_display_x_offset = 0;
    params->temp_display_y_offset = config->display_height * TEMP_SPACING_FACTOR;
    params->temp_vertical_adj_top = 0;
    params->temp_vertical_adj_bottom = 0;
    params->corner_radius = 8.0 * scale_avg;
}

/**
 * @brief Forward declarations for internal display rendering functions
 */
static void draw_temperature_displays(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params);
static void draw_temperature_bars(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params);
static void draw_single_temperature_bar(cairo_t *cr, const struct Config *config, const ScalingParams *params, float temp_value, int bar_x, int bar_y, int bar_width);
static void draw_labels(cairo_t *cr, const struct Config *config, const ScalingParams *params);
static Color get_temperature_bar_color(const struct Config *config, float val);
static void draw_rounded_rectangle_path(cairo_t *cr, int x, int y, int width, int height, double radius);
static cairo_t *create_cairo_context(const struct Config *config, cairo_surface_t **surface);
static void render_display_content(cairo_t *cr, const struct Config *config, const monitor_sensor_data_t *data, const ScalingParams *params);

/**
 * @brief Draw rounded rectangle path for temperature bars
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
 * @brief Calculate color gradient for temperature bars (green → orange → red)
 */
static Color get_temperature_bar_color(const struct Config *config, float val)
{
    const struct
    {
        float threshold;
        Color color;
    } temp_ranges[] = {
        {config->temp_threshold_1, config->temp_threshold_1_bar},
        {config->temp_threshold_2, config->temp_threshold_2_bar},
        {config->temp_threshold_3, config->temp_threshold_3_bar},
        {INFINITY, config->temp_threshold_4_bar}};

    for (size_t i = 0; i < sizeof(temp_ranges) / sizeof(temp_ranges[0]); i++)
    {
        if (val <= temp_ranges[i].threshold)
            return temp_ranges[i].color;
    }

    return config->temp_threshold_4_bar;
}

/**
 * @brief Draw temperature displays for CPU and GPU
 */
static void draw_temperature_displays(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params)
{
    if (!cr || !data || !config || !params)
        return;

    const int effective_bar_width = params->safe_bar_width;
    const int bar_x = (config->display_width - effective_bar_width) / 2;

    char cpu_num_str[16], gpu_num_str[16];
    snprintf(cpu_num_str, sizeof(cpu_num_str), "%d", (int)data->temp_cpu);
    snprintf(gpu_num_str, sizeof(gpu_num_str), "%d", (int)data->temp_gpu);

    cairo_text_extents_t cpu_num_ext, gpu_num_ext;
    cairo_text_extents(cr, cpu_num_str, &cpu_num_ext);
    cairo_text_extents(cr, gpu_num_str, &gpu_num_ext);

    cairo_set_font_size(cr, config->font_size_temp / 2.0);
    cairo_text_extents_t space_ext, degree_ext;
    cairo_text_extents(cr, " ", &space_ext);
    cairo_text_extents(cr, "°", &degree_ext);
    cairo_set_font_size(cr, config->font_size_temp);

    double cpu_temp_x = bar_x + (effective_bar_width - cpu_num_ext.width) / 2.0;
    double gpu_temp_x = bar_x + (effective_bar_width - gpu_num_ext.width) / 2.0;

    // For small displays (≤240×240), shift right
    if (config->display_width <= 240 && config->display_height <= 240)
    {
        cpu_temp_x += 18;
        gpu_temp_x += 18;
    }

    const double edge_margin = config->display_height * TEMP_EDGE_MARGIN_FACTOR;
    const double cpu_temp_y = edge_margin + cpu_num_ext.height;
    const double gpu_temp_y = config->display_height - edge_margin;

    // Draw CPU temperature
    cairo_move_to(cr, cpu_temp_x, cpu_temp_y);
    cairo_show_text(cr, cpu_num_str);

    cairo_set_font_size(cr, config->font_size_temp / 2.0);
    cairo_move_to(cr, cpu_temp_x + cpu_num_ext.width + 6, cpu_temp_y - cpu_num_ext.height * 0.5);
    cairo_show_text(cr, "°");
    cairo_set_font_size(cr, config->font_size_temp);

    // Draw GPU temperature
    cairo_move_to(cr, gpu_temp_x, gpu_temp_y);
    cairo_show_text(cr, gpu_num_str);

    cairo_set_font_size(cr, config->font_size_temp / 2.0);
    cairo_move_to(cr, gpu_temp_x + gpu_num_ext.width + 6, gpu_temp_y - gpu_num_ext.height * 0.5);
    cairo_show_text(cr, "°");
    cairo_set_font_size(cr, config->font_size_temp);
}

/**
 * @brief Draw a single temperature bar with background, fill, and border
 */
static void draw_single_temperature_bar(cairo_t *cr, const struct Config *config, const ScalingParams *params, float temp_value, int bar_x, int bar_y, int bar_width)
{
    if (!cr || !config || !params)
        return;

    const int fill_width = calculate_temp_fill_width(temp_value, bar_width);

    // Background
    set_cairo_color(cr, &config->layout_bar_color_background);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, bar_width, config->layout_bar_height, params->corner_radius);
    cairo_fill(cr);

    // Fill
    if (fill_width > 0)
    {
        Color fill_color = get_temperature_bar_color(config, temp_value);
        set_cairo_color(cr, &fill_color);

        if (fill_width >= 16)
            draw_rounded_rectangle_path(cr, bar_x, bar_y, fill_width, config->layout_bar_height, params->corner_radius);
        else
            cairo_rectangle(cr, bar_x, bar_y, fill_width, config->layout_bar_height);

        cairo_fill(cr);
    }

    // Border
    cairo_set_line_width(cr, config->layout_bar_border_width);
    set_cairo_color(cr, &config->layout_bar_color_border);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, bar_width, config->layout_bar_height, params->corner_radius);
    cairo_stroke(cr);
}

/**
 * @brief Draw temperature bars for CPU and GPU
 */
static void draw_temperature_bars(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config, const ScalingParams *params)
{
    if (!cr || !data || !config || !params)
        return;

    const double bar_side_margin = config->display_width * 0.0025;
    const int effective_bar_width = params->safe_bar_width - (int)(2 * bar_side_margin);
    const int bar_x = (config->display_width - effective_bar_width) / 2;

    const int total_height = 2 * config->layout_bar_height + config->layout_bar_gap;
    const int start_y = (config->display_height - total_height) / 2;

    const int cpu_bar_y = start_y;
    const int gpu_bar_y = start_y + config->layout_bar_height + config->layout_bar_gap;

    draw_single_temperature_bar(cr, config, params, data->temp_cpu, bar_x, cpu_bar_y, effective_bar_width);
    draw_single_temperature_bar(cr, config, params, data->temp_gpu, bar_x, gpu_bar_y, effective_bar_width);
}

/**
 * @brief Draw CPU/GPU labels (only for displays ≤240×240)
 */
static void draw_labels(cairo_t *cr, const struct Config *config, const ScalingParams *params)
{
    if (!cr || !config || !params)
        return;

    // Only show labels on small displays
    if (config->display_width > 240 || config->display_height > 240)
        return;

    const int total_height = 2 * config->layout_bar_height + config->layout_bar_gap;
    const int start_y = (config->display_height - total_height) / 2;
    const int cpu_bar_y = start_y;
    const int gpu_bar_y = start_y + config->layout_bar_height + config->layout_bar_gap;

    double label_x = params->safe_content_margin + (config->display_width * LABEL_MARGIN_FACTOR);
    label_x -= 6;

    cairo_font_extents_t font_ext;
    cairo_font_extents(cr, &font_ext);

    // CPU label - above bar
    const double cpu_label_y = cpu_bar_y - 4 - font_ext.descent;
    cairo_move_to(cr, label_x, cpu_label_y);
    cairo_show_text(cr, "CPU");

    // GPU label - below bar
    const double gpu_label_y = gpu_bar_y + config->layout_bar_height + 4 + font_ext.ascent;
    cairo_move_to(cr, label_x, gpu_label_y);
    cairo_show_text(cr, "GPU");
}

/**
 * @brief Create cairo context and surface
 */
static cairo_t *create_cairo_context(const struct Config *config, cairo_surface_t **surface)
{
    *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                          config->display_width,
                                          config->display_height);
    if (!*surface || cairo_surface_status(*surface) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to create cairo surface");
        if (*surface)
            cairo_surface_destroy(*surface);
        return NULL;
    }

    cairo_t *cr = cairo_create(*surface);
    if (!cr)
    {
        log_message(LOG_ERROR, "Failed to create cairo context");
        cairo_surface_destroy(*surface);
        *surface = NULL;
    }

    return cr;
}

/**
 * @brief Render display content to cairo context
 */
static void render_display_content(cairo_t *cr, const struct Config *config, const monitor_sensor_data_t *data, const ScalingParams *params)
{
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    set_cairo_color(cr, &config->font_color_temp);

    draw_temperature_displays(cr, data, config, params);
    draw_temperature_bars(cr, data, config, params);

    // Labels only if temp < 99°C
    if (data->temp_cpu < 99.0 && data->temp_gpu < 99.0)
    {
        cairo_set_font_size(cr, config->font_size_labels);
        set_cairo_color(cr, &config->font_color_label);
        draw_labels(cr, config, params);
    }
}

/**
 * @brief Display rendering - creates surface, renders content, saves PNG
 */
int render_display(const struct Config *config, const monitor_sensor_data_t *data, const char *device_name)
{
    if (!data || !config)
    {
        log_message(LOG_ERROR, "Invalid parameters for render_display");
        return 0;
    }

    ScalingParams scaling_params;
    calculate_scaling_params(config, &scaling_params, device_name);

    // Log display shape detection
    if (scaling_params.is_circular)
    {
        log_message(LOG_INFO, "Circular display detected (device: %s, inscribe factor: %.4f)",
                    device_name ? device_name : "unknown", scaling_params.inscribe_factor);
    }
    else
    {
        log_message(LOG_INFO, "Rectangular display detected (device: %s, inscribe factor: %.4f)",
                    device_name ? device_name : "unknown", scaling_params.inscribe_factor);
    }

    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;

    render_display_content(cr, config, data, &scaling_params);

    cairo_surface_flush(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Cairo drawing error: %s",
                    cairo_status_to_string(cairo_status(cr)));
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    cairo_status_t write_status = cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    int success = (write_status == CAIRO_STATUS_SUCCESS);

    if (!success)
        log_message(LOG_ERROR, "Failed to write PNG image: %s", config->paths_image_coolerdash);

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

    // Render display with device name for circular display detection
    if (!render_display(config, &sensor_data, device_name))
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