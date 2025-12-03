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
#include "../device/sys.h"
#include "dual.h"
#include "../srv/cc_main.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_sensor.h"

// Circle inscribe factor for circular displays (1/√2 ≈ 0.7071)
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.7071067811865476
#endif

// Dynamic positioning factors
#define LABEL_MARGIN_FACTOR 0.02

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
 * @param temp_value Current temperature value
 * @param max_width Maximum width of the bar in pixels
 * @param max_temp Maximum temperature from configuration (highest threshold)
 */
static inline int calculate_temp_fill_width(float temp_value, int max_width, float max_temp)
{
    if (temp_value <= 0.0f)
        return 0;

    const float ratio = fminf(temp_value / max_temp, 1.0f);
    return (int)(ratio * max_width);
}

/**
 * @brief Dynamic scaling parameters structure
 */
typedef struct
{
    double scale_x;
    double scale_y;
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

    // Check display_shape configuration
    if (strcmp(config->display_shape, "rectangular") == 0)
    {
        // Force rectangular (inscribe_factor = 1.0)
        params->is_circular = 0;
        params->inscribe_factor = 1.0;
        log_message(LOG_INFO, "Display shape forced to rectangular via config (inscribe_factor: 1.0)");
    }
    else if (strcmp(config->display_shape, "circular") == 0)
    {
        // Force circular (inscribe_factor = M_SQRT1_2 ≈ 0.7071)
        params->is_circular = 1;
        double cfg_inscribe;
        if (config->display_inscribe_factor == 0.0f)
            cfg_inscribe = M_SQRT1_2; // user 'auto'
        else if (config->display_inscribe_factor > 0.0f && config->display_inscribe_factor <= 1.0f)
            cfg_inscribe = (double)config->display_inscribe_factor;
        else
            cfg_inscribe = M_SQRT1_2; // fallback
        params->inscribe_factor = cfg_inscribe;
        log_message(LOG_INFO, "Display shape forced to circular via config (inscribe_factor: %.4f)", params->inscribe_factor);
    }
    else if (config->force_display_circular)
    {
        // Legacy developer override (CLI --develop)
        params->is_circular = 1;
        {
            double cfg_inscribe;
            if (config->display_inscribe_factor == 0.0f)
                cfg_inscribe = M_SQRT1_2;
            else if (config->display_inscribe_factor > 0.0f && config->display_inscribe_factor <= 1.0f)
                cfg_inscribe = (double)config->display_inscribe_factor;
            else
                cfg_inscribe = M_SQRT1_2;
            params->inscribe_factor = cfg_inscribe;
        }
        log_message(LOG_INFO, "Developer override active: forcing circular display detection (device: %s)", device_name ? device_name : "unknown");
    }
    else
    {
        // Auto-detection based on device database
        params->is_circular = is_circular_by_device;
        if (params->is_circular)
        {
            double cfg_inscribe;
            if (config->display_inscribe_factor == 0.0f)
                cfg_inscribe = M_SQRT1_2;
            else if (config->display_inscribe_factor > 0.0f && config->display_inscribe_factor <= 1.0f)
                cfg_inscribe = (double)config->display_inscribe_factor;
            else
                cfg_inscribe = M_SQRT1_2;
            params->inscribe_factor = cfg_inscribe;
        }
        else
        {
            params->inscribe_factor = 1.0;
        }
    }

    // Calculate safe area width
    const double safe_area_width = config->display_width * params->inscribe_factor;
    const float content_scale = (config->display_content_scale_factor > 0.0f && config->display_content_scale_factor <= 1.0f)
                                    ? config->display_content_scale_factor
                                    : 0.98f; // Fallback: 98%
    // Apply bar_width percentage (default 98% = 1% margin left+right)
    const double bar_width_factor = (config->layout_bar_width > 0) ? (config->layout_bar_width / 100.0) : 0.98;
    params->safe_bar_width = (int)(safe_area_width * content_scale * bar_width_factor);
    params->safe_content_margin = (config->display_width - params->safe_bar_width) / 2.0;

    params->corner_radius = 8.0 * scale_avg;

    // Log detailed scaling calculations (verbose only)
    log_message(LOG_INFO, "Scaling: safe_area=%.0fpx, bar_width=%dpx (%.0f%%), margin=%.1fpx",
                safe_area_width, params->safe_bar_width, bar_width_factor * 100.0, params->safe_content_margin);
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
// Helper to draw degree symbol at calculated position with proper font scaling
static void draw_degree_symbol(cairo_t *cr, double x, double y, const struct Config *config)
{
    if (!cr || !config)
        return;
    cairo_set_font_size(cr, config->font_size_temp / 1.66);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, "°");
    cairo_set_font_size(cr, config->font_size_temp);
}

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

    // Calculate bar positions (same as in draw_temperature_bars)
    const int total_height = 2 * config->layout_bar_height + config->layout_bar_gap;
    const int start_y = (config->display_height - total_height) / 2;
    const int cpu_bar_y = start_y;
    const int gpu_bar_y = start_y + config->layout_bar_height + config->layout_bar_gap;

    char cpu_num_str[16], gpu_num_str[16];
    snprintf(cpu_num_str, sizeof(cpu_num_str), "%d", (int)data->temp_cpu);
    snprintf(gpu_num_str, sizeof(gpu_num_str), "%d", (int)data->temp_gpu);

    cairo_text_extents_t cpu_num_ext, gpu_num_ext;
    cairo_text_extents(cr, cpu_num_str, &cpu_num_ext);
    cairo_text_extents(cr, gpu_num_str, &gpu_num_ext);

    cairo_font_extents_t font_ext;
    cairo_font_extents(cr, &font_ext);

    // Use actual text extents for positioning to handle 3-digit temperatures correctly
    // For values ≥100, use actual width; for <100, use fixed width of "88" for stability
    double cpu_width = (data->temp_cpu >= 100.0f) ? cpu_num_ext.width : cpu_num_ext.width;
    double gpu_width = (data->temp_gpu >= 100.0f) ? gpu_num_ext.width : gpu_num_ext.width;

    // Calculate reference width (widest 2-digit number) for sub-100 alignment
    cairo_text_extents_t ref_width_ext;
    cairo_text_extents(cr, "88", &ref_width_ext);

    // Use reference width for values <100 to keep position stable
    if (data->temp_cpu < 100.0f)
        cpu_width = ref_width_ext.width;
    if (data->temp_gpu < 100.0f)
        gpu_width = ref_width_ext.width;

    // Center-align based on actual or reference width
    double cpu_temp_x = bar_x + (effective_bar_width - cpu_width) / 2.0;
    double gpu_temp_x = bar_x + (effective_bar_width - gpu_width) / 2.0;

    // Default offset for small displays (240x240): +20px to the right
    if (config->display_width == 240 && config->display_height == 240)
    {
        cpu_temp_x += 20;
        gpu_temp_x += 20;
    }

    // Apply user-defined X offsets if set (not -9999)
    if (config->display_temp_offset_x_cpu != -9999)
        cpu_temp_x += config->display_temp_offset_x_cpu;
    if (config->display_temp_offset_x_gpu != -9999)
        gpu_temp_x += config->display_temp_offset_x_gpu;

    // Position temperatures 1px outside bars (same calculation as labels)
    // CPU temperature - above bar (same as CPU label)
    double cpu_temp_y = cpu_bar_y + 8 - font_ext.descent;

    // GPU temperature - below bar (same as GPU label)
    double gpu_temp_y = gpu_bar_y + config->layout_bar_height - 4 + font_ext.ascent;

    // Apply user-defined Y offsets if set
    if (config->display_temp_offset_y_cpu != -9999)
        cpu_temp_y += config->display_temp_offset_y_cpu;
    if (config->display_temp_offset_y_gpu != -9999)
        gpu_temp_y += config->display_temp_offset_y_gpu;

    // Draw CPU temperature number
    cairo_move_to(cr, cpu_temp_x, cpu_temp_y);
    cairo_show_text(cr, cpu_num_str);

    // Draw GPU temperature number
    cairo_move_to(cr, gpu_temp_x, gpu_temp_y);
    cairo_show_text(cr, gpu_num_str);

    // Draw degree symbols at configurable offset (default: 16px right, bound to temperature position)
    cairo_set_font_size(cr, config->font_size_temp / 1.66);

    // Position degree symbols using configured spacing (default: 16px)
    const int degree_spacing = (config->display_degree_spacing > 0) ? config->display_degree_spacing : 16;
    double degree_cpu_x = cpu_temp_x + cpu_width + degree_spacing;
    double degree_gpu_x = gpu_temp_x + gpu_width + degree_spacing;
    double degree_cpu_y = cpu_temp_y - cpu_num_ext.height * 0.40;
    double degree_gpu_y = gpu_temp_y - gpu_num_ext.height * 0.40;

    draw_degree_symbol(cr, degree_cpu_x, degree_cpu_y, config);
    draw_degree_symbol(cr, degree_gpu_x, degree_gpu_y, config);
}

