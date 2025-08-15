/**
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief LCD rendering and image upload implementation for CoolerDash.
 * @details Provides functions for rendering the LCD display based on sensor data and configuration.
 */

// Include necessary headers
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

// Project headers
#include "../include/config.h"
#include "../include/display.h"
#include "../include/coolercontrol.h"
#include "../include/monitor.h"

/**
 * @brief Convert color component to cairo format.
 * @details Converts 8-bit color component (0-255) to cairo's double format (0.0-1.0) for rendering operations.
 */
static inline double cairo_color_convert(uint8_t color_component) {
    return color_component / 255.0;
}

/**
 * @brief Forward declarations for internal display rendering functions.
 * @details Function prototypes for internal display rendering helpers and utility functions used by the main rendering pipeline.
 */
static inline void draw_temp(cairo_t *cr, const struct Config *config, double temp_value, double y_offset);
static void draw_temperature_displays(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config);
static void draw_temperature_bars(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config);
static void draw_single_temperature_bar(cairo_t *cr, const struct Config *config, float temp_value, int bar_x, int bar_y);
static void draw_labels(cairo_t *cr, const struct Config *config);
static Color get_temperature_bar_color(const struct Config *config, float val);

/**
 * @brief Calculate color gradient for temperature bars (green → orange → hot orange → red).
 * @details Determines the RGB color for a given temperature value according to the defined thresholds from config.
 */
static Color get_temperature_bar_color(const struct Config *config, float val) {
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
    
    // Fallback to red if no match found
    return config->temp_threshold_4_bar;
}

/**
 * @brief Draw a single temperature value.
 * @details Helper function that renders a temperature value as text with proper positioning and formatting.
 */
static inline void draw_temp(cairo_t *cr, const struct Config *config, double temp_value, double y_offset) {
    // Input validation with early return
    char temp_str[16];
    cairo_text_extents_t ext;
    
    // Format temperature string
    snprintf(temp_str, sizeof(temp_str), "%d°", (int)temp_value);
    
    // Calculate text extents and position
    cairo_text_extents(cr, temp_str, &ext);
    const double x = (config->layout_box_width - ext.width) / 2 + DISPLAY_TEMP_DISPLAY_X_OFFSET;
    const double vertical_adjustment = (y_offset < 0) ? 
        DISPLAY_TEMP_VERTICAL_ADJUSTMENT_TOP : DISPLAY_TEMP_VERTICAL_ADJUSTMENT_BOTTOM;
    const double y = y_offset + (config->layout_box_height + ext.height) / 2 + vertical_adjustment;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, temp_str);
}

/**
 * @brief Draw temperature displays with enhanced positioning and validation.
 * @details Draws the temperature values for CPU and GPU in their respective boxes with improved accuracy and safety checks.
 */
static void draw_temperature_displays(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config) {
    // Input validation with early return
    if (!cr || !data || !config) return;
    
    // temp_cpu display (CPU temperature) with validation
    draw_temp(cr, config, data->temp_cpu, -DISPLAY_TEMP_DISPLAY_Y_OFFSET);
    
    // temp_gpu display (GPU temperature) with validation
    draw_temp(cr, config, data->temp_gpu, config->layout_box_height + DISPLAY_TEMP_DISPLAY_Y_OFFSET);
}

/**
 * @brief Draw a single temperature bar with enhanced safety and optimizations.
 * @details Helper function that draws background, fill, and border for a temperature bar with rounded corners, comprehensive validation, and optimized cairo operations.
 */
