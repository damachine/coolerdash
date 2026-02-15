/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Display mode dispatcher and shared rendering utilities.
 * @details Routes to dual or circle rendering module, provides shared Cairo
 * helpers and sensor slot functions used by all display modes.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_sensor.h"
#include "circle.h"
#include "display.h"
#include "dual.h"

// Circle inscribe factor for circular displays (1/sqrt(2) ~ 0.7071)
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.7071067811865476
#endif

/**
 * @brief Convert color component into a 0.0-1.0 range.
 */
double cairo_color_convert(uint8_t color_component)
{
    return color_component / 255.0;
}

/**
 * @brief Set cairo source color from Color structure.
 */
void set_cairo_color(cairo_t *cr, const Color *color)
{
    cairo_set_source_rgb(cr, cairo_color_convert(color->r),
                         cairo_color_convert(color->g),
                         cairo_color_convert(color->b));
}

/**
 * @brief Calculate temperature fill width with bounds checking.
 */
int calculate_temp_fill_width(float temp_value, int max_width, float max_temp)
{
    if (temp_value <= 0.0f)
        return 0;

    const float ratio = fminf(temp_value / max_temp, 1.0f);
    return (int)(ratio * max_width);
}

/**
 * @brief Draw rounded rectangle path for temperature bars.
 */
void draw_rounded_rectangle_path(cairo_t *cr, int x, int y, int width,
                                 int height, double radius)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -DISPLAY_M_PI_2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0,
              DISPLAY_M_PI_2);
    cairo_arc(cr, x + radius, y + height - radius, radius, DISPLAY_M_PI_2,
              DISPLAY_M_PI);
    cairo_arc(cr, x + radius, y + radius, radius, DISPLAY_M_PI,
              1.5 * DISPLAY_M_PI);
    cairo_close_path(cr);
}

/**
 * @brief Draw degree symbol at calculated position with proper font scaling.
 */
void draw_degree_symbol(cairo_t *cr, double x, double y,
                        const struct Config *config)
{
    if (!cr || !config)
        return;
    cairo_set_font_size(cr, config->font_size_temp / 1.66);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, "°");
    cairo_set_font_size(cr, config->font_size_temp);
}

/**
 * @brief Create cairo surface and context for display rendering.
 * @details Creates ARGB32 surface with dimensions from config.
 */
cairo_t *create_cairo_context(const struct Config *config,
                              cairo_surface_t **surface)
{
    *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, config->display_width, config->display_height);
    if (!*surface || cairo_surface_status(*surface) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to create cairo surface");
        if (*surface)
            cairo_surface_destroy(*surface);
        *surface = NULL;
        return NULL;
    }

    cairo_t *cr = cairo_create(*surface);
    if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to create cairo context");
        if (cr)
            cairo_destroy(cr);
        cairo_surface_destroy(*surface);
        *surface = NULL;
        return NULL;
    }

    return cr;
}

/**
 * @brief Calculate dynamic scaling parameters based on display dimensions.
 * @details Display shape detection: NZXT Kraken 240x240=rect, >240=circular.
 */
