/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief LCD rendering and image upload implementation for CoolerDash.
 * @details Implements all display rendering logic, including temperature bars, labels, and image upload.
 * @example
 *     See function documentation for usage examples.
 */

// Include project headers
#include "../include/display.h"
#include "../include/config.h"
#include "../include/coolercontrol.h"
#include "monitor.h"

// Include necessary headers
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include <sys/stat.h>

// Constants for display layout
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

// Layout constants
#define TEMP_DISPLAY_X_OFFSET 22
#define TEMP_DISPLAY_Y_OFFSET 22
#define CORNER_RADIUS 8.0
#define LABEL_Y_OFFSET_1 8
#define LABEL_Y_OFFSET_2 15

/**
 * @brief Forward declarations for internal display rendering functions.
 * @details These functions are only used internally in display.c for modular rendering logic.
 * @example
 *     // Not intended for direct use; see render_display() and draw_combined_image().
 */

static int should_update_display(const sensor_data_t *data, const Config *config);
static inline void draw_temp(cairo_t *cr, const Config *config, double temp_value, double y_offset);
static void draw_temperature_displays(cairo_t *cr, const sensor_data_t *data, const Config *config);
static void draw_temperature_bars(cairo_t *cr, const sensor_data_t *data, const Config *config);
static Color get_temperature_bar_color(const Config *config, float val);
static void draw_single_temperature_bar(cairo_t *cr, const Config *config, float temp_value, int bar_x, int bar_y);
static void draw_labels(cairo_t *cr, const Config *config);

/**
 * @brief Check if display update is needed (change detection).
 * @details Compares current sensor data with last drawn values and determines if a redraw is necessary. Uses static variables for last data and first run detection. Returns 1 if update is needed, 0 otherwise.
 * @example
 *     if (should_update_display(&sensor_data, config)) {
 *         // redraw
 *     }
 */
static int should_update_display(const sensor_data_t *data, const Config *config) {
    static sensor_data_t last_data = {.temp_1 = 0.1f, .temp_2 = 0.1f};
    static bool first_run = true;
    
    if (first_run) {
        first_run = false;
        last_data = *data;
        return 1;
    }
    
    // Update if either temperature changed significantly
    const bool temp_1_changed = fabsf(last_data.temp_1 - data->temp_1) >= config->temp_1_update_threshold;
    const bool temp_2_changed = fabsf(last_data.temp_2 - data->temp_2) >= config->temp_2_update_threshold;
    
    if (temp_1_changed || temp_2_changed) {
        last_data = *data;
        return 1;
    }
    
    return 0;
}

/**
 * @brief Render display based on sensor data.
 * @details Renders the LCD display image using the provided sensor data. Handles drawing, saving, and uploading the image.
 * @example
 *     int result = render_display(&config, &sensor_data);
 */
int render_display(const Config *config, const sensor_data_t *data) {
    if (!data || !config) return 0;

    // Only update if sensor data changed significantly
    if (!should_update_display(data, config)) {
        return 1; // No update needed, but no error
    }

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 
                                                     config->display_width, 
                                                     config->display_height);
    if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        if (surface) cairo_surface_destroy(surface);
        return 0;
    }

    cairo_t *cr = cairo_create(surface);
    if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return 0;
    }

    // Fill background with black
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);

    // Set font and color for temperature values
    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    cairo_set_source_rgb(cr, config->font_color_temp.r / 255.0, 
                         config->font_color_temp.g / 255.0, 
                         config->font_color_temp.b / 255.0);
    
    draw_temperature_displays(cr, data, config);
    draw_temperature_bars(cr, data, config);

    // Set font and color for labels
    if (config->font_size_labels != config->font_size_temp ||
        memcmp(&config->font_color_label, &config->font_color_temp, sizeof(Color)) != 0) {
        cairo_set_font_size(cr, config->font_size_labels);
        cairo_set_source_rgb(cr, config->font_color_label.r / 255.0, 
                             config->font_color_label.g / 255.0, 
                             config->font_color_label.b / 255.0);
    }
    draw_labels(cr, config);

    // Ensure image directory exists
    struct stat st = {0};
    if (stat(config->paths_images, &st) == -1) {
        mkdir(config->paths_images, 0755);
    }

    // Save PNG image
    int success = (cairo_surface_write_to_png(surface, config->paths_image_coolerdash) == CAIRO_STATUS_SUCCESS);
    if (success) {
        fflush(NULL); // Ensure PNG is written before upload
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    return success;
}

