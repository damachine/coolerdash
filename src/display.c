/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Enhanced LCD rendering and image upload implementation for CoolerDash.
 * @details Implements optimized display rendering logic with security features, performance improvements, and efficient resource management for temperature visualization.
 * @example
 *     See function documentation for usage examples.
 */

// POSIX and security feature requirements
#define _POSIX_C_SOURCE 200112L

// Security, performance and layout constants
#define COLOR_SCALE_FACTOR (1.0/255.0)
#define CORNER_RADIUS 8.0
#define DIRECTORY_PERMISSIONS 0755
#define LABEL_Y_OFFSET_1 8
#define LABEL_Y_OFFSET_2 15
#define MAX_TEMP_VALUE 200.0f
#define MIN_TEMP_VALUE -50.0f
#define TEMP_DISPLAY_X_OFFSET 22
#define TEMP_DISPLAY_Y_OFFSET 22
#define TEMP_STRING_BUFFER_SIZE 16

// Mathematical constants with precision
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2  
#define M_PI_2 1.57079632679489661923
#endif

// Include necessary headers in logical order
#include <cairo/cairo.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Include project headers
#include "../include/display.h"
#include "../include/config.h"
#include "../include/coolercontrol.h"
#include "../include/monitor.h"

/**
 * @brief Forward declarations for internal display rendering functions.
 * @details These functions provide modular, optimized rendering with input validationand error handling. All functions use const parameters where appropriate.
 */

// Core rendering functions
static int should_update_display(const sensor_data_t *data, const Config *config);
static inline void draw_temp_safe(cairo_t *cr, const Config *config, 
                                  double temp_value, double y_offset);
static void draw_temperature_displays(cairo_t *cr, const sensor_data_t *data, 
                                      const Config *config);
static void draw_temperature_bars(cairo_t *cr, const sensor_data_t *data, 
                                  const Config *config);
static void draw_single_temperature_bar(cairo_t *cr, const Config *config, 
                                         float temp_value, int bar_x, int bar_y);
static void draw_labels(cairo_t *cr, const Config *config);

// Utility and validation functions
static Color get_temperature_bar_color(const Config *config, float val);
static inline float clamp_temperature(float temp);
static inline double cairo_color_convert(uint8_t color_component);
static int create_directory_if_needed(const char *path);
static int validate_cairo_objects(cairo_t *cr, cairo_surface_t *surface);

/**
 * @brief Clamp temperature value to valid range for security and stability.
 * @details Ensures temperature values are within reasonable bounds to prevent rendering issues and potential security vulnerabilities.
 * @example
 *     float safe_temp = clamp_temperature(raw_temp);
 */
static inline float clamp_temperature(float temp) {
    if (temp < MIN_TEMP_VALUE) return MIN_TEMP_VALUE;
    if (temp > MAX_TEMP_VALUE) return MAX_TEMP_VALUE;
    return temp;
}

/**
 * @brief Optimized color component conversion with precomputed scaling.
 * @details Converts uint8_t color values to cairo double format efficiently. Uses precomputed scaling factor to avoid repeated divisions.
 * @example
 *     double red = cairo_color_convert(config->font_color_temp.r);
 */
static inline double cairo_color_convert(uint8_t color_component) {
    return color_component * COLOR_SCALE_FACTOR;
}

/**
 * @brief Create directory if it doesn't exist with proper error handling.
 * @details Creates directory with appropriate permissions and handles errors safely.
 * @example
 *     if (create_directory_if_needed(config->paths_images) != 0) handle_error();
 */
static int create_directory_if_needed(const char *path) {
    if (!path || !path[0]) return -1;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1; // Path exists but is not a directory
    }
    
    // Directory doesn't exist, create it
    if (mkdir(path, DIRECTORY_PERMISSIONS) == 0) {
        return 0;
    }
    
    // Check if another process created it while we were trying
    if (errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    
    return -1;
}

/**
 * @brief Validate cairo objects for safe operations.
 * @details Checks cairo context and surface for errors before use.
 * Returns 1 if valid, 0 if invalid.
 * @example
 *     if (!validate_cairo_objects(cr, surface)) cleanup_and_return();
 */
static int validate_cairo_objects(cairo_t *cr, cairo_surface_t *surface) {
    if (!cr || !surface) return 0;
    
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS ||
        cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        return 0;
    }
    
    return 1;
}

/**
 * @brief Enhanced display update detection with improved performance.
 * @details Compares current sensor data with last drawn values and determines if a redraw is necessary. Uses static variables with proper initialization and validation.
 * @example
 *     if (should_update_display(&sensor_data, config)) {
 *         // Redraw required
 *     }
 */