void calculate_scaling_params(const struct Config *config,
                              ScalingParams *params, const char *device_name)
{
    const double base_width = 240.0;
    const double base_height = 240.0;

    params->scale_x = config->display_width / base_width;
    params->scale_y = config->display_height / base_height;
    const double scale_avg = (params->scale_x + params->scale_y) / 2.0;

    // Detect circular displays using device database with resolution info
    int is_circular_by_device = is_circular_display_device(
        device_name, config->display_width, config->display_height);

    // Check display_shape configuration
    if (strcmp(config->display_shape, "rectangular") == 0)
    {
        // Force rectangular (inscribe_factor = 1.0)
        params->is_circular = 0;
        params->inscribe_factor = 1.0;
        log_message(LOG_INFO, "Display shape forced to rectangular via config "
                              "(inscribe_factor: 1.0)");
    }
    else if (strcmp(config->display_shape, "circular") == 0)
    {
        // Force circular (inscribe_factor = M_SQRT1_2 ~ 0.7071)
        params->is_circular = 1;
        double cfg_inscribe;
        if (config->display_inscribe_factor == 0.0f)
            cfg_inscribe = M_SQRT1_2;
        else if (config->display_inscribe_factor > 0.0f &&
                 config->display_inscribe_factor <= 1.0f)
            cfg_inscribe = (double)config->display_inscribe_factor;
        else
            cfg_inscribe = M_SQRT1_2;
        params->inscribe_factor = cfg_inscribe;
        log_message(
            LOG_INFO,
            "Display shape forced to circular via config (inscribe_factor: %.4f)",
            params->inscribe_factor);
    }
    else if (config->force_display_circular)
    {
        // Legacy developer override (CLI --develop)
        params->is_circular = 1;
        {
            double cfg_inscribe;
            if (config->display_inscribe_factor == 0.0f)
                cfg_inscribe = M_SQRT1_2;
            else if (config->display_inscribe_factor > 0.0f &&
                     config->display_inscribe_factor <= 1.0f)
                cfg_inscribe = (double)config->display_inscribe_factor;
            else
                cfg_inscribe = M_SQRT1_2;
            params->inscribe_factor = cfg_inscribe;
        }
        log_message(LOG_INFO,
                    "Developer override active: forcing circular display detection "
                    "(device: %s)",
                    device_name ? device_name : "unknown");
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
            else if (config->display_inscribe_factor > 0.0f &&
                     config->display_inscribe_factor <= 1.0f)
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
    const double safe_area_width =
        config->display_width * params->inscribe_factor;
    const float content_scale = (config->display_content_scale_factor > 0.0f &&
                                 config->display_content_scale_factor <= 1.0f)
                                    ? config->display_content_scale_factor
                                    : 0.98f; // Fallback: 98%
    // Apply bar_width percentage
    const double bar_width_factor = (config->layout_bar_width > 0)
                                        ? (config->layout_bar_width / 100.0)
                                        : 0.98;
    params->safe_bar_width =
        (int)(safe_area_width * content_scale * bar_width_factor);
    params->safe_content_margin =
        (config->display_width - params->safe_bar_width) / 2.0;

    params->corner_radius = 8.0 * scale_avg;

    // Log detailed scaling calculations
    log_message(
        LOG_INFO,
        "Scaling: safe_area=%.0fpx, bar_width=%dpx (%.0f%%), margin=%.1fpx",
        safe_area_width, params->safe_bar_width, bar_width_factor * 100.0,
        params->safe_content_margin);
}

/**
 * @brief Check if a sensor slot is active.
 */
int slot_is_active(const char *slot_value)
{
    if (!slot_value || slot_value[0] == '\0')
        return 0;
    return strcmp(slot_value, "none") != 0;
}

/**
 * @brief Get sensor value for a slot.
 */
float get_slot_temperature(const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return 0.0f;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->value;

    return 0.0f;
}

/**
 * @brief Get display label for a sensor slot.
 * @details Returns custom label from SensorConfig if set, otherwise
 * uses legacy names or sensor name from data.
 */
const char *get_slot_label(const struct Config *config,
                           const monitor_sensor_data_t *data,
                           const char *slot_value)
{
    if (!slot_value || slot_value[0] == '\0' || strcmp(slot_value, "none") == 0)
        return NULL;

    /* Check for custom label override in SensorConfig */
    if (config)
    {
        const SensorConfig *sc = get_sensor_config(config, slot_value);
        if (sc && sc->label[0] != '\0')
            return sc->label;
    }

    /* Legacy labels */
    if (strcmp(slot_value, "cpu") == 0)
        return "CPU";
    if (strcmp(slot_value, "gpu") == 0)
        return "GPU";
    if (strcmp(slot_value, "liquid") == 0)
        return "LIQ";

    /* Dynamic: use sensor name */
    if (data)
    {
        const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
        if (entry)
            return entry->name;
    }

    return "???";
}

/**
 * @brief Get bar color for a sensor slot based on value.
 * @details Uses SensorConfig thresholds via get_sensor_config().
 */
Color get_slot_bar_color(const struct Config *config, const char *slot_value,
                         float value)
{
    Color default_color = {0, 255, 0, 1};

    if (!config || !slot_value)
        return default_color;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (!sc)
        return default_color;

    if (value < sc->threshold_1)
        return sc->threshold_1_bar;
    else if (value < sc->threshold_2)
        return sc->threshold_2_bar;
    else if (value < sc->threshold_3)
        return sc->threshold_3_bar;
    else
        return sc->threshold_4_bar;
}

/**
 * @brief Get maximum scale for a sensor slot.
 */
float get_slot_max_scale(const struct Config *config, const char *slot_value)
{
    if (!config)
        return 115.0f;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (sc && sc->max_scale > 0.0f)
        return sc->max_scale;

    return 115.0f;
}

/**
 * @brief Get bar height for a specific slot.
 */
uint16_t get_slot_bar_height(const struct Config *config, const char *slot_name)
{
    if (!config || !slot_name)
        return 24;

    if (strcmp(slot_name, "up") == 0)
        return config->layout_bar_height_up;
    else if (strcmp(slot_name, "mid") == 0)
        return config->layout_bar_height_mid;
    else if (strcmp(slot_name, "down") == 0)
        return config->layout_bar_height_down;

    return config->layout_bar_height;
}

/**
 * @brief Get display unit string for a sensor slot.
 */
const char *get_slot_unit(const monitor_sensor_data_t *data,
                          const char *slot_value)
{
    if (!data || !slot_value)
        return "\xC2\xB0"
               "C";

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->unit;

    return "\xC2\xB0"
           "C";
}

/**
 * @brief Check if sensor should display decimal values.
 */
int get_slot_use_decimal(const monitor_sensor_data_t *data,
                         const char *slot_value)
{
    if (!data || !slot_value)
        return 0;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return entry->use_decimal;

    return 0;
}

/**
 * @brief Get display X offset for a sensor slot.
 */
int get_slot_offset_x(const struct Config *config, const char *slot_value)
{
    if (!config || !slot_value)
        return 0;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    return sc ? sc->offset_x : 0;
}

/**
 * @brief Get display Y offset for a sensor slot.
 */
int get_slot_offset_y(const struct Config *config, const char *slot_value)
{
    if (!config || !slot_value)
        return 0;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    return sc ? sc->offset_y : 0;
}

/**
 * @brief Check if a sensor slot is a temperature sensor.
 */
int get_slot_is_temp(const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return 1; /* Default to temp */

    /* Legacy slots are always temperature */
    if (strcmp(slot_value, "cpu") == 0 || strcmp(slot_value, "gpu") == 0 ||
        strcmp(slot_value, "liquid") == 0)
        return 1;

    const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
    if (entry)
        return (entry->category == SENSOR_CATEGORY_TEMP) ? 1 : 0;

    return 1;
}

/**
 * @brief Get font size for a sensor slot.
 * @details Returns per-sensor font size if configured (> 0), otherwise global.
 */
float get_slot_font_size(const struct Config *config, const char *slot_value)
{
    if (!config)
        return 100.0f;

    const SensorConfig *sc = get_sensor_config(config, slot_value);
    if (sc && sc->font_size_temp > 0.0f)
        return sc->font_size_temp;

    return config->font_size_temp;
}

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
