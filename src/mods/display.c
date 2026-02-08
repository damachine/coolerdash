/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Display mode dispatcher.
 * @details Routes to dual or circle rendering module, shared sensor helpers.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "../srv/cc_sensor.h"
#include "circle.h"
#include "display.h"
#include "dual.h"

// ============================================================================
// Sensor Slot Helper Functions
// ============================================================================

/**
 * @brief Check if a sensor slot is active (not "none")
 */
int slot_is_active(const char *slot_value)
{
    if (!slot_value || slot_value[0] == '\0')
        return 0;
    return strcmp(slot_value, "none") != 0;
}

/**
 * @brief Get temperature value for a sensor slot
 */
float get_slot_temperature(const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return 0.0f;

    if (strcmp(slot_value, "cpu") == 0)
        return data->temp_cpu;
    else if (strcmp(slot_value, "gpu") == 0)
        return data->temp_gpu;
    else if (strcmp(slot_value, "liquid") == 0)
        return data->temp_liquid;

    return 0.0f;
}

/**
 * @brief Get display label for a sensor slot
 */
const char *get_slot_label(const char *slot_value)
{
    if (!slot_value || slot_value[0] == '\0')
        return NULL;

    if (strcmp(slot_value, "cpu") == 0)
        return "CPU";
    else if (strcmp(slot_value, "gpu") == 0)
        return "GPU";
    else if (strcmp(slot_value, "liquid") == 0)
        return "LIQ";
    else if (strcmp(slot_value, "none") == 0)
        return NULL;

    return NULL;
}

/**
 * @brief Get bar color for a sensor slot based on temperature
 */
Color get_slot_bar_color(const struct Config *config, const char *slot_value, float temperature)
{
    if (!config || !slot_value)
    {
        // Return default green color
        Color default_color = {0, 255, 0, 1};
        return default_color;
    }

    // Liquid uses separate thresholds
    if (strcmp(slot_value, "liquid") == 0)
    {
        if (temperature < config->temp_liquid_threshold_1)
            return config->temp_liquid_threshold_1_bar;
        else if (temperature < config->temp_liquid_threshold_2)
            return config->temp_liquid_threshold_2_bar;
        else if (temperature < config->temp_liquid_threshold_3)
            return config->temp_liquid_threshold_3_bar;
        else
            return config->temp_liquid_threshold_4_bar;
    }

    // GPU uses separate thresholds
    if (strcmp(slot_value, "gpu") == 0)
    {
        if (temperature < config->temp_gpu_threshold_1)
            return config->temp_gpu_threshold_1_bar;
        else if (temperature < config->temp_gpu_threshold_2)
            return config->temp_gpu_threshold_2_bar;
        else if (temperature < config->temp_gpu_threshold_3)
            return config->temp_gpu_threshold_3_bar;
        else
            return config->temp_gpu_threshold_4_bar;
    }

    // CPU (default) uses separate thresholds
    if (temperature < config->temp_cpu_threshold_1)
        return config->temp_cpu_threshold_1_bar;
    else if (temperature < config->temp_cpu_threshold_2)
        return config->temp_cpu_threshold_2_bar;
    else if (temperature < config->temp_cpu_threshold_3)
        return config->temp_cpu_threshold_3_bar;
    else
        return config->temp_cpu_threshold_4_bar;
}

/**
 * @brief Get maximum scale for a sensor slot
 */
float get_slot_max_scale(const struct Config *config, const char *slot_value)
{
    if (!config)
        return 115.0f;

    // Liquid has its own max scale (typically lower, e.g., 50°C)
    if (slot_value && strcmp(slot_value, "liquid") == 0)
        return config->temp_liquid_max_scale;

    // GPU has its own max scale
    if (slot_value && strcmp(slot_value, "gpu") == 0)
        return config->temp_gpu_max_scale;

    // CPU (default) max scale
    return config->temp_cpu_max_scale;
}

/**
 * @brief Get bar height for a specific slot
 */
uint16_t get_slot_bar_height(const struct Config *config, const char *slot_name)
{
    if (!config || !slot_name)
        return 24; // Default fallback

    if (strcmp(slot_name, "up") == 0)
        return config->layout_bar_height_up;
    else if (strcmp(slot_name, "mid") == 0)
        return config->layout_bar_height_mid;
    else if (strcmp(slot_name, "down") == 0)
        return config->layout_bar_height_down;

    return config->layout_bar_height; // Fallback to global
}

// ============================================================================
// Display Dispatcher
// ============================================================================

/**
 * @brief Main display dispatcher - routes to appropriate rendering mode.
 * @details Examines display_mode configuration and dispatches to either dual or
 * circle renderer. This provides a clean separation between mode selection
 * logic and mode-specific rendering.
 * @param config Configuration containing display mode and rendering parameters
 */
void draw_display_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config parameter for draw_display_image");
        return;
    }

    // Check display mode and dispatch to appropriate renderer
    if (config->display_mode[0] != '\0' &&
        strcmp(config->display_mode, "circle") == 0)
    {
        // Circle mode: alternating single sensor display
        draw_circle_image(config);
    }
    else
    {
        // Dual mode (default): simultaneous CPU+GPU display
        draw_dual_image(config);
    }
}