/**
 * @brief Draw a single temperature value at the given y offset.
 * @details Helper for draw_temperature_displays. Draws the temperature string centered in its box.
 * @example
 *     draw_temp(cr, config, temp_value, y_offset);
 */
static inline void draw_temp(cairo_t *cr, const Config *config, double temp_value, double y_offset)
{
    char temp_str[8];
    cairo_text_extents_t ext;
    snprintf(temp_str, sizeof(temp_str), "%d°", (int)temp_value);
    cairo_text_extents(cr, temp_str, &ext);
    const double x = (config->layout_box_width - ext.width) / 2 + TEMP_DISPLAY_X_OFFSET;
    const double y = y_offset + (config->layout_box_height + ext.height) / 2;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, temp_str);
}

/**
 * @brief Draw temperature displays (large numbers for CPU, GPU).
 * @details Draws the temperature values for CPU and GPU in their respective boxes according to the 240x240px layout. CPU and GPU temperatures are centered in their boxes.
 * @example
 *     draw_temperature_displays(cr, &sensor_data);
 */
static void draw_temperature_displays(cairo_t *cr, const sensor_data_t *data, const Config *config) {
    // temp_1 display (was CPU temperature)
    draw_temp(cr, config, data->temp_1, -TEMP_DISPLAY_Y_OFFSET);
    
    // temp_2 display (was GPU temperature)
    draw_temp(cr, config, data->temp_2, config->layout_box_height + TEMP_DISPLAY_Y_OFFSET);
}

/**
 * @brief Draw temperature bars (temp_1 and temp_2).
 * @details Draws horizontal bars representing CPU and GPU temperatures, with color gradient according to temperature value. Uses cairo for drawing, clamps values to valid range, and handles rounded corners. No resources are allocated in this function.
 * @example
 *     draw_temperature_bars(cr, &sensor_data);
 */
static void draw_temperature_bars(cairo_t *cr, const sensor_data_t *data, const Config *config) {
    // Calculate horizontal position (centered)
    const int bar_x = (config->display_width - config->layout_bar_width) / 2;
    // Calculate vertical positions for CPU and GPU bars
    const int cpu_bar_y = (config->display_height - (2 * config->layout_bar_height + config->layout_bar_gap)) / 2 + 1;
    const int gpu_bar_y = cpu_bar_y + config->layout_bar_height + config->layout_bar_gap;
    
    draw_single_temperature_bar(cr, config, data->temp_1, bar_x, cpu_bar_y);
    draw_single_temperature_bar(cr, config, data->temp_2, bar_x, gpu_bar_y);
}

/**
 * @brief Draw a single temperature bar at the specified position.
 * @details Helper function that draws background, fill, and border for a temperature bar with rounded corners.
 * @example
 *     draw_single_temperature_bar(cr, config, temp_value, bar_x, bar_y);
 */
