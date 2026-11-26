/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "circle_render.h"
#include "text.h"
#include "bar.h"
#include "circle_text.h"

void paint_circle_sensor(cairo_t *cr, const Config *config, const ScalingParams *params,
                         const monitor_sensor_data_t *data, const char *slot_value,
                         const char *slot_name)
{
    if (!cr || !config || !params || !data)
        return;

    LayoutContext geometry = {0};
    if (!calculate_layout_context(config, params, &geometry))
        return;

    // Skip if slot is not active
    if (!slot_is_active(slot_value))
        return;

    // Get temperature and label for current slot
    const float temp_value = get_slot_temperature(data, slot_value);
    char short_name[SENSOR_DEVICE_NAME_LEN];
    const char *label_text = get_slot_label(config, data, slot_value, short_name);
    const float max_temp = get_slot_max_scale(config, slot_value);
    const int centered = strcmp(config->circle_layout, "centered") == 0;

    char extra_buf[64] = {0};
    char line2_buf[64] = {0};
    const int has_extra_line =
        circle_extra_info_text(config, data, slot_value, extra_buf, sizeof(extra_buf));
    const int has_rpm_line =
        config->circle_show_rpm &&
        circle_extra_info_line2(data, slot_value, line2_buf, sizeof(line2_buf));
    const int extra_lines = has_extra_line + has_rpm_line;
    const int adaptive = !config->circle_show_bar || !config->circle_show_load ||
                         !config->circle_show_frequency || !config->circle_show_watts ||
                         !config->circle_show_rpm;

    int effective_bar_width = params->safe_bar_width;
    const int bar_height = config->circle_show_bar
        ? get_scaled_slot_bar_height(config, params, slot_name)
        : 0;
    int bar_x = (int)lround(params->safe_content_margin);

    const double region_gap = get_effective_label_spacing(config, params);
    const double circle_minimum_gap =
        fmin((double)config->display_width,
             (double)config->display_height) * 0.04;
    const double circle_bar_gap = fmax(region_gap, circle_minimum_gap);
    double label_font_size = get_preferred_label_font_size(config, params);
    cairo_text_extents_t label_text_ext = {0};
    double label_ink_bottom = 0.0;

    const double available_height = geometry.height;
    const double lower_height = adaptive
        ? geometry.height * (0.16 + 0.13 * extra_lines) + circle_bar_gap
        : geometry.height * 0.5;
    const int bar_y = adaptive
        ? (int)lround(fmax(geometry.top, geometry.bottom - lower_height - bar_height))
        : (int)lround(geometry.center_y - bar_height / 2.0);
    calculate_bar_bounds(config, params, bar_y, bar_height,
                         &bar_x, &effective_bar_width);
    const double value_bar_gap = circle_bar_gap;

    const double value_box_y = geometry.top + (adaptive
        ? fmax(circle_minimum_gap, (bar_y - geometry.top) * 0.20)
        : centered ? circle_minimum_gap : 0.0);
    const SensorConfig *sc_gap = get_sensor_config(config, slot_value);
    const double gap_above = (sc_gap && sc_gap->value_to_bar_gap > 0.0f)
                                 ? fmax(value_bar_gap,
                                        available_height *
                                            (sc_gap->value_to_bar_gap / 100.0))
                                 : value_bar_gap;
    const double gap_below = (sc_gap && sc_gap->label_to_bar_gap > 0.0f)
                                 ? fmax(circle_bar_gap,
                                        available_height *
                                            (sc_gap->label_to_bar_gap / 100.0))
                                 : circle_bar_gap;
    const double value_box_height = fmax(0.0, bar_y - gap_above - value_box_y);
    const double label_box_y = bar_y + bar_height + gap_below;
    const double label_box_height = fmax(0.0, geometry.bottom - label_box_y);
    const double label_height_budget = adaptive
        ? fmin(label_box_height, geometry.height * 0.13)
        : label_box_height * 0.30;

    const Color *value_color = &config->font_color_temp;

    if (config->circle_show_bar)
    {
        const LayoutBox box = {bar_x, bar_y, effective_bar_width, bar_height};
        const int fill_width = calculate_temp_fill_width(temp_value, effective_bar_width, max_temp);
        const Color bar_color = get_slot_bar_color(config, slot_value, temp_value);
        draw_temperature_bar(cr, config, params, &box, fill_width, &bar_color, BAR_CLIPPED_FILL);
    }

    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    set_cairo_color(cr, value_color);
    SlotValueLayout value_layout = {0};
    if (value_box_height > 0.0)
    {
        double safe_x = bar_x;
        double safe_width = effective_bar_width;
        if (centered || adaptive)
            calculate_safe_region_bounds(params, value_box_y, value_box_height,
                                         0.96, bar_x, effective_bar_width,
                                         &safe_x, &safe_width);
        else
            calculate_text_lane_bounds(config, params, value_box_y,
                                       value_box_height, 1, bar_x,
                                       effective_bar_width, &safe_x,
                                       &safe_width);
        const double left_margin_factor =
            (config->layout_label_margin_left > 0)
                ? config->layout_label_margin_left / 100.0
                : 0.01;
        const double row_x = safe_x + safe_width * left_margin_factor;
        const double row_width =
            fmax(1.0, safe_x + safe_width - row_x);
        const double element_gap = fmax(1.0, circle_minimum_gap);
        /* Measure both values before assigning their disjoint row regions. */
        SlotValueLayout natural_value_layout = {0};
        layout_and_render_slot_value(cr, data, config, params, slot_value,
                                     temp_value, row_x, value_box_y,
                                     row_width, value_box_height, 1,
                                     0, &natural_value_layout);
        DutyLayout duty_layout = {0};
        if (config->circle_show_load)
            circle_duty_layout(cr, config, params, data, slot_value,
                                  row_width, value_box_height,
                                  natural_value_layout.font_size * 0.5,
                                  &duty_layout);

        double value_x = row_x;
        double value_width = row_width;
        if (duty_layout.active && natural_value_layout.active)
        {
            const double available_width = fmax(2.0, row_width - element_gap);
            const double natural_value_width =
                natural_value_layout.block_right -
                natural_value_layout.block_left;
            const double natural_total =
                duty_layout.total_width + natural_value_width;
            double duty_region_width =
                natural_total > 0.0
                    ? available_width *
                          (duty_layout.total_width / natural_total)
                    : available_width * 0.5;
            duty_region_width =
                fmax(1.0, fmin(available_width - 1.0, duty_region_width));

            circle_duty_layout(cr, config, params, data, slot_value,
                                  duty_region_width, value_box_height,
                                  natural_value_layout.font_size * 0.5,
                                  &duty_layout);
            value_x = row_x + duty_region_width + element_gap;
            value_width = fmax(1.0, row_x + row_width - value_x);

            layout_and_render_slot_value(cr, data, config, params, slot_value,
                                         temp_value, value_x, value_box_y,
                                         value_width, value_box_height, 1,
                                         0, &natural_value_layout);
            circle_duty_layout(cr, config, params, data, slot_value,
                                  duty_region_width, value_box_height,
                                  natural_value_layout.font_size * 0.5,
                                  &duty_layout);
        }

        cairo_save(cr);
        if ((centered || !config->circle_show_load) &&
            !duty_layout.active && natural_value_layout.active)
        {
            const double block_width = natural_value_layout.block_right -
                                       natural_value_layout.block_left;
            const double offset = scale_value_x(
                params, (double)get_slot_offset_x(config, slot_value));
            const double target_left = fmax(safe_x, fmin(
                safe_x + safe_width - block_width,
                safe_x + (safe_width - block_width) * 0.5 + offset));
            cairo_translate(cr, target_left - natural_value_layout.block_left, 0.0);
        }
        layout_and_render_slot_value(cr, data, config, params, slot_value,
                                     temp_value, value_x, value_box_y,
                                     value_width, value_box_height, 1,
                                     1, &value_layout);
        cairo_restore(cr);

        if (value_layout.active && duty_layout.active)
        {
            const double duty_y = value_layout.baseline_y;
            cairo_set_font_size(cr, duty_layout.font_size);
            set_cairo_color(cr, value_color);
            cairo_move_to(cr, row_x - duty_layout.number_ext.x_bearing,
                          duty_y);
            cairo_show_text(cr, duty_layout.number);

            const double number_top =
                duty_y + duty_layout.number_ext.y_bearing;
            const double percent_top =
                number_top + duty_layout.number_ext.height * 0.08;
            const double percent_x = row_x + duty_layout.number_width +
                                     duty_layout.spacing -
                                     duty_layout.percent_ext.x_bearing;
            const double percent_y =
                percent_top - duty_layout.percent_ext.y_bearing;
            cairo_set_font_size(cr, duty_layout.percent_font_size);
            cairo_move_to(cr, percent_x, percent_y);
            cairo_show_text(cr, "%");
        }
    }

    // Draw label (CPU, GPU, or LIQ) in a dedicated bottom lane anchored to the bar.
    if (label_text)
    {
        const Color *label_color = &config->font_color_label;
        double label_safe_x = bar_x;
        double label_safe_width = effective_bar_width;
        const double left_margin_factor =
            (config->layout_label_margin_left > 0)
                ? (config->layout_label_margin_left / 100.0)
                : 0.01;

        calculate_text_lane_bounds(config, params, label_box_y,
                                   label_box_height, 0, bar_x,
                                   effective_bar_width, &label_safe_x,
                                   &label_safe_width);

        const double label_left_padding = label_safe_width * left_margin_factor;
        const double available_label_width =
            fmax(24.0, label_safe_width - label_left_padding);
        const double available_label_height =
            fmax(1.0, label_box_height);
        const double min_label_font_size =
            (config->font_size_labels > 0.0f)
                ? fmax(12.0, scale_value_avg(params,
                                             (double)config->font_size_labels) *
                                 0.70)
                : fmax(12.0, scale_value_avg(params, 12.0));

        cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);

        label_font_size = fit_text_font_size(
            cr, label_text, label_font_size, available_label_width,
            fmin(available_label_height, label_height_budget), min_label_font_size);
        cairo_set_font_size(cr, label_font_size);
        cairo_text_extents(cr, label_text, &label_text_ext);

        set_cairo_color(cr, label_color);

        double label_x = centered
                             ? label_safe_x + (label_safe_width - label_text_ext.width) * 0.5 -
                                   label_text_ext.x_bearing
                             : label_safe_x + (label_safe_width * left_margin_factor);
        double final_label_y = label_box_y - label_text_ext.y_bearing;

        // Apply user-defined offsets using the uniform layout scale.
        label_x += get_scaled_label_offset_x(config, params);
        final_label_y += get_scaled_label_offset_y(config, params);
        const double minimum_label_y = label_box_y - label_text_ext.y_bearing;
        const double maximum_label_y =
            fmax(minimum_label_y,
                 geometry.bottom - label_text_ext.y_bearing -
                     label_text_ext.height);
        final_label_y = fmin(fmax(final_label_y, minimum_label_y),
                             maximum_label_y);
        const double label_width =
            fmax(label_text_ext.x_advance, label_text_ext.width);
        const double minimum_label_x = label_safe_x - label_text_ext.x_bearing;
        const double maximum_label_x =
            fmax(minimum_label_x,
                 label_safe_x + label_safe_width - label_width -
                     label_text_ext.x_bearing);
        label_x = fmin(fmax(label_x, minimum_label_x), maximum_label_x);

        log_layout_box(cr, config, "label", label_x + label_text_ext.x_bearing,
                       final_label_y + label_text_ext.y_bearing,
                       label_text_ext.width, label_text_ext.height);
        cairo_move_to(cr, label_x, final_label_y);
        cairo_show_text(cr, label_text);
        label_ink_bottom = final_label_y + label_text_ext.y_bearing +
                           label_text_ext.height;
    }

    // Draw extra info (freq/watts/RPM) below the label if enabled
    if (config->circle_show_frequency || config->circle_show_watts ||
        config->circle_show_rpm)
    {

        if (verbose_logging)
        {
            log_message(LOG_INFO, "Circle extra info: slot=%s line1=%s line2=%s",
                        slot_value ? slot_value : "none",
                        has_extra_line ? extra_buf : "none",
                        has_rpm_line ? line2_buf : "none");
        }

        if (has_extra_line || has_rpm_line)
        {
            double extra_font_size = config->font_size_watts > 0.0f
                                         ? scale_value_avg(params,
                                                           config->font_size_watts)
                                         : label_font_size * 2.2;
            const double extra_padding_top = region_gap;
            double extra_safe_x = bar_x;
            double extra_available_width =
                fmax(24.0, (double)effective_bar_width * 0.96);
            const double min_extra_font =
                fmax(6.0, scale_value_avg(params, 6.0));

            cairo_font_extents_t font_ext = {0};
            cairo_text_extents_t extra_text_ext = {0};
            cairo_text_extents_t line2_text_ext = {0};
            double extra_y = 0.0;
            double line2_y = 0.0;
            double rendered_bottom = 0.0;
            const int line_count =
                (has_extra_line ? 1 : 0) + (has_rpm_line ? 1 : 0);

            cairo_select_font_face(cr, config->font_face,
                                   CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_BOLD);

            while (1)
            {
                cairo_set_font_size(cr, extra_font_size);
                cairo_font_extents(cr, &font_ext);
                cairo_text_extents(cr, has_extra_line ? extra_buf : "",
                                   &extra_text_ext);
                cairo_text_extents(cr, has_rpm_line ? line2_buf : "",
                                   &line2_text_ext);

                const double line_height = font_ext.ascent + font_ext.descent;
                const double block_top =
                    fmax(label_box_y, label_ink_bottom) + extra_padding_top;
                const double block_height =
                    (line_height * line_count) +
                    (extra_padding_top * (line_count - 1));

                calculate_safe_region_bounds(
                    params, block_top, block_height, 0.96,
                    bar_x, effective_bar_width,
                    &extra_safe_x, &extra_available_width);

                extra_y = block_top + font_ext.ascent;
                line2_y = has_extra_line
                              ? extra_y + line_height + extra_padding_top
                              : extra_y;
                rendered_bottom = block_top + block_height;

                const double max_text_width =
                    fmax(fmax(extra_text_ext.x_advance, extra_text_ext.width),
                         fmax(line2_text_ext.x_advance, line2_text_ext.width));

                if ((max_text_width <= extra_available_width &&
                     rendered_bottom <= geometry.bottom) ||
                    extra_font_size <= min_extra_font)
                    break;

                extra_font_size *= 0.92;
                if (extra_font_size < min_extra_font)
                    extra_font_size = min_extra_font;
            }

            // Only render if it fits within the display height after autoshrink.
            if (rendered_bottom <= geometry.bottom)
            {
                const Color *value_col = &config->font_color_temp;
                set_cairo_color(cr, value_col);

                const double left_margin_factor =
                    (config->layout_label_margin_left > 0)
                        ? (config->layout_label_margin_left / 100.0)
                        : 0.01;
                double extra_x =
                    extra_safe_x +
                    (extra_available_width * left_margin_factor) +
                    get_scaled_label_offset_x(config, params);

                const char *lines[] = {extra_buf, line2_buf};
                const int active[] = {has_extra_line, has_rpm_line};
                const double baselines[] = {extra_y, line2_y};
                for (int i = 0; i < 2; ++i)
                {
                    if (!active[i])
                        continue;
                    double line_x = extra_x;
                    if (centered)
                    {
                        const double width = circle_paint_text_with_small_units(
                            cr, extra_font_size, 0.0, 0.0, lines[i], 0);
                        line_x = extra_safe_x + (extra_available_width - width) * 0.5 +
                                 get_scaled_label_offset_x(config, params);
                        line_x = fmax(extra_safe_x, fmin(
                            extra_safe_x + extra_available_width - width, line_x));
                    }
                    circle_paint_text_with_small_units(cr, extra_font_size,
                                                 line_x, baselines[i], lines[i], 1);
                }
            }
            else if (verbose_logging)
            {
                log_message(LOG_INFO,
                            "Circle extra info skipped: slot=%s bottom=%.1f display_height=%u",
                            slot_value ? slot_value : "none",
                            rendered_bottom, config->display_height);
            }
        }
    }
}
