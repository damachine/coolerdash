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
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cairo/cairo.h>
#include <string.h>

// Constants for display layout
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

/**
 * @brief Forward declarations for internal display rendering functions.
 * @details These functions are only used internally in display.c for modular rendering logic.
 * @example
 *     // Not intended for direct use; see render_display() and draw_combined_image().
 */

static void draw_temperature_displays(cairo_t *cr, const sensor_data_t *data, const Config *config);
static void draw_temperature_bars(cairo_t *cr, const sensor_data_t *data, const Config *config);
static void draw_color_temperature_bars(const Config *config, float val, int* r, int* g, int* b);
static void draw_labels(cairo_t *cr, const Config *config);
static int should_update_display(const sensor_data_t *data, const Config *config);

/**
 * @brief Render display based on sensor data (only default mode).
 * @details Renders the LCD display image using the provided sensor data. Handles drawing, saving, and uploading the image.
 * @example
 *     int result = render_display(&config, &sensor_data);
 */
int render_display(const Config *config, const sensor_data_t *data) {
    if (!data || !config) return 0;

    cairo_surface_t *surface = NULL;
    cairo_t *cr = NULL;
    int success = 0;

    // Only update if sensor data changed significantly
    if (!should_update_display(data, config)) {
        return 1; // No update needed, but no error
    }

    // Create Cairo surface for drawing
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, config->display_width, config->display_height);
    if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        goto cleanup;
    }

    cr = cairo_create(surface);
    if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        goto cleanup;
    }

    // Fill background with black
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);

    // Draw temperature values
    draw_temperature_displays(cr, data, config);

    // Draw temperature bars
    draw_temperature_bars(cr, data, config);

    // Draw labels (CPU/GPU)
    draw_labels(cr, config);

    // Ensure image directory exists
    struct stat st = {0};
    if (stat(config->paths_images, &st) == -1) {
        mkdir(config->paths_images, 0755);
    }

    // Save PNG image
    if (cairo_surface_write_to_png(surface, config->paths_image_coolerdash) == CAIRO_STATUS_SUCCESS) {
        fflush(NULL); // Ensure PNG is written before upload
        success = 1;
    }

cleanup:
    // Free Cairo resources
    if (cr) {
        cairo_destroy(cr); // Destroy Cairo context
        cr = NULL; // Reset context pointer
    }
    if (surface) {
        cairo_surface_destroy(surface); // Destroy Cairo surface
        surface = NULL; // Reset surface pointer
    }

    return success;
}

/**
 * @brief Draw temperature displays (large numbers for CPU, GPU).
 * @details Draws the temperature values for CPU and GPU in their respective boxes according to the 240x240px layout. CPU and GPU temperatures are centered in their boxes.
 * @example
 *     draw_temperature_displays(cr, &sensor_data);
 */
static void draw_temperature_displays(cairo_t *cr, const sensor_data_t *data, const Config *config) {
    // Set font and size
    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_source_rgb(cr, config->font_color_temp.r / 255.0, config->font_color_temp.g / 255.0, config->font_color_temp.b / 255.0);

    char temp_str[8];
    cairo_text_extents_t ext;

    // temp_1 display (was CPU temperature)
    snprintf(temp_str, sizeof(temp_str), "%d\xC2\xB0", (int)data->temp_1);
    cairo_set_font_size(cr, config->font_size_temp);
    cairo_text_extents(cr, temp_str, &ext);
    // Centered in top box (no bearing correction)
    const double temp_1_x = (config->layout_box_width - ext.width) / 2 + 22;
    const double temp_1_y = (config->layout_box_height + ext.height) / 2 - 22;
    cairo_move_to(cr, temp_1_x, temp_1_y);
    cairo_show_text(cr, temp_str);

    // temp_2 display (was GPU temperature)
    snprintf(temp_str, sizeof(temp_str), "%d\xC2\xB0", (int)data->temp_2);
    cairo_set_font_size(cr, config->font_size_temp);
    cairo_text_extents(cr, temp_str, &ext);
    // Centered in bottom box (no bearing correction)
    const double temp_2_x = (config->layout_box_width - ext.width) / 2 + 22;
    const double temp_2_y = config->layout_box_height + (config->layout_box_height + ext.height) / 2 + 22;
    cairo_move_to(cr, temp_2_x, temp_2_y);
    cairo_show_text(cr, temp_str);
}