static void draw_single_temperature_bar(cairo_t *cr, const Config *config, float temp_value, int bar_x, int bar_y) {
    Color color = get_temperature_bar_color(config, temp_value);
    const int temp_val_w = (temp_value > 0.0f) ? 
        (int)((temp_value / 100.0f) * config->layout_bar_width) : 0; // Calculate filled width
    const int safe_temp_val_w = (temp_val_w < 0) ? 0 : 
        (temp_val_w > config->layout_bar_width) ? config->layout_bar_width : temp_val_w; // Clamp to valid range
    
    // Draw bar background (rounded corners)
    cairo_set_source_rgb(cr, config->layout_bar_color_background.r / 255.0, config->layout_bar_color_background.g / 255.0, config->layout_bar_color_background.b / 255.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, -M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, 0, M_PI_2);
    cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, M_PI_2, M_PI);
    cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Draw bar fill (temperature color, rounded)
    cairo_set_source_rgb(cr, color.r/255.0, color.g/255.0, color.b/255.0);
    double fill_width = safe_temp_val_w;
    cairo_new_sub_path(cr);
    if (fill_width > 2 * CORNER_RADIUS) {
        cairo_arc(cr, bar_x + fill_width - CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, -M_PI_2, 0);
        cairo_arc(cr, bar_x + fill_width - CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, 0, M_PI_2);
        cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, M_PI_2, M_PI);
        cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, M_PI, 1.5 * M_PI);
    } else {
        // If fill is too small, draw as rectangle
        cairo_rectangle(cr, bar_x, bar_y, fill_width, config->layout_bar_height);
    }
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Draw bar border (rounded)
    cairo_set_line_width(cr, config->layout_bar_border_width);
    cairo_set_source_rgb(cr, config->layout_bar_color_border.r / 255.0, config->layout_bar_color_border.g / 255.0, config->layout_bar_color_border.b / 255.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, -M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, 0, M_PI_2);
    cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, M_PI_2, M_PI);
    cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

/**
 * @brief Calculate color gradient for temperature bars (green → orange → hot orange → red).
 * @details Determines the RGB color for a given temperature value according to the defined thresholds from config.
 * @example
 *     Color color = get_temperature_bar_color(&config, 65.0f);
 */
static Color get_temperature_bar_color(const Config *config, float val) {
    // Temperature threshold and color mapping table
    const struct {
        float threshold;
        Color color;
    } temp_ranges[] = {
        {config->temp_threshold_1, config->temp_threshold_1_bar},
        {config->temp_threshold_2, config->temp_threshold_2_bar},
        {config->temp_threshold_3, config->temp_threshold_3_bar},
        {INFINITY, config->temp_threshold_4_bar}
    };
    
    // Find the appropriate color range for the given temperature
    for (size_t i = 0; i < sizeof(temp_ranges) / sizeof(temp_ranges[0]); i++) {
        if (val <= temp_ranges[i].threshold) {
            return temp_ranges[i].color;
        }
    }
    
    return config->temp_threshold_4_bar; // fallback
}

/**
 * @brief Draw CPU/GPU labels (default mode only).
 * @details Draws text labels for CPU and GPU in the default style. Uses cairo for font and color settings. No resources are allocated in this function.
 * @example
 *     draw_labels(cr);
 */
static void draw_labels(cairo_t *cr, const Config *config) {
    // Font und Farbe werden im Haupt-Rendering gesetzt!
    // CPU label: left in top box
    cairo_move_to(cr, 0, config->layout_box_height / 2 + config->font_size_labels / 2 + LABEL_Y_OFFSET_1);
    cairo_show_text(cr, "CPU");
    // GPU label: left in bottom box
    cairo_move_to(cr, 0, config->layout_box_height + config->layout_box_height / 2 + config->font_size_labels / 2 - LABEL_Y_OFFSET_2);
    cairo_show_text(cr, "GPU");
}

/**
 * @brief Collects sensor data and renders display (default mode only).
 * @details Reads all relevant sensor data (CPU and GPU temperatures) and renders the display image. Also uploads the image to the device if available. Handles errors silently and frees all resources. Main entry point for display updates in default mode.
 * @example
 *     draw_combined_image(&config);
 */
void draw_combined_image(const Config *config) {
    sensor_data_t sensor_data = {0};
    cc_sensor_data_t cc_data = {0};
    
    // Early return if sensor data retrieval fails
    if (!monitor_get_sensor_data(config, &cc_data)) {
        return;
    }
    
    // Copy temperature data
    sensor_data.temp_1 = cc_data.temp_1;
    sensor_data.temp_2 = cc_data.temp_2;
    
    // Render display and send to LCD if all conditions are met
    if (render_display(config, &sensor_data) && 
        is_session_initialized() && 
        cc_data.device_uid[0] != '\0') {
        send_image_to_lcd(config, config->paths_image_coolerdash, cc_data.device_uid);
        send_image_to_lcd(config, config->paths_image_coolerdash, cc_data.device_uid); // Send twice to ensure upload and no artfact is displayed
    }
}