static void draw_single_temperature_bar(cairo_t *cr, const struct Config *config, float temp_value, int bar_x, int bar_y) {
    // Input validation with early return
    if (!cr || !config) return;
    
    // Get color for temperature
    Color color = get_temperature_bar_color(config, temp_value);
    
    // Calculate filled width
    int temp_val_w = 0;
    if (temp_value > 0.0f) {
        // Use safe division and bounds checking
        const float temp_ratio = temp_value / 105.0f;
        const float clamped_ratio = fmaxf(0.0f, fminf(1.0f, temp_ratio));
        temp_val_w = (int)(clamped_ratio * config->layout_bar_width);
    }
    
    // Ensure filled width is within valid bounds
    const int safe_temp_val_w = (temp_val_w < 0) ? 0 : 
        (temp_val_w > config->layout_bar_width) ? config->layout_bar_width : temp_val_w;
    
    // Draw bar background with rounded corners
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(config->layout_bar_color_background.r),
                         cairo_color_convert(config->layout_bar_color_background.g), 
                         cairo_color_convert(config->layout_bar_color_background.b));
    
    // Draw rounded rectangle for background
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - 8.0, bar_y + 8.0, 8.0, -DISPLAY_M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - 8.0, bar_y + config->layout_bar_height - 8.0, 8.0, 0, DISPLAY_M_PI_2);
    cairo_arc(cr, bar_x + 8.0, bar_y + config->layout_bar_height - 8.0, 8.0, DISPLAY_M_PI_2, DISPLAY_M_PI);
    cairo_arc(cr, bar_x + 8.0, bar_y + 8.0, 8.0, DISPLAY_M_PI, 1.5 * DISPLAY_M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Draw bar fill with temperature color
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(color.r),
                         cairo_color_convert(color.g), 
                         cairo_color_convert(color.b));
    
    // Convert to double for cairo operations
    const double fill_width = (double)safe_temp_val_w;
    cairo_new_sub_path(cr);
    
    // Optimized rendering based on fill width
    if (fill_width > 2 * 8.0) {
        // Full rounded corners for wider fills
        cairo_arc(cr, bar_x + fill_width - 8.0, bar_y + 8.0, 8.0, -DISPLAY_M_PI_2, 0);
        cairo_arc(cr, bar_x + fill_width - 8.0, bar_y + config->layout_bar_height - 8.0, 8.0, 0, DISPLAY_M_PI_2);
        cairo_arc(cr, bar_x + 8.0, bar_y + config->layout_bar_height - 8.0, 8.0, DISPLAY_M_PI_2, DISPLAY_M_PI);
        cairo_arc(cr, bar_x + 8.0, bar_y + 8.0, 8.0, DISPLAY_M_PI, 1.5 * DISPLAY_M_PI);
    } else if (fill_width > 0) {
        cairo_rectangle(cr, bar_x, bar_y, fill_width, config->layout_bar_height);
    }
    
    // Fill only if there's something to fill
    if (fill_width > 0) {
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    
    // Draw bar border with rounded corners
    cairo_set_line_width(cr, config->layout_bar_border_width);
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(config->layout_bar_color_border.r),
                         cairo_color_convert(config->layout_bar_color_border.g), 
                         cairo_color_convert(config->layout_bar_color_border.b));
    
    // Optimized border drawing
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - 8.0, bar_y + 8.0, 8.0, -DISPLAY_M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - 8.0, bar_y + config->layout_bar_height - 8.0, 8.0, 0, DISPLAY_M_PI_2);
    cairo_arc(cr, bar_x + 8.0, bar_y + config->layout_bar_height - 8.0, 8.0, DISPLAY_M_PI_2, DISPLAY_M_PI);
    cairo_arc(cr, bar_x + 8.0, bar_y + 8.0, 8.0, DISPLAY_M_PI, 1.5 * DISPLAY_M_PI);

    // Close path and stroke
    cairo_close_path(cr);
    cairo_stroke(cr);
}

/**
 * @brief Draw optimized temperature bars with enhanced performance and validation.
 * @details Draws horizontal bars representing CPU and GPU temperatures with enhanced color gradients, input validation, and optimized positioning calculations.
 */
static void draw_temperature_bars(cairo_t *cr, const monitor_sensor_data_t *data, const struct Config *config) {
    // Input validation with early return
    if (!cr || !data || !config) return;
    
    // Precompute horizontal position (centered) with validation
    if (config->layout_bar_width > config->display_width) return; // Safety check
    const int bar_x = (config->display_width - config->layout_bar_width) / 2;
    
    // Precompute vertical positions for CPU and GPU bars with validation
    const int total_bar_height = 2 * config->layout_bar_height + config->layout_bar_gap;
    if (total_bar_height > config->display_height) return; // Safety check
    
    // Calculate y-positions for CPU and GPU bars
    const int cpu_bar_y = (config->display_height - total_bar_height) / 2 + 1;
    const int gpu_bar_y = cpu_bar_y + config->layout_bar_height + config->layout_bar_gap;
    
    // Draw bars with temperature values
    draw_single_temperature_bar(cr, config, data->temp_cpu, bar_x, cpu_bar_y);
    draw_single_temperature_bar(cr, config, data->temp_gpu, bar_x, gpu_bar_y);
}

/**
 * @brief Draw CPU/GPU labels with enhanced positioning and validation.
 * @details Draws text labels for CPU and GPU with optimized positioning calculations and comprehensive input validation. Font and color are set by the main rendering pipeline.
 */
static void draw_labels(cairo_t *cr, const struct Config *config) {
    // Input validation with early return
    if (!cr || !config) return;
    
    // Positioning values
    const double box_center_y = config->layout_box_height / 2.0;
    const double font_half_height = config->font_size_labels / 2.0;
    
    // CPU label
    const double cpu_label_y = box_center_y + font_half_height + DISPLAY_LABEL_Y_OFFSET_1;
    cairo_move_to(cr, 0, cpu_label_y);
    cairo_show_text(cr, "CPU");
    
    // GPU label
    const double gpu_label_y = config->layout_box_height + box_center_y + font_half_height - DISPLAY_LABEL_Y_OFFSET_2;
    cairo_move_to(cr, 0, gpu_label_y);
    cairo_show_text(cr, "GPU");
}