/**
 * @brief Draw temperature bars (CPU and GPU).
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
    
    // --- temp_1 bar (was CPU bar) ---
    int r, g, b;
    draw_color_temperature_bars(config, data->temp_1, &r, &g, &b); // Get color for temp_1
    const int temp_1_val_w = (data->temp_1 > 0.0f) ? 
        (int)((data->temp_1 / 100.0f) * config->layout_bar_width) : 0; // Calculate filled width
    const int safe_temp_1_val_w = (temp_1_val_w < 0) ? 0 : 
        (temp_1_val_w > config->layout_bar_width) ? config->layout_bar_width : temp_1_val_w; // Clamp to valid range
    // Draw temp_1 bar background (rounded corners)
    double radius = 8.0; // Corner radius in px
    cairo_set_source_rgb(cr, config->layout_bar_color_background.r / 255.0, config->layout_bar_color_background.g / 255.0, config->layout_bar_color_background.b / 255.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, cpu_bar_y + radius, radius, -M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, cpu_bar_y + config->layout_bar_height - radius, radius, 0, M_PI_2);
    cairo_arc(cr, bar_x + radius, cpu_bar_y + config->layout_bar_height - radius, radius, M_PI_2, M_PI);
    cairo_arc(cr, bar_x + radius, cpu_bar_y + radius, radius, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);
    // Draw temp_1 bar fill (temperature color, rounded)
    cairo_set_source_rgb(cr, r/255.0, g/255.0, b/255.0);
    double fill_width = safe_temp_1_val_w;
    cairo_new_sub_path(cr);
    if (fill_width > 2 * radius) {
        cairo_arc(cr, bar_x + fill_width - radius, cpu_bar_y + radius, radius, -M_PI_2, 0);
        cairo_arc(cr, bar_x + fill_width - radius, cpu_bar_y + config->layout_bar_height - radius, radius, 0, M_PI_2);
        cairo_arc(cr, bar_x + radius, cpu_bar_y + config->layout_bar_height - radius, radius, M_PI_2, M_PI);
        cairo_arc(cr, bar_x + radius, cpu_bar_y + radius, radius, M_PI, 1.5 * M_PI);
    } else {
        // If fill is too small, draw as rectangle
        cairo_rectangle(cr, bar_x, cpu_bar_y, fill_width, config->layout_bar_height);
    }
    cairo_close_path(cr);
    cairo_fill(cr);
    // Draw temp_1 bar border (rounded)
    cairo_set_line_width(cr, config->layout_bar_border_width);
    cairo_set_source_rgb(cr, config->layout_bar_color_border.r / 255.0, config->layout_bar_color_border.g / 255.0, config->layout_bar_color_border.b / 255.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, cpu_bar_y + radius, radius, -M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, cpu_bar_y + config->layout_bar_height - radius, radius, 0, M_PI_2);
    cairo_arc(cr, bar_x + radius, cpu_bar_y + config->layout_bar_height - radius, radius, M_PI_2, M_PI);
    cairo_arc(cr, bar_x + radius, cpu_bar_y + radius, radius, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_stroke(cr);

    // --- temp_2 bar (was GPU bar) ---
    draw_color_temperature_bars(config, data->temp_2, &r, &g, &b); // Get color for temp_2
    const int temp_2_val_w = (data->temp_2 > 0.0f) ? 
        (int)((data->temp_2 / 100.0f) * config->layout_bar_width) : 0; // Calculate filled width
    const int safe_temp_2_val_w = (temp_2_val_w < 0) ? 0 : 
        (temp_2_val_w > config->layout_bar_width) ? config->layout_bar_width : temp_2_val_w; // Clamp to valid range
    // Draw temp_2 bar background (rounded corners)
    cairo_set_source_rgb(cr, config->layout_bar_color_background.r / 255.0, config->layout_bar_color_background.g / 255.0, config->layout_bar_color_background.b / 255.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, gpu_bar_y + radius, radius, -M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, gpu_bar_y + config->layout_bar_height - radius, radius, 0, M_PI_2);
    cairo_arc(cr, bar_x + radius, gpu_bar_y + config->layout_bar_height - radius, radius, M_PI_2, M_PI);
    cairo_arc(cr, bar_x + radius, gpu_bar_y + radius, radius, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);
    // Draw temp_2 bar fill (temperature color, rounded)
    cairo_set_source_rgb(cr, r/255.0, g/255.0, b/255.0);
    fill_width = safe_temp_2_val_w;
    cairo_new_sub_path(cr);
    if (fill_width > 2 * radius) {
        cairo_arc(cr, bar_x + fill_width - radius, gpu_bar_y + radius, radius, -M_PI_2, 0);
        cairo_arc(cr, bar_x + fill_width - radius, gpu_bar_y + config->layout_bar_height - radius, radius, 0, M_PI_2);
        cairo_arc(cr, bar_x + radius, gpu_bar_y + config->layout_bar_height - radius, radius, M_PI_2, M_PI);
        cairo_arc(cr, bar_x + radius, gpu_bar_y + radius, radius, M_PI, 1.5 * M_PI);
    } else {
        cairo_rectangle(cr, bar_x, gpu_bar_y, fill_width, config->layout_bar_height);
    }
    cairo_close_path(cr);
    cairo_fill(cr);
    // Draw temp_2 bar border (rounded)
    cairo_set_line_width(cr, config->layout_bar_border_width);
    cairo_set_source_rgb(cr, config->layout_bar_color_border.r / 255.0, config->layout_bar_color_border.g / 255.0, config->layout_bar_color_border.b / 255.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, gpu_bar_y + radius, radius, -M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - radius, gpu_bar_y + config->layout_bar_height - radius, radius, 0, M_PI_2);
    cairo_arc(cr, bar_x + radius, gpu_bar_y + config->layout_bar_height - radius, radius, M_PI_2, M_PI);
    cairo_arc(cr, bar_x + radius, gpu_bar_y + radius, radius, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

/**
 * @brief Calculate color gradient for temperature bars (green → orange → hot orange → red).
 * @details Determines the RGB color for a given temperature value according to the defined thresholds from config.
 * @example
 *     int r, g, b;
 *     draw_color_temperature_bars(&config, 65.0f, &r, &g, &b);
 */
