/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <string.h>
#include <time.h>

#include "../srv/cc_conf.h"
#include "layout.h"

int layout_columns(const Config *config, const ScalingParams *params, unsigned occupied,
                    LayoutBox columns[2], LayoutBox *divider)
{
    LayoutContext geometry;
    if (!calculate_layout_context(config, params, &geometry))
        return 0;

    const double inset = geometry.height * (params->is_circular ? 0.15 : 0.07);
    const double top = geometry.top + inset;
    const double height = (geometry.bottom - inset) - top;
    double x = params->safe_content_margin, width = params->safe_bar_width;
    calculate_safe_region_bounds(params, top, height, params->is_circular ? 0.94 : 0.96,
                                 x, width, &x, &width);
    const double padding = fmax(scale_value_avg(params, 3.0), config->display_width * 0.018);
    memset(columns, 0, 2 * sizeof(*columns));
    *divider = (LayoutBox){0};
    if (occupied == 3)
    {
        const double gap = fmax(scale_value_avg(params, 5.0), config->display_width * 0.025);
        const double left = x + padding, right = geometry.center_x + gap;
        columns[0] = (LayoutBox){left, top, geometry.center_x - gap - left, height};
        columns[1] = (LayoutBox){right, top, x + width - padding - right, height};
        const double line = fmax(1.0, scale_value_avg(params, 1.4));
        *divider = (LayoutBox){geometry.center_x - line / 2.0, top, line, height};
    }
    else if (occupied)
        columns[occupied == 1 ? 0 : 1] = (LayoutBox){x + padding, top, width - 2.0 * padding, height};

    for (int i = 0; i < 2; i++)
        if ((occupied & (1U << i)) && columns[i].width < 12.0)
            return 0;
    return 1;
}

void layout_stack(const LayoutBox *column, const CustomElement *const widgets[],
                   int count, LayoutBox boxes[])
{
    if (count <= 0)
        return;

    const double gap = count > 1 ? fmin(fmax(1.0, column->height * 0.01),
                                        column->height / (2 * (count - 1))) : 0.0;
    const double available = column->height - (count - 1) * gap;
    int fixed = 0, flexible = 0;
    for (int i = 0; i < count; i++)
        if (widgets[i]->grow)
            flexible += widgets[i]->height;
        else
            fixed += widgets[i]->height;
    const double unit = column->height / CUSTOM_LAYOUT_GRID;
    const double remaining = available - fixed * unit;
    double y = column->y;
    for (int i = 0; i < count; i++)
    {
        const CustomElement *widget = widgets[i];
        const double height = flexible && remaining > 0.0
                                  ? widget->height * (widget->grow ? remaining / flexible : unit)
                                  : available * widget->height / (fixed + flexible);
        boxes[i] = (LayoutBox){column->x, y, column->width, height};
        y += height + gap;
    }
}

int layout_arrange_screen(const Config *config, const ScalingParams *params,
                          const CustomScreen *screen, LayoutBox boxes[CUSTOM_LAYOUT_MAX_ELEMENTS])
{
    if (screen->arrangement == CUSTOM_FREE)
    {
        double width = config->display_width, height = config->display_height;
        if (strcmp(config->display_mode, "circle-rings") == 0)
            width = height = fmin(width, height);
        const double left = (config->display_width - width) / 2.0;
        const double top = (config->display_height - height) / 2.0;
        const double sx = width / CUSTOM_LAYOUT_GRID;
        const double sy = height / CUSTOM_LAYOUT_GRID;
        for (int i = 0; i < screen->count; i++)
        {
            const CustomElement *widget = &screen->elements[i];
            boxes[i] = (LayoutBox){left + widget->x * sx, top + widget->y * sy,
                                  widget->width * sx, widget->height * sy};
        }
        return 1;
    }

    unsigned occupied = 0;
    for (int i = 0; i < screen->count; i++)
        if (screen->elements[i].column >= 0)
            occupied |= 1U << screen->elements[i].column;
    LayoutBox columns[2], divider;
    if (!layout_columns(config, params, occupied, columns, &divider))
        return 0;
    for (int i = 0; i < screen->count; i++)
        if (screen->elements[i].column < 0)
            boxes[i] = divider;

    for (int col = 0; col < 2; col++)
    {
        const CustomElement *widgets[CUSTOM_LAYOUT_MAX_ELEMENTS];
        int indices[CUSTOM_LAYOUT_MAX_ELEMENTS], count = 0;
        for (int i = 0; i < screen->count; i++)
        {
            if (screen->elements[i].column == col)
            {
                widgets[count] = &screen->elements[i];
                indices[count++] = i;
            }
        }
        if (count == 0)
            continue;
        LayoutBox stack[CUSTOM_LAYOUT_MAX_ELEMENTS];
        layout_stack(&columns[col], widgets, count, stack);
        for (int i = 0; i < count; i++)
            boxes[indices[i]] = stack[i];
    }
    return 1;
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
    log_layout_box(NULL, config, "bar", *bar_x, bar_y, *bar_width, bar_height);
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
        "Scaling: mode=%s display=%ux%u scale=(%.3f, %.3f) uniform=%.3f "
        "profile=%s shape=%s content=%.3f radius=%.1fpx "
        "center=(%.1f,%.1f) center_bar=%dpx (%.0f%%) margin=%.1fpx "
        "margin_v=(%.1f, %.1f)",
        config->display_mode, config->display_width, config->display_height, params->scale_x,
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

/* The daemon's render thread owns the screen timer. Previews always use screen 0. */
static int active_screen = 0;
static int64_t screen_started_us = -1;

void reset_layout_screen_state(void)
{
    active_screen = 0;
    screen_started_us = -1;
}

int64_t layout_screen_deadline_us(const Config *config)
{
    if (screen_started_us < 0 || config->preview_render)
        return 0;

    int duration = 0;
    if (strcmp(config->display_mode, "circle-rings") == 0)
        duration = config->circle_switch_interval;
    else if (strcmp(config->display_mode, "custom") == 0 &&
             config->custom_layout.screen_count > 1 && active_screen < config->custom_layout.screen_count)
        duration = config->custom_layout.screens[active_screen].duration;
    return duration > 0 ? screen_started_us + (int64_t)duration * 1000000 : 0;
}

int layout_screen_index(const Config *config, int count)
{
    if (config->preview_render || count <= 1)
        return 0;

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return active_screen < count ? active_screen : 0;

    const int64_t now_us = (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
    if (screen_started_us < 0 || active_screen >= count)
    {
        active_screen = 0;
        screen_started_us = now_us;
    }
    else
    {
        const int64_t deadline = layout_screen_deadline_us(config);
        if (deadline > 0 && now_us >= deadline)
        {
            active_screen = (active_screen + 1) % count;
            screen_started_us = now_us;
        }
    }
    return active_screen;
}