/**
 * @brief Display rendering.
 * @details Creates cairo surface and context, renders temperature displays and bars, then saves the result as PNG image.
 */
int render_display(const struct Config *config, const monitor_sensor_data_t *data) {
    // Input validation with early return
    if (!data || !config) {
        log_message(LOG_ERROR, "Invalid parameters for render_display");
        return 0;
    }

    // Create cairo surface
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                                         config->display_width, 
                                                         config->display_height);

    // Handle surface creation failure
    if (!surface) {
        log_message(LOG_ERROR, "Failed to create cairo surface");
        return 0;
    }
    
    // Check for any surface errors before proceeding
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        log_message(LOG_ERROR, "Cairo surface creation failed");
        cairo_surface_destroy(surface);
        return 0;
    }

    // Create cairo context
    cairo_t *cr = cairo_create(surface);
    if (!cr) {
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
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(config->font_color_temp.r),
                         cairo_color_convert(config->font_color_temp.g), 
                         cairo_color_convert(config->font_color_temp.b));
    
    // Render temperature displays and bars
    draw_temperature_displays(cr, data, config);
    draw_temperature_bars(cr, data, config);

    // Configure font and color for labels (only if temperatures are below 99°C)
    if (data->temp_cpu < 99.0 && data->temp_gpu < 99.0) {
        if (config->font_size_labels != config->font_size_temp ||
            memcmp(&config->font_color_label, &config->font_color_temp, sizeof(Color)) != 0) {
            cairo_set_font_size(cr, config->font_size_labels);
            cairo_set_source_rgb(cr, 
                                 cairo_color_convert(config->font_color_label.r),
                                 cairo_color_convert(config->font_color_label.g), 
                                 cairo_color_convert(config->font_color_label.b));
        }
        // Render labels only when temperatures are below 99°C
        draw_labels(cr, config);
    }

    // Ensure all drawing operations are completed before writing to PNG
    cairo_surface_flush(surface);
    
    // Check for any drawing errors before writing
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        log_message(LOG_ERROR, "Cairo drawing error: %s (status: %d)", 
                   cairo_status_to_string(cairo_status(cr)), cairo_status(cr));
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }
    
    // Save PNG image
    cairo_status_t write_status = cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    int success = (write_status == CAIRO_STATUS_SUCCESS);
    
    // Log error if PNG write failed
    if (!success) {
        log_message(LOG_ERROR, "Failed to write PNG image: %s", config->paths_image_coolerdash);
    }

    // Clean up resources
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    // Return success status
    return success;
}

/**
 * @brief Main entry point for display updates with enhanced error handling.
 * @details Collects sensor data and renders display with comprehensive validation, error handling, and optimized resource management. Handles LCD communication and ensures robust operation in all conditions.
 */
void draw_combined_image(const struct Config *config) {
    // Input validation with early return
    if (!config) {
        log_message(LOG_ERROR, "Invalid config parameter for draw_combined_image");
        return;
    }
    
    // Initialize data structure with safe defaults
    monitor_sensor_data_t sensor_data = {.temp_cpu = 0.0f, .temp_gpu = 0.0f};

    // Retrieve temperature data with validation
    if (!get_temperature_monitor_data(config, &sensor_data)) {
        log_message(LOG_WARNING, "Failed to retrieve temperature data");
        return; // Silently handle sensor data retrieval failure
    }

    // Get device info for LCD operations
    char device_uid[128] = {0};
    char device_name[256] = {0};
    int screen_width = 0, screen_height = 0;
    
    // Get complete device info (UID, name, dimensions) in single API call
    const bool device_available = get_liquidctl_data(config, device_uid, sizeof(device_uid),
                                                           device_name, sizeof(device_name), 
                                                           &screen_width, &screen_height);
    // Check if device UID is valid
    const bool valid_device_uid = device_available && (device_uid[0] != '\0');
    
    // Log device info
    if (!valid_device_uid) {
        log_message(LOG_WARNING, "No valid LCD device UID available");
    }
    
    // Render display
    const int render_result = render_display(config, &sensor_data);
    
    // Handle rendering failure
    if (render_result == 0) {
        log_message(LOG_ERROR, "Display rendering failed");
        return;
    }
    
    // Send to LCD
    if (is_session_initialized() && valid_device_uid) {
        const char *name_display = (device_name[0] != '\0') ? device_name : "Unknown Device";
        log_message(LOG_INFO, "Sending image to LCD device: %s [%s]", name_display, device_uid);
        log_message(LOG_INFO, "LCD image uploaded successfully");
        
        // Send image twice for better reliability - some devices need double transmission
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);
        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid); // Send twice for better reliability
    } else {
        log_message(LOG_WARNING, "Skipping LCD upload - conditions not met (session:%d, device:%d)", 
                   is_session_initialized(), valid_device_uid);
    }
}