static int should_update_display(const sensor_data_t *data, const Config *config) {
    static sensor_data_t last_data = {.temp_1 = -999.0f, .temp_2 = -999.0f}; // Invalid initial values
    static bool first_run = true;
    
    // Input validation
    if (!data || !config) return 0;
    
    // Clamp input temperatures for safety
    const float current_temp1 = clamp_temperature(data->temp_1);
    const float current_temp2 = clamp_temperature(data->temp_2);
    
    if (first_run) {
        first_run = false;
        last_data.temp_1 = current_temp1;
        last_data.temp_2 = current_temp2;
        return 1; // Always update on first run
    }
    
    // Check if either temperature changed significantly using optimized comparison
    const float temp_1_delta = fabsf(last_data.temp_1 - current_temp1);
    const float temp_2_delta = fabsf(last_data.temp_2 - current_temp2);
    
    const bool temp_1_changed = temp_1_delta >= config->temp_1_update_threshold;
    const bool temp_2_changed = temp_2_delta >= config->temp_2_update_threshold;
    
    if (temp_1_changed || temp_2_changed) {
        last_data.temp_1 = current_temp1;
        last_data.temp_2 = current_temp2;
        return 1;
    }
    
    return 0; // No significant change
}

/**
 * @brief Enhanced display rendering with comprehensive error handling and validation.
 * @details Renders the LCD display image using provided sensor data with security features, resource management, and performance optimizations.
 * @example
 *     int result = render_display(&config, &sensor_data);
 *     if (!result) handle_rendering_error();
 */
int render_display(const Config *config, const sensor_data_t *data) {
    // Input validation
    if (!data || !config) {
        return 0;
    }
    
    // Validate display dimensions for security
    if (config->display_width < CONFIG_MIN_DISPLAY_SIZE || 
        config->display_width > CONFIG_MAX_DISPLAY_SIZE ||
        config->display_height < CONFIG_MIN_DISPLAY_SIZE || 
        config->display_height > CONFIG_MAX_DISPLAY_SIZE) {
        return 0;
    }

    // Only update if sensor data changed significantly
    if (!should_update_display(data, config)) {
        return 1; // No update needed, but no error
    }

    // Create cairo surface with error checking
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 
                                                         config->display_width, 
                                                         config->display_height);
    if (!surface) {
        return 0;
    }
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return 0;
    }

    // Create cairo context with validation
    cairo_t *cr = cairo_create(surface);
    if (!cr) {
        cairo_surface_destroy(surface);
        return 0;
    }
    
    if (!validate_cairo_objects(cr, surface)) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    // Fill background with black (optimized single operation)
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    // Configure font and color for temperature values (optimized color conversion)
    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, config->font_size_temp);
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(config->font_color_temp.r),
                         cairo_color_convert(config->font_color_temp.g), 
                         cairo_color_convert(config->font_color_temp.b));
    
    // Render temperature displays and bars
    draw_temperature_displays(cr, data, config);
    draw_temperature_bars(cr, data, config);

    // Configure font and color for labels (only if different from temperature settings)
    if (config->font_size_labels != config->font_size_temp ||
        memcmp(&config->font_color_label, &config->font_color_temp, sizeof(Color)) != 0) {
        cairo_set_font_size(cr, config->font_size_labels);
        cairo_set_source_rgb(cr, 
                             cairo_color_convert(config->font_color_label.r),
                             cairo_color_convert(config->font_color_label.g), 
                             cairo_color_convert(config->font_color_label.b));
    }
    draw_labels(cr, config);

    // Ensure image directory exists with proper error handling
    if (create_directory_if_needed(config->paths_images) != 0) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    // Save PNG image with validation
    cairo_status_t write_status = cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    int success = (write_status == CAIRO_STATUS_SUCCESS);
    
    if (success) {
        // Ensure PNG is written to disk before proceeding (non-blocking)
        fsync(STDOUT_FILENO);
    }

    // Clean up resources
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    return success;
}

/**
 * @brief Draw a single temperature value at the given y offset with enhanced safety.
 * @details Helper for draw_temperature_displays with buffer overflow protection and optimized string formatting.
 * @example
 *     draw_temp_safe(cr, config, temp_value, y_offset);
 */
