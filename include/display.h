/**
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief LCD rendering and image upload interface for CoolerDash.
 * @details Provides functions for rendering temperature data to LCD displays, including Cairo-based image generation and upload to CoolerControl devices.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

// Display constants
#define DISPLAY_LABEL_Y_OFFSET_1 8
#define DISPLAY_LABEL_Y_OFFSET_2 15
#define DISPLAY_TEMP_DISPLAY_X_OFFSET 22
#define DISPLAY_TEMP_DISPLAY_Y_OFFSET 22

// Mathematical constants
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

#include <stddef.h>
#include <stdint.h>

struct Config;

/**
 * @brief Sensor data structure for display rendering.
 * @details Contains temperature values (temp_1 and temp_2) used for rendering LCD display content with appropriate color coding and bar charts.
 */
typedef struct {
    float temp_1;
    float temp_2;
} sensor_data_t;

/**
 * @brief Render display based on sensor data and configuration.
 * @details Creates PNG image using Cairo graphics library based on temperature sensor data and configuration settings, then uploads to LCD device.
 */
int render_display(const struct Config *config, const sensor_data_t *data);

/**
 * @brief Collects sensor data and renders display.
 * @details High-level function that retrieves temperature data from monitoring subsystem and triggers display rendering with current configuration.
 */
void draw_combined_image(const struct Config *config);

#endif // DISPLAY_H
