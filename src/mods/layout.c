/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <string.h>

#include "../srv/cc_conf.h"
#include "display.h"

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

    /* Auto starts deliberately large; each mode fits it to its own region. */
    return scale_value_avg(params, 400.0);
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
        fmax(0.01, config->layout_label_margin_bar / 100.0);
    const double min_dimension =
        (config->display_width < config->display_height)
            ? (double)config->display_width
            : (double)config->display_height;
    const double scaled_spacing = min_dimension * spacing_factor;
    const double minimum_spacing = scale_value_avg(params, 2.0);

    return fmax(minimum_spacing, scaled_spacing);
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

    const double band_ratio = 0.36;
    const double band_height = box_height * band_ratio;
    const double band_y = align_bottom ? box_y + box_height - band_height : box_y;
    calculate_safe_region_bounds(params, band_y, band_height, 1.0,
                                 fallback_x, fallback_width,
                                 safe_x, safe_width);
}

void calculate_safe_region_bounds(const ScalingParams *params,
                                  double region_y, double region_height,
                                  double width_factor,
                                  double fallback_x, double fallback_width,
                                  double *safe_x, double *safe_width)
{
    if (!safe_x || !safe_width)
        return;

    *safe_x = fallback_x;
    *safe_width = fallback_width;

    if (!params || !params->is_circular || params->circle_radius <= 0.0)
        return;

    calculate_circle_chord_bounds(
        params->circle_center_x, params->circle_center_y,
        params->circle_radius, region_y, region_height, width_factor,
        safe_x, safe_width);
}

void calculate_bar_bounds(const struct Config *config,
                          const ScalingParams *params,
                          double bar_y, double bar_height,
                          int *bar_x, int *bar_width)
{
    if (!config || !params || !bar_x || !bar_width)
        return;

    double safe_x = params->safe_content_margin;
    double safe_width = params->safe_bar_width;
    const double bar_width_factor = (config->layout_bar_width > 0)
                                        ? config->layout_bar_width / 100.0
                                        : 0.98;

    calculate_safe_region_bounds(params, bar_y, bar_height, bar_width_factor,
                                 safe_x, safe_width, &safe_x, &safe_width);
    *bar_x = (int)lround(safe_x);
    *bar_width = (int)lround(safe_width);
}

/**
 * @brief Calculate dynamic scaling parameters based on display dimensions.
 * @details Known devices use profiles; unknown devices retain the legacy
 * resolution fallback.
 */
void calculate_scaling_params(const struct Config *config,
                              ScalingParams *params, const char *device_name)
{
    const double base_size = 240.0;
    const double min_dimension = fmin(config->display_width, config->display_height);
    params->scale_x = config->display_width / base_size;
    params->scale_y = config->display_height / base_size;
    params->scale_uniform = min_dimension / base_size;

    const DisplayProfile *profile = resolve_display_profile(
        0, 0, device_name, config->display_width, config->display_height);
    const int rectangular_canvas = !display_uses_sensor_slots(config) &&
                                   config->display_width != config->display_height;
    params->shape = profile ? profile->shape
                            : (!rectangular_canvas && is_circular_display_device(
                                   device_name, config->display_width,
                                   config->display_height)
                                   ? DISPLAY_SHAPE_CIRCULAR
                                   : DISPLAY_SHAPE_RECTANGULAR);
    params->is_circular = params->shape == DISPLAY_SHAPE_CIRCULAR;
    params->profile_name = profile ? profile->name : "legacy-fallback";

    const float content_scale = (config->display_content_scale_factor > 0.0f &&
                                 config->display_content_scale_factor <= 1.0f)
                                    ? config->display_content_scale_factor
                                    : 0.98f; // Fallback: 98%
    const double center_x_ratio = profile ? profile->center_x_ratio : 0.5;
    const double center_y_ratio = profile ? profile->center_y_ratio : 0.5;
    const double diameter_ratio = profile ? profile->visible_diameter_ratio : 1.0;
    params->circle_center_x = config->display_width * center_x_ratio;
    params->circle_center_y = config->display_height * center_y_ratio;
    params->circle_radius = params->is_circular
                                ? min_dimension * 0.5 * diameter_ratio * content_scale
                                : 0.0;
    const double safe_area_width = params->is_circular
                                       ? params->circle_radius * 2.0
                                       : (double)config->display_width * content_scale;
    const double bar_width_factor = (config->layout_bar_width > 0)
                                        ? (config->layout_bar_width / 100.0)
                                        : 0.98;
    params->safe_bar_width = (int)(safe_area_width * bar_width_factor);
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
        "Scaling: display=%ux%u scale=(%.3f, %.3f) uniform=%.3f "
        "profile=%s shape=%s content=%.3f radius=%.1fpx "
        "center=(%.1f,%.1f) center_bar=%dpx (%.0f%%) margin=%.1fpx "
        "margin_v=(%.1f, %.1f)",
        config->display_width, config->display_height, params->scale_x,
        params->scale_y, params->scale_uniform, params->profile_name,
        display_shape_name(params->shape), content_scale,
        params->circle_radius, params->circle_center_x,
        params->circle_center_y,
        params->safe_bar_width, bar_width_factor * 100.0,
        params->safe_content_margin,
        params->margin_top, params->margin_bottom);
}

int calculate_layout_context(const struct Config *config,
                             const ScalingParams *params,
                             LayoutContext *layout)
{
    if (!config || !params || !layout || config->display_width == 0 ||
        config->display_height == 0)
        return 0;

    memset(layout, 0, sizeof(*layout));
    if (params->is_circular && params->circle_radius > 0.0)
    {
        layout->left = fmax(0.0, params->circle_center_x - params->circle_radius);
        layout->right = fmin((double)config->display_width,
                             params->circle_center_x + params->circle_radius);
        layout->top = fmax(params->margin_top,
                           params->circle_center_y - params->circle_radius);
        layout->bottom = fmin((double)config->display_height - params->margin_bottom,
                              params->circle_center_y + params->circle_radius);
        layout->center_x = params->circle_center_x;
        layout->center_y = params->circle_center_y;
    }
    else
    {
        layout->left = params->safe_content_margin;
        layout->right = config->display_width - params->safe_content_margin;
        layout->top = params->margin_top;
        layout->bottom = config->display_height - params->margin_bottom;
        layout->center_x = config->display_width / 2.0;
        layout->center_y = config->display_height / 2.0;
    }

    layout->width = layout->right - layout->left;
    layout->height = layout->bottom - layout->top;
    return layout->width > 0.0 && layout->height > 0.0;
}
