/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Enhanced LCD rendering and image upload interface for CoolerDash.
 * @details Provides secure, optimized interface for display rendering with validation and performance enhancements.
 * Enhanced with input validation, buffer overflow protection, and cache-friendly data structures.
 * @example
 *     See function documentation for usage examples.
 */

// POSIX and security feature requirements (must be first)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

// Function prototypes
#ifndef DISPLAY_H
#define DISPLAY_H

// Display constants for validation and security limits
#define DISPLAY_MAX_FILEPATH 512
#define DISPLAY_MAX_HEIGHT 4096
#define DISPLAY_MAX_WIDTH 4096
#define DISPLAY_MIN_HEIGHT 32
#define DISPLAY_MIN_WIDTH 32
#define DISPLAY_TEMP_INVALID -999.0f
#define DISPLAY_TEMP_MAX 150.0f
#define DISPLAY_TEMP_MIN -50.0f

// Security, performance and layout constants
#define DISPLAY_COLOR_SCALE_FACTOR (1.0/255.0)
#define DISPLAY_CORNER_RADIUS 8.0
#define DISPLAY_DIRECTORY_PERMISSIONS 0755
#define DISPLAY_LABEL_Y_OFFSET_1 8
#define DISPLAY_LABEL_Y_OFFSET_2 15
#define DISPLAY_MAX_TEMP_VALUE 200.0f
#define DISPLAY_MIN_TEMP_VALUE -50.0f
#define DISPLAY_TEMP_DISPLAY_X_OFFSET 22
#define DISPLAY_TEMP_DISPLAY_Y_OFFSET 22
#define DISPLAY_TEMP_STRING_BUFFER_SIZE 16

// Mathematical constants with precision
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

// Include minimal necessary headers
#include <stddef.h>
#include <stdint.h>

// Forward declarations to reduce compilation dependencies
struct Config;
struct CoolerControlSession;

/**
 * @brief Sensor data structure for display rendering.
 * @details Holds temperature values for temp_1 and temp_2 with validation support. Optimized for cache alignment and safe rendering operations.
 * @example
 *     sensor_data_t data = { .temp_1 = 55.0f, .temp_2 = 48.0f };
 *     if (display_validate_sensor_data(&data)) { ... }
 */
typedef struct __attribute__((aligned(8))) {  // 8-byte alignment for cache efficiency
    float temp_1;        // Temperature 1 in degrees Celsius (4 bytes)
    float temp_2;        // Temperature 2 in degrees Celsius (4 bytes)
    // Total: 8 bytes (cache-friendly size, aligned to 8-byte boundary)
} sensor_data_t;

/**
 * @brief Render display based on sensor data and configuration.
 * @details Renders the LCD display image using the provided sensor data and configuration. 
 * Handles drawing, saving, and uploading the image with comprehensive error checking.
 * Enhanced with input validation and buffer overflow protection.
 * @example
 *     if (render_display(&config, &sensor_data)) {
 *         // Success
 *     } else {
 *         // Handle error
 *     }
 */
int render_display(const struct Config *config, const sensor_data_t *data);

/**
 * @brief Enhanced display rendering with session management.
 * @details Collects sensor data and renders display with explicit session handling. Provides better error reporting and resource management than draw_combined_image.
 * @example
 *     if (draw_combined_image_safe(session, &config, &sensor_data)) { ... }
 */
int draw_combined_image_safe(struct CoolerControlSession *session, const struct Config *config, const sensor_data_t *data);

/**
 * @brief Legacy function: Collects sensor data and renders display.
 * @details Reads all relevant sensor data (temperatures) and renders the display image.
 * @example
 *     draw_combined_image(&config);  // Legacy usage
 */
void draw_combined_image(const struct Config *config);

/**
 * @brief Send shutdown image to display.
 * @details Displays a shutdown image on the LCD before daemon termination.
 * @example
 *     if (send_shutdown_image(session, "/path/to/shutdown.png")) { ... }
 */
int send_shutdown_image(struct CoolerControlSession *session, const char *image_path);

/**
 * @brief Validate sensor data for display rendering.
 * @details Checks if temperature values are within valid display range with enhanced validation.
 * @example
 *     if (display_validate_sensor_data(&data)) { ... }
 */
static inline int display_validate_sensor_data(const sensor_data_t *data) {
    return (data && 
            data->temp_1 >= DISPLAY_TEMP_MIN && data->temp_1 <= DISPLAY_TEMP_MAX &&
            data->temp_2 >= DISPLAY_TEMP_MIN && data->temp_2 <= DISPLAY_TEMP_MAX &&
            data->temp_1 != DISPLAY_TEMP_INVALID && data->temp_2 != DISPLAY_TEMP_INVALID);
}

/**
 * @brief Initialize sensor data structure with safe defaults.
 * @details Sets all temperature values to invalid state for safety.
 * @example
 *     sensor_data_t data;
 *     display_init_sensor_data(&data);
 */
static inline void display_init_sensor_data(sensor_data_t *data) {
    if (data) {
        data->temp_1 = DISPLAY_TEMP_INVALID;
        data->temp_2 = DISPLAY_TEMP_INVALID;
    }
}

/**
 * @brief Validate display dimensions.
 * @details Checks if width and height are within supported display limits.
 * @example
 *     if (display_validate_dimensions(width, height)) { ... }
 */
static inline int display_validate_dimensions(uint16_t width, uint16_t height) {
    return (width >= DISPLAY_MIN_WIDTH && width <= DISPLAY_MAX_WIDTH &&
            height >= DISPLAY_MIN_HEIGHT && height <= DISPLAY_MAX_HEIGHT);
}

/**
 * @brief Check if temperature is valid for display.
 * @details Validates single temperature value for display rendering.
 * @example
 *     if (display_is_temp_valid(temp)) { ... }
 */
static inline int display_is_temp_valid(float temperature) {
    return (temperature >= DISPLAY_TEMP_MIN && 
            temperature <= DISPLAY_TEMP_MAX &&
            temperature != DISPLAY_TEMP_INVALID);
}

/**
 * @brief Clamp temperature to valid display range.
 * @details Ensures temperature is within displayable bounds.
 * @example
 *     float safe_temp = display_clamp_temp(temp);
 */
static inline float display_clamp_temp(float temperature) {
    if (temperature < DISPLAY_TEMP_MIN) return DISPLAY_TEMP_MIN;
    if (temperature > DISPLAY_TEMP_MAX) return DISPLAY_TEMP_MAX;
    return temperature;
}

#endif // DISPLAY_H
