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

static double clamp_double(double value, double min_value, double max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

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
 * @brief Set cairo source color from Color structure with alpha.
 */
void set_cairo_color_alpha(cairo_t *cr, const Color *color, double alpha)
{
    cairo_set_source_rgba(cr, cairo_color_convert(color->r),
                          cairo_color_convert(color->g),
                          cairo_color_convert(color->b), alpha);
}

/**
 * @brief Scale a design-space X value based on display width.
 */
double scale_value_x(const ScalingParams *params, double value)
{
    if (!params)
        return value;
    return value * params->scale_x;
}

/**
 * @brief Scale a design-space Y value based on display height.
 */
double scale_value_y(const ScalingParams *params, double value)
{
    if (!params)
        return value;
    return value * params->scale_y;
}

/**
 * @brief Scale a design-space value using average display scaling.
 */
double scale_value_avg(const ScalingParams *params, double value)
{
    if (!params)
        return value;
    return value * params->scale_uniform;
}

/**
 * @brief Get effective degree symbol spacing for the current display.
 */
int get_scaled_degree_spacing(const struct Config *config,
                              const ScalingParams *params)
{
    const double base_spacing =
        (config && config->display_degree_spacing > 0)
            ? (double)config->display_degree_spacing
            : 16.0;

    const int scaled_spacing = (int)lround(scale_value_avg(params, base_spacing));
    return (scaled_spacing > 0) ? scaled_spacing : 1;
}

/**
 * @brief Get the scaled bar border width.
 */
double get_scaled_bar_border_width(const struct Config *config,
                                   const ScalingParams *params)
{
    if (!config || config->layout_bar_border <= 0.0f)
        return 0.0;

    const double scaled_border = scale_value_avg(params,
                                                 (double)config->layout_bar_border);
    return (scaled_border > 0.0) ? scaled_border : 0.0;
}

/**
 * @brief Get the preferred label font size.
 */
double get_preferred_label_font_size(const struct Config *config,
                                     const ScalingParams *params)
{
    if (!config)
        return 0.0;

    if (config->font_size_labels > 0.0f)
        return scale_value_avg(params, (double)config->font_size_labels);

    return scale_value_avg(params, 28.0);
}

/**
 * @brief Get the scaled global label X offset using uniform scaling.
 */
int get_scaled_label_offset_x(const struct Config *config,
                              const ScalingParams *params)
{
    if (!config)
        return 0;

    return (int)lround(scale_value_avg(
        params, (double)config->display_label_offset_x));
}

/**
 * @brief Get the scaled global label Y offset using uniform scaling.
 */
int get_scaled_label_offset_y(const struct Config *config,
                              const ScalingParams *params)
{
    if (!config)
        return 0;

    return (int)lround(scale_value_avg(
        params, (double)config->display_label_offset_y));
}

/**
 * @brief Get the scaled pixel bar gap.
 */
int get_scaled_bar_gap(const struct Config *config,
                       const ScalingParams *params)
{
    if (!config)
        return 0;

    const int scaled_gap = (int)lround(scale_value_avg(
        params, (double)config->layout_bar_gap));
    return (scaled_gap > 0) ? scaled_gap : 0;
}

/**
 * @brief Get the effective vertical spacing between bars and labels.
 */
double get_effective_label_spacing(const struct Config *config,
                                   const ScalingParams *params)
{
    if (!config)
        return 0.0;

    const double spacing_factor =
        config->layout_label_margin_bar / 100.0;
    const double min_dimension =
        (config->display_width < config->display_height)
            ? (double)config->display_width
            : (double)config->display_height;
    const double scaled_spacing = min_dimension * spacing_factor;
    const double minimum_spacing = scale_value_avg(params, 2.0);

    return fmax(minimum_spacing, scaled_spacing);
}

/**
 * @brief Format a sensor value with optional decimals.
 */
void format_slot_value_text(char *buffer, size_t buffer_size,
                            const monitor_sensor_data_t *data,
                            const char *slot_value, float temp_value)
{
    if (!buffer || buffer_size == 0)
        return;

    if (get_slot_use_decimal(data, slot_value))
        snprintf(buffer, buffer_size, "%.1f", temp_value);
    else
        snprintf(buffer, buffer_size, "%d", (int)temp_value);
}

/**
 * @brief Measure and optionally render a right-aligned sensor value block.
 */
void layout_and_render_slot_value(cairo_t *cr,
                                  const monitor_sensor_data_t *data,
                                  const struct Config *config,
                                  const ScalingParams *params,
                                  const char *slot_value,
                                  float temp_value, double box_x,
                                  double box_y, double box_width,
                                  double box_height, int align_bottom,
                                  int draw_output,
                                  SlotValueLayout *layout)
{
    if (!cr || !data || !config || !params || !slot_value || !layout ||
        box_width <= 0.0 || box_height <= 0.0)
        return;

    memset(layout, 0, sizeof(*layout));

    char value_text[16] = {0};
    format_slot_value_text(value_text, sizeof(value_text), data, slot_value,
                           temp_value);

    const int is_temp = get_slot_is_temp(data, slot_value);
    const double offset_x =
        scale_value_x(params, (double)get_slot_offset_x(config, slot_value));
    const double offset_y =
        scale_value_y(params, (double)get_slot_offset_y(config, slot_value));
    const int degree_spacing =
        is_temp ? ((get_scaled_degree_spacing(config, params) > 1)
                       ? (int)lround(get_scaled_degree_spacing(config, params) * 0.65)
                       : 1)
                : get_scaled_degree_spacing(config, params);
    const double temp_margin_factor =
        (config->layout_label_margin_left > 0)
            ? (config->layout_label_margin_left / 200.0)
            : 0.005;
    const double right_margin = fmax(0.0, box_width * temp_margin_factor);
    const double inner_padding_x = clamp_double(scale_value_avg(params, 0.75),
                                                0.0, box_width * 0.015);
    const double inner_padding_y = clamp_double(scale_value_avg(params, 0.25),
                                                0.0, box_height * 0.02);
    const double growth_factor =
        (config->font_growth_factor >= 1.0f)
            ? (double)config->font_growth_factor
            : 1.0;
    const double available_width =
        fmax(16.0, (box_width - inner_padding_x - right_margin) * growth_factor);
    const double available_height =
        fmax(12.0, (box_height - inner_padding_y) * growth_factor);
    const double configured_font_size = get_slot_font_size(config, slot_value);
    const double configured_scaled_font_size =
        (configured_font_size > 0.0)
            ? scale_value_avg(params, configured_font_size)
            : 0.0;
    const double auto_font_size = fmax(scale_value_avg(params, 220.0),
                                       available_height * 7.40);
    const double preferred_font_size =
        (configured_scaled_font_size > 0.0)
            ? configured_scaled_font_size
            : auto_font_size;
    const double min_font_size =
        (configured_scaled_font_size > 0.0)
            ? fmax(8.0, scale_value_avg(params, 8.0))
            : fmax(12.0, scale_value_avg(params, 14.0));

    cairo_text_extents_t number_ext = {0};
    cairo_text_extents_t degree_ext = {0};
    cairo_font_extents_t font_ext = {0};
    double font_size = preferred_font_size;
    double degree_font_size = font_size / 2.05;
    double total_width = 0.0;
    double number_width = 0.0;
    double content_height = 0.0;

    while (1)
    {
        cairo_set_font_size(cr, font_size);
        cairo_font_extents(cr, &font_ext);
        cairo_text_extents(cr, value_text, &number_ext);
        number_width = fmax(number_ext.x_advance, number_ext.width);

        degree_font_size = font_size / 2.05;
        degree_ext = (cairo_text_extents_t){0};
        if (is_temp)
        {
            cairo_set_font_size(cr, degree_font_size);
            cairo_text_extents(cr, "\xC2\xB0", &degree_ext);
            cairo_set_font_size(cr, font_size);
        }

        total_width = number_width +
                      (is_temp
                           ? (degree_spacing +
                              fmax(degree_ext.x_advance, degree_ext.width))
                           : 0.0);
        content_height = fmax(number_ext.height,
                              font_ext.ascent + font_ext.descent);

        if ((total_width <= available_width &&
             content_height <= available_height) ||
            font_size <= min_font_size)
            break;

        font_size *= 0.93;
        if (font_size < min_font_size)
            font_size = min_font_size;
    }

    if (configured_scaled_font_size <= 0.0)
    {
        double growth_step = fmax(0.5, scale_value_avg(params, 1.0));

        while (growth_step >= 0.25)
        {
            while (1)
            {
                const double candidate_font_size = font_size + growth_step;
                cairo_text_extents_t candidate_number_ext = {0};
                cairo_text_extents_t candidate_degree_ext = {0};
                cairo_font_extents_t candidate_font_ext = {0};
                double candidate_number_width = 0.0;
                double candidate_total_width = 0.0;
                double candidate_content_height = 0.0;
                double candidate_degree_font_size = candidate_font_size / 2.05;

                cairo_set_font_size(cr, candidate_font_size);
                cairo_font_extents(cr, &candidate_font_ext);
                cairo_text_extents(cr, value_text, &candidate_number_ext);
                candidate_number_width =
                    fmax(candidate_number_ext.x_advance,
                         candidate_number_ext.width);

                if (is_temp)
                {
                    cairo_set_font_size(cr, candidate_degree_font_size);
                    cairo_text_extents(cr, "\xC2\xB0", &candidate_degree_ext);
                    cairo_set_font_size(cr, candidate_font_size);
                }

                candidate_total_width = candidate_number_width +
                                        (is_temp
                                             ? (degree_spacing +
                                                fmax(candidate_degree_ext.x_advance,
                                                     candidate_degree_ext.width))
                                             : 0.0);
                candidate_content_height =
                    fmax(candidate_number_ext.height,
                         candidate_font_ext.ascent +
                             candidate_font_ext.descent);

                if (candidate_total_width > available_width ||
                    candidate_content_height > available_height)
                    break;

                font_size = candidate_font_size;
                degree_font_size = candidate_degree_font_size;
                number_ext = candidate_number_ext;
                degree_ext = candidate_degree_ext;
                font_ext = candidate_font_ext;
                number_width = candidate_number_width;
                total_width = candidate_total_width;
                content_height = candidate_content_height;
            }

            growth_step *= 0.5;
        }
    }

    const double requested_right =
        box_x + box_width - right_margin + offset_x;
    const double block_left =
        fmax(box_x + inner_padding_x + offset_x, requested_right - total_width);
    const double number_x = block_left - number_ext.x_bearing;

    double baseline_y;
    if (align_bottom)
    {
        const double ink_bottom =
            box_y + box_height - inner_padding_y + offset_y;
        baseline_y = ink_bottom - (number_ext.y_bearing + number_ext.height);
    }
    else
    {
        const double ink_top = box_y + inner_padding_y + offset_y;
        baseline_y = ink_top - number_ext.y_bearing;
    }

    if (draw_output)
    {
        cairo_set_font_size(cr, font_size);
        cairo_move_to(cr, number_x, baseline_y);
        cairo_show_text(cr, value_text);

        if (is_temp)
        {
            const double number_top = baseline_y + number_ext.y_bearing;
            const double degree_top = number_top + (number_ext.height * 0.08);
            const double degree_x =
                block_left + number_width + degree_spacing - degree_ext.x_bearing;
            const double degree_y = degree_top - degree_ext.y_bearing;

            cairo_set_font_size(cr, degree_font_size);
            cairo_move_to(cr, degree_x, degree_y);
            cairo_show_text(cr, "\xC2\xB0");
            cairo_set_font_size(cr, font_size);
        }
    }

    layout->active = 1;
    layout->block_left = block_left;
    layout->block_right = block_left + total_width;
    layout->block_top = baseline_y + number_ext.y_bearing;
    layout->block_bottom = layout->block_top + number_ext.height;
    layout->baseline_y = baseline_y;
    layout->font_size = font_size;
}

/**
 * @brief Calculate usable horizontal bounds for a text lane.
 */
void calculate_text_lane_bounds(const struct Config *config,
                                const ScalingParams *params,
                                double box_y, double box_height,
                                int align_bottom,
                                double fallback_x, double fallback_width,
                                double *safe_x, double *safe_width)
{
    if (!safe_x || !safe_width)
        return;

    *safe_x = fallback_x;
    *safe_width = fallback_width;

    if (!config || !params || !params->is_circular || box_height <= 0.0)
        return;

    const double min_dimension =
        (config->display_width < config->display_height)
            ? (double)config->display_width
            : (double)config->display_height;
    const double content_scale =
        (config->display_content_scale_factor > 0.0f &&
         config->display_content_scale_factor <= 1.0f)
            ? config->display_content_scale_factor
            : 0.98;
    const double radius = (min_dimension * 0.5) * content_scale;
    const double center_x = config->display_width / 2.0;
    const double center_y = config->display_height / 2.0;
    const double anchor_ratio = align_bottom ? 0.82 : 0.18;
    const double sample_y = box_y + (box_height * anchor_ratio);
    const double limiting_distance = fabs(sample_y - center_y);

    if (limiting_distance >= radius)
        return;

    const double half_width = sqrt((radius * radius) -
                                   (limiting_distance * limiting_distance));
    const double candidate_x = center_x - half_width;
    const double candidate_width = half_width * 2.0;

    if (candidate_width > 0.0)
    {
        *safe_x = candidate_x;
        *safe_width = candidate_width;
    }
}

/**
 * @brief Paint a configured overlay over a loaded background image.
 */
static void paint_background_overlay(cairo_t *cr, const struct Config *config)
{
    if (config->display_background_overlay_opacity <= 0.0f)
        return;

    cairo_save(cr);
    cairo_set_source_rgba(cr,
                          cairo_color_convert(config->display_background_color.r),
                          cairo_color_convert(config->display_background_color.g),
                          cairo_color_convert(config->display_background_color.b),
                          config->display_background_overlay_opacity);
    cairo_paint(cr);
    cairo_restore(cr);
}

/**
 * @brief Paint configured background image or fallback color.
 */
void paint_display_background(cairo_t *cr, const struct Config *config)
{
    static char last_failed_path[CONFIG_MAX_PATH_LEN] = {0};

    if (!cr || !config)
        return;

    set_cairo_color(cr, &config->display_background_color);
    cairo_paint(cr);

    if (config->paths_image_background[0] != '\0')
    {
        cairo_surface_t *background =
            cairo_image_surface_create_from_png(config->paths_image_background);

        if (background &&
            cairo_surface_status(background) == CAIRO_STATUS_SUCCESS)
        {
            const int image_width = cairo_image_surface_get_width(background);
            const int image_height = cairo_image_surface_get_height(background);

            if (image_width > 0 && image_height > 0)
            {
                double scale_x = 1.0;
                double scale_y = 1.0;
                double offset_x = 0.0;
                double offset_y = 0.0;

                if (strcmp(config->display_background_image_fit, "contain") == 0)
                {
                    const double scale = fmin((double)config->display_width / image_width,
                                              (double)config->display_height / image_height);
                    scale_x = scale;
                    scale_y = scale;
                    offset_x = ((double)config->display_width - image_width * scale) / 2.0;
                    offset_y = ((double)config->display_height - image_height * scale) / 2.0;
                }
                else if (strcmp(config->display_background_image_fit, "stretch") == 0)
                {
                    scale_x = (double)config->display_width / image_width;
                    scale_y = (double)config->display_height / image_height;
                }
                else
                {
                    const double scale = fmax((double)config->display_width / image_width,
                                              (double)config->display_height / image_height);
                    scale_x = scale;
                    scale_y = scale;
                    offset_x = ((double)config->display_width - image_width * scale) / 2.0;
                    offset_y = ((double)config->display_height - image_height * scale) / 2.0;
                }

                cairo_save(cr);
                cairo_translate(cr, offset_x, offset_y);
                cairo_scale(cr, scale_x, scale_y);
                cairo_set_source_surface(cr, background, 0.0, 0.0);
                cairo_pattern_set_filter(cairo_get_source(cr),
                                         CAIRO_FILTER_BILINEAR);
                cairo_paint(cr);
                cairo_restore(cr);

                paint_background_overlay(cr, config);
                cairo_surface_destroy(background);
                last_failed_path[0] = '\0';
                return;
            }

            cairo_surface_destroy(background);
        }

        if (strcmp(last_failed_path, config->paths_image_background) != 0)
        {
            SAFE_STRCPY(last_failed_path, config->paths_image_background);
            log_message(LOG_WARNING,
                        "Failed to load background image, using background color: %s",
                        config->paths_image_background);
        }
    }

    return;
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
    const double min_dimension =
        (config->display_width < config->display_height)
            ? (double)config->display_width
            : (double)config->display_height;
    const double base_min_dimension =
        (base_width < base_height) ? base_width : base_height;

    params->scale_x = config->display_width / base_width;
    params->scale_y = config->display_height / base_height;
    params->scale_uniform =
        (base_min_dimension > 0.0) ? (min_dimension / base_min_dimension) : 1.0;

    params->is_circular = is_circular_display_device(
        device_name, config->display_width, config->display_height);
    params->inscribe_factor = params->is_circular ? M_SQRT1_2 : 1.0;

    // Calculate safe area width from detected device geometry only.
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

    params->corner_radius = 8.0 * params->scale_uniform;

    // Calculate vertical margins (0 = auto-detect based on display shape)
    if (config->display_margin_top != 0)
        params->margin_top = config->display_margin_top * params->scale_uniform;
    else
        params->margin_top = params->is_circular
                                 ? (config->display_height * 0.03)
                                 : 0.0;

    if (config->display_margin_bottom != 0)
        params->margin_bottom = config->display_margin_bottom * params->scale_uniform;
    else
        params->margin_bottom = params->is_circular
                                    ? (config->display_height * 0.03)
                                    : 0.0;

    // Log detailed scaling calculations
    log_message(
        LOG_INFO,
        "Scaling: display=%ux%u scale=(%.3f, %.3f) uniform=%.3f shape=%s inscribe=%.4f content=%.3f safe_area=%.0fpx bar_width=%dpx (%.0f%%) margin=%.1fpx margin_v=(%.1f, %.1f)",
        config->display_width, config->display_height, params->scale_x,
        params->scale_y, params->scale_uniform,
        params->is_circular ? "circular" : "rectangular",
        params->inscribe_factor, content_scale, safe_area_width,
        params->safe_bar_width, bar_width_factor * 100.0,
        params->safe_content_margin,
        params->margin_top, params->margin_bottom);
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

    /* Use device name from API if available (first two words only) */
    if (data)
    {
        const sensor_entry_t *entry = find_sensor_for_slot(data, slot_value);
        if (entry && entry->device_name[0] != '\0')
        {
            static char short_name[SENSOR_DEVICE_NAME_LEN];
            cc_safe_strcpy(short_name, sizeof(short_name),
                           entry->device_name);
            /* Find end of second word */
            int words = 0;
            for (size_t i = 0; short_name[i] != '\0'; i++)
            {
                if (short_name[i] == ' ')
                {
                    words++;
                    if (words >= 2)
                    {
                        short_name[i] = '\0';
                        break;
                    }
                }
            }
            return short_name;
        }
    }

    /* Fallback: legacy labels */
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

    if (strcmp(slot_name, "1") == 0)
    {
        return (config->layout_bar_height_1 > 0)
                   ? config->layout_bar_height_1
                   : config->layout_bar_height;
    }
    else if (strcmp(slot_name, "2") == 0)
    {
        return (config->layout_bar_height_2 > 0)
                   ? config->layout_bar_height_2
                   : config->layout_bar_height;
    }
    else if (strcmp(slot_name, "3") == 0)
    {
        return (config->layout_bar_height_3 > 0)
                   ? config->layout_bar_height_3
                   : config->layout_bar_height;
    }

    return config->layout_bar_height;
}

/**
 * @brief Get the scaled pixel bar height for a slot.
 */
int get_scaled_slot_bar_height(const struct Config *config,
                               const ScalingParams *params,
                               const char *slot_name)
{
    const uint16_t logical_height = get_slot_bar_height(config, slot_name);
    const int scaled_height =
        (int)lround(scale_value_avg(params, (double)logical_height));

    return (scaled_height > 0) ? scaled_height : 1;
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