/**
 * @brief Draw a single temperature bar with background, fill, and border
 */
static void draw_single_temperature_bar(cairo_t *cr, const struct Config *config, const ScalingParams *params, float temp_value, int bar_x, int bar_y, int bar_width)
{
    if (!cr || !config || !params)
        return;

    // Use configured maximum temperature for bar scaling (default: 115°C)
    const float max_temp = config->temp_max_scale;
    const int fill_width = calculate_temp_fill_width(temp_value, bar_width, max_temp);

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
    cairo_set_line_width(cr, config->layout_bar_border);
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
    // if (config->display_width > 240 || config->display_height > 240)
    //    return;

    const int total_height = 2 * config->layout_bar_height + config->layout_bar_gap;
    const int start_y = (config->display_height - total_height) / 2;
    const int cpu_bar_y = start_y;
    const int gpu_bar_y = start_y + config->layout_bar_height + config->layout_bar_gap;

    // Labels: Configurable distance from left screen edge (default: 1%)
    const double left_margin_factor = (config->layout_label_margin_left > 0)
                                          ? (config->layout_label_margin_left / 100.0)
                                          : 0.01;
    double label_x = config->display_width * left_margin_factor;

    // Apply user-defined X offset if set
    if (config->display_label_offset_x != -9999)
    {
        label_x += config->display_label_offset_x;
    }

    cairo_font_extents_t font_ext;
    cairo_font_extents(cr, &font_ext);

    // Vertical spacing: Configurable distance from bars (default: 1%)
    const double bar_margin_factor = (config->layout_label_margin_bar > 0)
                                         ? (config->layout_label_margin_bar / 100.0)
                                         : 0.01;
    const double label_spacing = config->display_height * bar_margin_factor;

    // CPU label - 1% above CPU bar
    double cpu_label_y = cpu_bar_y - label_spacing - font_ext.descent;

    // GPU label - 1% below GPU bar
    double gpu_label_y = gpu_bar_y + config->layout_bar_height + label_spacing + font_ext.ascent;

    // Apply user-defined Y offset if set
    if (config->display_label_offset_y != -9999)
    {
        cpu_label_y += config->display_label_offset_y;
        gpu_label_y += config->display_label_offset_y;
    }

    cairo_move_to(cr, label_x, cpu_label_y);
    cairo_show_text(cr, "CPU");

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
 * @brief Display rendering - creates surface, renders content, saves PNG (Dual mode - CPU+GPU)
 */
int render_dual_display(const struct Config *config, const monitor_sensor_data_t *data, const char *device_name)
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
 * @brief High-level entry point for dual mode rendering.
 * @details Collects sensor data, renders dual display, and sends to LCD device.
 */
void draw_dual_image(const struct Config *config)
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

    // Render dual display with device name for circular display detection
    if (!render_dual_display(config, &sensor_data, device_name))
    {
        log_message(LOG_ERROR, "Dual display rendering failed");
        return;
    }

    // Send to LCD if available
    if (is_session_initialized() && device_available && device_uid[0] != '\0')
    {
        const char *name = (device_name[0] != '\0') ? device_name : "Unknown Device";
        log_message(LOG_INFO, "Sending dual image to LCD: %s [%s]", name, device_uid);

        // Send image to LCD device
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);

        log_message(LOG_INFO, "Dual LCD image uploaded successfully");
    }
    else
    {
        log_message(LOG_WARNING, "Skipping dual LCD upload - device not available");
    }
}

/**
 * @brief Main entry point for display updates with mode selection.
 * @details Collects sensor data and dispatches to dual or circle mode renderer based on configuration.
 */
void draw_combined_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config parameter for draw_combined_image");
        return;
    }

    // Check display mode and dispatch to appropriate renderer
    if (config->display_mode[0] != '\0' && strcmp(config->display_mode, "circle") == 0)
    {
        // Circle mode: alternating single sensor display
        extern void draw_circle_image(const struct Config *config);
        draw_circle_image(config);
    }
    else
    {
        // Dual mode (default): simultaneous CPU+GPU display
        draw_dual_image(config);
    }
}