static void draw_color_temperature_bars(const Config *config, float val, int* r, int* g, int* b) {
    if (val <= config->temp_threshold_1) {
        *r = config->temp_threshold_1_bar.r; *g = config->temp_threshold_1_bar.g; *b = config->temp_threshold_1_bar.b;
    } else if (val <= config->temp_threshold_2) {
        *r = config->temp_threshold_2_bar.r; *g = config->temp_threshold_2_bar.g; *b = config->temp_threshold_2_bar.b;
    } else if (val <= config->temp_threshold_3) {
        *r = config->temp_threshold_3_bar.r; *g = config->temp_threshold_3_bar.g; *b = config->temp_threshold_3_bar.b;
    } else {
        *r = config->temp_threshold_4_bar.r; *g = config->temp_threshold_4_bar.g; *b = config->temp_threshold_4_bar.b;
    }
}

/**
 * @brief Draw CPU/GPU labels (default mode only).
 * @details Draws text labels for CPU and GPU in the default style. Uses cairo for font and color settings. No resources are allocated in this function.
 * @example
 *     draw_labels(cr);
 */
static void draw_labels(cairo_t *cr, const Config *config) {
    // Set font and size
    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_labels);
    cairo_set_source_rgb(cr, config->font_color_label.r / 255.0, config->font_color_label.g / 255.0, config->font_color_label.b / 255.0);
    // CPU label: left in top box
    cairo_move_to(cr, + 0, config->layout_box_height / 2 + config->font_size_labels / 2 + 8);
    cairo_show_text(cr, "CPU");
    // GPU label: left in bottom box
    cairo_move_to(cr, + 0 ,config->layout_box_height + config->layout_box_height / 2 + config->font_size_labels / 2 - 15);
    cairo_show_text(cr, "GPU");
}

/**
 * @brief Check if display update is needed (change detection).
 * @details Compares current sensor data with last drawn values and determines if a redraw is necessary. Uses static variables for last data and first run detection. Returns 1 if update is needed, 0 otherwise.
 * @example
 *     if (should_update_display(&sensor_data, config)) {
 *         // redraw
 *     }
 */
static int should_update_display(const sensor_data_t *data, const Config *config) {
    (void)config;
    static sensor_data_t last_data = {.temp_1 = 1.0f, .temp_2 = 1.0f};
    static int first_run = 1;
    if (first_run) {
        first_run = 0;
        last_data = *data;
        return 1;
    }
    // Update if either temp_1 or temp_2 changed significantly
    if (last_data.temp_1 != data->temp_1 || last_data.temp_2 != data->temp_2) {
        last_data = *data;
        return 1;
    }
    return 0;
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
    if (monitor_get_sensor_data(config, &cc_data)) {
        sensor_data.temp_1 = cc_data.temp_1;
        sensor_data.temp_2 = cc_data.temp_2;
        int render_result = render_display(config, &sensor_data);
        if (render_result && is_session_initialized() && cc_data.device_uid[0] != '\0') {
            send_image_to_lcd(config, config->paths_image_coolerdash, cc_data.device_uid);
            send_image_to_lcd(config, config->paths_image_coolerdash, cc_data.device_uid);
        }
    }
}