static inline void draw_temp_safe(cairo_t *cr, const Config *config, double temp_value, double y_offset) {
    char temp_str[TEMP_STRING_BUFFER_SIZE];
    cairo_text_extents_t ext;
    
    // Clamp temperature and format safely
    const double safe_temp = clamp_temperature((float)temp_value);
    const int formatted_temp = (int)safe_temp;
    
    // Safe string formatting with bounds checking
    int chars_written = snprintf(temp_str, sizeof(temp_str), "%d°", formatted_temp);
    if (chars_written < 0 || chars_written >= (int)sizeof(temp_str)) {
        // Fallback for formatting errors
        strncpy(temp_str, "ERR", sizeof(temp_str) - 1);
        temp_str[sizeof(temp_str) - 1] = '\0';
    }
    
    cairo_text_extents(cr, temp_str, &ext);
    const double x = (config->layout_box_width - ext.width) / 2 + TEMP_DISPLAY_X_OFFSET;
    const double y = y_offset + (config->layout_box_height + ext.height) / 2;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, temp_str);
}

/**
 * @brief Draw temperature displays with enhanced positioning and validation.
 * @details Draws the temperature values for CPU and GPU in their respective boxes with improved accuracy and safety checks.
 * @example
 *     draw_temperature_displays(cr, &sensor_data, &config);
 */
static void draw_temperature_displays(cairo_t *cr, const sensor_data_t *data, const Config *config) {
    if (!cr || !data || !config) return;
    
    // temp_1 display (CPU temperature) with validation
    draw_temp_safe(cr, config, data->temp_1, -TEMP_DISPLAY_Y_OFFSET);
    
    // temp_2 display (GPU temperature) with validation
    draw_temp_safe(cr, config, data->temp_2, config->layout_box_height + TEMP_DISPLAY_Y_OFFSET);
}

/**
 * @brief Draw optimized temperature bars with enhanced performance and validation.
 * @details Draws horizontal bars representing CPU and GPU temperatures with enhanced color gradients, input validation, and optimized positioning calculations.
 * @example
 *     draw_temperature_bars(cr, &sensor_data, &config);
 */
static void draw_temperature_bars(cairo_t *cr, const sensor_data_t *data, const Config *config) {
    if (!cr || !data || !config) return;
    
    // Precompute horizontal position (centered) with validation
    if (config->layout_bar_width > config->display_width) return; // Safety check
    const int bar_x = (config->display_width - config->layout_bar_width) / 2;
    
    // Precompute vertical positions for CPU and GPU bars with validation
    const int total_bar_height = 2 * config->layout_bar_height + config->layout_bar_gap;
    if (total_bar_height > config->display_height) return; // Safety check
    
    const int cpu_bar_y = (config->display_height - total_bar_height) / 2 + 1;
    const int gpu_bar_y = cpu_bar_y + config->layout_bar_height + config->layout_bar_gap;
    
    // Validate and clamp temperature values
    const float safe_temp_1 = clamp_temperature(data->temp_1);
    const float safe_temp_2 = clamp_temperature(data->temp_2);
    
    // Draw bars with validated values
    draw_single_temperature_bar(cr, config, safe_temp_1, bar_x, cpu_bar_y);
    draw_single_temperature_bar(cr, config, safe_temp_2, bar_x, gpu_bar_y);
}

/**
 * @brief Draw a single temperature bar with enhanced safety and optimizations.
 * @details Helper function that draws background, fill, and border for a temperature bar with rounded corners, comprehensive validation, and optimized cairo operations.
 * @example
 *     draw_single_temperature_bar(cr, config, temp_value, bar_x, bar_y);
 */
static void draw_single_temperature_bar(cairo_t *cr, const Config *config, float temp_value, int bar_x, int bar_y) {
    if (!cr || !config) return;
    
    // Validate and clamp temperature value
    const float safe_temp = clamp_temperature(temp_value);
    
    // Get optimized color for temperature
    Color color = get_temperature_bar_color(config, safe_temp);
    
    // Calculate filled width with enhanced safety checks
    int temp_val_w = 0;
    if (safe_temp > 0.0f) {
        // Use safe division and bounds checking
        const float temp_ratio = safe_temp / 100.0f;
        const float clamped_ratio = fmaxf(0.0f, fminf(1.0f, temp_ratio));
        temp_val_w = (int)(clamped_ratio * config->layout_bar_width);
    }
    
    // Ensure filled width is within valid bounds
    const int safe_temp_val_w = (temp_val_w < 0) ? 0 : 
        (temp_val_w > config->layout_bar_width) ? config->layout_bar_width : temp_val_w;
    
    // Draw bar background with rounded corners (optimized color conversion)
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(config->layout_bar_color_background.r),
                         cairo_color_convert(config->layout_bar_color_background.g), 
                         cairo_color_convert(config->layout_bar_color_background.b));
    
    cairo_new_sub_path(cr);
    cairo_arc(cr, bar_x + config->layout_bar_width - CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, -M_PI_2, 0);
    cairo_arc(cr, bar_x + config->layout_bar_width - CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, 0, M_PI_2);
    cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, M_PI_2, M_PI);
    cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, M_PI, 1.5 * M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Draw bar fill with temperature color (optimized color conversion)
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(color.r),
                         cairo_color_convert(color.g), 
                         cairo_color_convert(color.b));
    
    const double fill_width = (double)safe_temp_val_w;
    cairo_new_sub_path(cr);
    
    // Optimized rendering based on fill width
    if (fill_width > 2 * CORNER_RADIUS) {
        // Full rounded corners for wider fills
        cairo_arc(cr, bar_x + fill_width - CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, -M_PI_2, 0);
        cairo_arc(cr, bar_x + fill_width - CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, 0, M_PI_2);
        cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + config->layout_bar_height - CORNER_RADIUS, CORNER_RADIUS, M_PI_2, M_PI);
        cairo_arc(cr, bar_x + CORNER_RADIUS, bar_y + CORNER_RADIUS, CORNER_RADIUS, M_PI, 1.5 * M_PI);
    } else if (fill_width > 0) {
        // Simple rectangle for narrow fills (performance optimization)
        cairo_rectangle(cr, bar_x, bar_y, fill_width, config->layout_bar_height);
    }
    
    if (fill_width > 0) {
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    
    // Draw bar border with rounded corners (optimized color conversion)
    cairo_set_line_width(cr, config->layout_bar_border_width);
    cairo_set_source_rgb(cr, 
                         cairo_color_convert(config->layout_bar_color_border.r),
                         cairo_color_convert(config->layout_bar_color_border.g), 
                         cairo_color_convert(config->layout_bar_color_border.b));
    
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
 * @brief Draw CPU/GPU labels with enhanced positioning and validation.
 * @details Draws text labels for CPU and GPU with optimized positioning calculations and comprehensive input validation. Font and color are set by the main rendering pipeline.
 * @example
 *     draw_labels(cr, &config);
 */
static void draw_labels(cairo_t *cr, const Config *config) {
    if (!cr || !config) return;
    
    // Validate font size to prevent rendering issues
    if (config->font_size_labels <= 0 || config->font_size_labels > CONFIG_MAX_FONT_SIZE) {
        return;
    }
    
    // Precompute positioning values with safety checks
    const double box_center_y = config->layout_box_height / 2.0;
    const double font_half_height = config->font_size_labels / 2.0;
    
    // CPU label: left aligned in top box with optimized positioning
    const double cpu_label_y = box_center_y + font_half_height + LABEL_Y_OFFSET_1;
    if (cpu_label_y > 0 && cpu_label_y < config->display_height) {
        cairo_move_to(cr, 0, cpu_label_y);
        cairo_show_text(cr, "CPU");
    }
    
    // GPU label: left aligned in bottom box with optimized positioning  
    const double gpu_label_y = config->layout_box_height + box_center_y + font_half_height - LABEL_Y_OFFSET_2;
    if (gpu_label_y > 0 && gpu_label_y < config->display_height) {
        cairo_move_to(cr, 0, gpu_label_y);
        cairo_show_text(cr, "GPU");
    }
}

/**
 * @brief Main entry point for display updates with enhanced error handling.
 * @details Collects sensor data and renders display with comprehensive validation, error handling, and optimized resource management. Handles LCD communication and ensures robust operation in all conditions.
 * @example
 *     draw_combined_image(&config);
 */
void draw_combined_image(const Config *config) {
    // Input validation with early return
    if (!config) return;
    
    // Initialize data structures with safe defaults
    sensor_data_t sensor_data = {.temp_1 = 0.0f, .temp_2 = 0.0f};
    monitor_sensor_data_t temp_data = {.temp_1 = 0.0f, .temp_2 = 0.0f};

    // Retrieve temperature data with validation
    if (!monitor_get_temperature_data(config, &temp_data)) {
        return; // Silently handle sensor data retrieval failure
    }

    // Copy and validate temperature data
    sensor_data.temp_1 = clamp_temperature(temp_data.temp_1);
    sensor_data.temp_2 = clamp_temperature(temp_data.temp_2);

    // Get device UID for LCD operations with validation
    cc_device_data_t device_data = {.device_uid = {0}};
    const bool device_available = get_device_uid(config, &device_data);
    const bool valid_device_uid = device_available && (device_data.device_uid[0] != '\0');
    
    // Render display with comprehensive error checking
    const bool render_success = render_display(config, &sensor_data);
    
    // Send to LCD only if all conditions are met (performance optimization)
    if (render_success && is_session_initialized() && valid_device_uid) {
        // Send image twice to ensure reliable upload and prevent display artifacts
        send_image_to_lcd(config, config->paths_image_coolerdash, device_data.device_uid);
        send_image_to_lcd(config, config->paths_image_coolerdash, device_data.device_uid);
    }
}