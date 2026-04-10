/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Circle mode rendering (alternating CPU/GPU).
 * @details Single sensor display with automatic 2.5s switching.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "../srv/cc_sensor.h"
#include "display.h"
#include "circle.h"

/**
 * @brief Global state for sensor alternation (slot-based cycling).
 */
static int current_slot_index = 0; // 0=slot1, 1=slot2, 2=slot3
static time_t last_switch_time = 0;

/**
 * @brief Resets circle mode state for config reload (SIGHUP).
 * @details Resets slot cycling to the first sensor and clears the switch timer.
 */
void reset_circle_state(void)
{
    current_slot_index = 0;
    last_switch_time = 0;
}

/**
 * @brief Find pump RPM sensor for a Liquidctl device.
 * @details Searches for an RPM sensor whose name contains "pump"
 * (case-insensitive). Falls back to first RPM sensor if no pump found.
 * @param data Sensor data collection
 * @return Pointer to pump RPM sensor, or NULL if not found
 */
static const sensor_entry_t *find_liquid_pump_rpm(
    const monitor_sensor_data_t *data)
{
    if (!data)
        return NULL;

    const sensor_entry_t *first_rpm = NULL;
    for (int i = 0; i < data->sensor_count; i++)
    {
        if (data->sensors[i].category != SENSOR_CATEGORY_RPM)
            continue;
        if (strcmp(data->sensors[i].device_type, "Liquidctl") != 0)
            continue;

        /* Prefer sensor with "pump" in the name */
        if (strstr(data->sensors[i].name, "pump"))
            return &data->sensors[i];

        if (!first_rpm)
            first_rpm = &data->sensors[i];
    }

    return first_rpm;
}

/**
 * @brief Build extra info text (freq/watts/RPM) for a slot.
 * @details Maps slot type to channel sensors:
 *   CPU → freq + watts ("X.X GHz  XXW")
 *   GPU → freq + watts ("XXXX MHz  XXW")
 *   Liquid → RPM ("XXXX RPM")
 *   Dynamic → tries freq, then watts, then RPM
 * @param data Sensor data collection
 * @param slot_value Slot configuration value
 * @param buf Output buffer
 * @param buf_size Output buffer size
 * @return 1 if text was written, 0 if no data available
 */
static int get_extra_info_text(const monitor_sensor_data_t *data,
                               const char *slot_value,
                               char *buf, size_t buf_size)
{
    if (!data || !slot_value || !buf || buf_size == 0)
        return 0;

    buf[0] = '\0';

    if (strcmp(slot_value, "cpu") == 0)
    {
        const sensor_entry_t *freq = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_FREQ);
        const sensor_entry_t *watts = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_WATTS);
        if (!freq && !watts)
            return 0;
        if (freq && watts)
        {
            if (freq->value >= 1000.0f)
                snprintf(buf, buf_size, "%.1f GHz  %.0fW",
                         freq->value / 1000.0f, watts->value);
            else
                snprintf(buf, buf_size, "%.0f MHz  %.0fW",
                         freq->value, watts->value);
        }
        else if (freq)
        {
            if (freq->value >= 1000.0f)
                snprintf(buf, buf_size, "%.1f GHz", freq->value / 1000.0f);
            else
                snprintf(buf, buf_size, "%.0f MHz", freq->value);
        }
        else
            snprintf(buf, buf_size, "%.0fW", watts->value);
        return 1;
    }

    if (strcmp(slot_value, "gpu") == 0)
    {
        const sensor_entry_t *freq = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_FREQ);
        const sensor_entry_t *watts = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_WATTS);
        if (!freq && !watts)
            return 0;
        if (freq && watts)
            snprintf(buf, buf_size, "%.0f MHz  %.0fW",
                     freq->value, watts->value);
        else if (freq)
            snprintf(buf, buf_size, "%.0f MHz", freq->value);
        else
            snprintf(buf, buf_size, "%.0fW", watts->value);
        return 1;
    }

    if (strcmp(slot_value, "liquid") == 0)
    {
        const sensor_entry_t *rpm = find_liquid_pump_rpm(data);
        if (!rpm)
            return 0;
        snprintf(buf, buf_size, "%.0f RPM", rpm->value);
        return 1;
    }

    /* Dynamic slot: try freq → watts → rpm */
    const sensor_entry_t *s = find_channel_sensor_for_slot(
        data, slot_value, SENSOR_CATEGORY_FREQ);
    if (s)
    {
        if (s->value >= 1000.0f)
            snprintf(buf, buf_size, "%.1f GHz", s->value / 1000.0f);
        else
            snprintf(buf, buf_size, "%.0f MHz", s->value);
        return 1;
    }

    s = find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_WATTS);
    if (s)
    {
        snprintf(buf, buf_size, "%.0fW", s->value);
        return 1;
    }

    s = find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_RPM);
    if (s)
    {
        snprintf(buf, buf_size, "%.0f RPM", s->value);
        return 1;
    }

    return 0;
}

/**
 * @brief Build second line of extra info (fan RPM for CPU/GPU).
 * @details CPU fan RPM comes from the Liquidctl device (AIO cooler),
 * since the CPU device itself has no fan sensor. GPU fan RPM comes
 * from the GPU device directly.
 * @param data Sensor data collection
 * @param slot_value Slot configuration value
 * @param buf Output buffer
 * @param buf_size Output buffer size
 * @return 1 if text was written, 0 if no data available
 */
static int get_extra_info_line2(const monitor_sensor_data_t *data,
                                const char *slot_value,
                                char *buf, size_t buf_size)
{
    if (!data || !slot_value || !buf || buf_size == 0)
        return 0;

    buf[0] = '\0';

    /* CPU: fan RPM comes from Liquidctl (AIO cooler fan, not pump) */
    if (strcmp(slot_value, "cpu") == 0)
    {
        const sensor_entry_t *rpm = find_channel_sensor_for_slot(
            data, "liquid", SENSOR_CATEGORY_RPM);
        if (!rpm)
            return 0;
        snprintf(buf, buf_size, "%.0f RPM", rpm->value);
        return 1;
    }

    /* GPU: fan RPM from the GPU device itself */
    if (strcmp(slot_value, "gpu") == 0)
    {
        const sensor_entry_t *rpm = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_RPM);
        if (!rpm)
            return 0;
        snprintf(buf, buf_size, "%.0f RPM", rpm->value);
        return 1;
    }

    return 0;
}

/**
 * @brief Check if text at position starts with a known unit suffix.
 * @details Recognises MHz, GHz, RPM (3-char) and W (1-char, only when
 * followed by end-of-string, space, or another separator).
 * @param text Pointer into the string to test
 * @return Length of matched unit token, or 0 if no match
 */
static int match_unit_at(const char *text)
{
    if (strncmp(text, "MHz", 3) == 0)
        return 3;
    if (strncmp(text, "GHz", 3) == 0)
        return 3;
    if (strncmp(text, "RPM", 3) == 0)
        return 3;
    if (text[0] == 'W' && (text[1] == '\0' || text[1] == ' '))
        return 1;
    return 0;
}

/**
 * @brief Render text with unit suffixes (MHz, GHz, W, RPM) at 2/3 font size.
 * @details Walks the string, renders numeric/space segments at @p full_size
 * and recognised unit tokens at 2/3 of @p full_size.  Uses Cairo's
 * current-point advancement so no manual x-tracking is needed.
 * @param cr      Cairo context (font face must already be selected)
 * @param full_size  Font size for numeric parts
 * @param x       Left start position
 * @param y       Baseline position
 * @param text    The combined info string (e.g. "1500 MHz  95W")
 */
static void render_text_with_small_units(cairo_t *cr, double full_size,
                                         double x, double y,
                                         const char *text)
{
    if (!cr || !text || text[0] == '\0')
        return;

    const double unit_size = full_size * (2.0 / 3.0);
    const char *p = text;
    char segment[64];

    cairo_move_to(cr, x, y);
    cairo_set_font_size(cr, full_size);

    while (*p)
    {
        int unit_len = match_unit_at(p);
        if (unit_len > 0)
        {
            if ((size_t)unit_len >= sizeof(segment))
                unit_len = (int)(sizeof(segment) - 1);
            memcpy(segment, p, (size_t)unit_len);
            segment[unit_len] = '\0';

            cairo_set_font_size(cr, unit_size);
            cairo_show_text(cr, segment);
            cairo_set_font_size(cr, full_size);

            p += unit_len;
        }
        else
        {
            int len = 0;
            while (p[len] && match_unit_at(p + len) == 0)
                len++;
            if ((size_t)len >= sizeof(segment))
                len = (int)(sizeof(segment) - 1);
            memcpy(segment, p, (size_t)len);
            segment[len] = '\0';

            cairo_set_font_size(cr, full_size);
            cairo_show_text(cr, segment);

            p += len;
        }
    }
}

/**
 * @brief Get the slot value for a given slot index.
 * @param config Configuration
 * @param slot_index 0=slot1, 1=slot2, 2=slot3
 * @return Slot value string ("cpu", "gpu", "liquid", "none")
 */
static const char *get_slot_value_by_index(const struct Config *config, int slot_index)
{
    if (!config)
        return "none";

    switch (slot_index)
    {
    case 0:
        return config->sensor_slot_1;
    case 1:
        return config->sensor_slot_2;
    case 2:
        return config->sensor_slot_3;
    default:
        return "none";
    }
}

/**
 * @brief Get slot name for a given slot index.
 */
static const char *get_slot_name_by_index(int slot_index)
{
    switch (slot_index)
    {
    case 0:
        return "1";
    case 1:
        return "2";
    case 2:
        return "3";
    default:
        return "1";
    }
}

/**
 * @brief Find next active slot index (wrapping around).
 * @param config Configuration
 * @param start_index Starting slot index
 * @return Next active slot index, or -1 if none found
 */
static int find_next_active_slot(const struct Config *config, int start_index)
{
    for (int i = 0; i < 3; i++)
    {
        int idx = (start_index + i) % 3;
        const char *slot_value = get_slot_value_by_index(config, idx);
        if (slot_is_active(slot_value))
            return idx;
    }
    return -1; // No active slots
}

/**
 * @brief Check if sensor should switch based on configured interval.
 */
static void update_sensor_mode(const struct Config *config)
{
    time_t current_time = time(NULL);

    if (last_switch_time == 0)
    {
        // Initialize to first active slot
        current_slot_index = find_next_active_slot(config, 0);
        if (current_slot_index < 0)
            current_slot_index = 0; // Fallback
        last_switch_time = current_time;
        return;
    }

    // Check if configured interval has elapsed
    const double interval = (config && config->circle_switch_interval > 0)
                                ? (double)config->circle_switch_interval
                                : 5.0; // Fallback: 5 seconds

    if (difftime(current_time, last_switch_time) >= interval)
    {
        // Find next active slot
        int next_slot = find_next_active_slot(config, (current_slot_index + 1) % 3);
        if (next_slot >= 0)
            current_slot_index = next_slot;
        last_switch_time = current_time;

        // Verbose logging only
        if (verbose_logging)
        {
            const char *slot_value = get_slot_value_by_index(config, current_slot_index);
            const char *label = get_slot_label(config, NULL, slot_value);
            log_message(LOG_INFO,
                        "Circle mode: switched to %s display (slot: %s, interval: %.0fs)",
                        label ? label : "unknown",
                        get_slot_name_by_index(current_slot_index),
                        interval);
        }
    }
}

/**
 * @brief Draw single sensor display based on current slot.
 * @param cr Cairo context
 * @param config Configuration
 * @param params Scaling parameters
 * @param data Sensor data
 * @param slot_value Current slot sensor value ("cpu", "gpu", "liquid")
 */
static void draw_single_sensor(cairo_t *cr, const struct Config *config,
                               const ScalingParams *params,
                               const monitor_sensor_data_t *data,
                               const char *slot_value)
{
    if (!cr || !config || !params || !data || !slot_value)
        return;

    // Skip if slot is not active
    if (!slot_is_active(slot_value))
        return;

    // Get temperature and label for current slot
    const float temp_value = get_slot_temperature(data, slot_value);
    const char *label_text = get_slot_label(config, data, slot_value);
    const float max_temp = get_slot_max_scale(config, slot_value);

    const int effective_bar_width = params->safe_bar_width;
    const int bar_height = get_scaled_slot_bar_height(
        config, params, get_slot_name_by_index(current_slot_index));
    const int bar_x = (int)lround(params->safe_content_margin);

    const double region_gap = get_effective_label_spacing(config, params);
    const double label_padding = fmax(2.0, scale_value_avg(params, 4.0));

    double label_band_height = 0.0;
    double label_font_size = get_preferred_label_font_size(config, params);
    cairo_font_extents_t label_font_ext = {0};
    cairo_text_extents_t label_text_ext = {0};

    if (label_text)
    {
        const double left_margin_factor =
            (config->layout_label_margin_left > 0)
                ? (config->layout_label_margin_left / 100.0)
                : 0.01;
        const double label_left_padding = effective_bar_width * left_margin_factor;
        const double available_label_width =
            fmax(24.0, effective_bar_width - label_left_padding);
        const double min_label_font_size =
            (config->font_size_labels > 0.0f)
                ? fmax(12.0, scale_value_avg(params,
                                             (double)config->font_size_labels) *
                                 0.70)
                : fmax(12.0, scale_value_avg(params, 12.0));

        cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
        while (1)
        {
            cairo_set_font_size(cr, label_font_size);
            cairo_font_extents(cr, &label_font_ext);
            cairo_text_extents(cr, label_text, &label_text_ext);
            if (fmax(label_text_ext.x_advance, label_text_ext.width) <=
                    available_label_width ||
                label_font_size <= min_label_font_size)
                break;

            label_font_size *= 0.94;
            if (label_font_size < min_label_font_size)
                label_font_size = min_label_font_size;
        }

        label_band_height = label_font_ext.ascent + label_font_ext.descent +
                            (2.0 * label_padding);
    }

    const double grouped_height =
        bar_height + (label_text ? (region_gap + label_band_height) : 0.0);
    const double available_height =
        config->display_height - params->margin_top - params->margin_bottom;
    const int bar_y =
        (int)lround(fmax(0.0, params->margin_top +
                                  (available_height - grouped_height) / 2.0));
    const double value_bar_gap = region_gap * 0.05;

    const double value_box_y = params->margin_top;
    const SensorConfig *sc_gap = get_sensor_config(config, slot_value);
    const double gap_above = (sc_gap && sc_gap->value_to_bar_gap > 0.0f)
                                 ? available_height * (sc_gap->value_to_bar_gap / 100.0)
                                 : value_bar_gap;
    const double gap_below = (sc_gap && sc_gap->label_to_bar_gap > 0.0f)
                                 ? available_height * (sc_gap->label_to_bar_gap / 100.0)
                                 : region_gap;
    const double value_box_height = fmax(0.0, bar_y - gap_above - params->margin_top);
    const double label_box_y = bar_y + bar_height + gap_below;
    const double label_box_height =
        fmax(0.0, config->display_height - params->margin_bottom - label_box_y);

    if (verbose_logging)
    {
        log_message(
            LOG_INFO,
            "Circle layout: slot=%s logical(height=%u gap=%u) scaled(height=%d gap=%.1f) bar_y=%d grouped_height=%.1f value_gap=%.1f value_box=%.1fx%.1f label_box_y=%.1f label_box_h=%.1f safe_width=%d",
            get_slot_name_by_index(current_slot_index),
            get_slot_bar_height(config, get_slot_name_by_index(current_slot_index)),
            config->layout_bar_gap, bar_height, region_gap, bar_y,
            grouped_height, value_bar_gap, value_box_y, value_box_height, label_box_y,
            label_box_height, effective_bar_width);
    }

    const Color *value_color = &config->font_color_temp;

    // Draw temperature bar (centered reference point)
    const double bar_alpha = config->layout_bar_opacity;

    // Bar background
    set_cairo_color_alpha(cr, &config->layout_bar_color_background, bar_alpha);
    draw_rounded_rectangle_path(cr, bar_x, bar_y, effective_bar_width, bar_height,
                                params->corner_radius);
    cairo_fill(cr);

    // Bar border (only if enabled and thickness > 0)
    if (config->layout_bar_border_enabled && config->layout_bar_border > 0.0f)
    {
        set_cairo_color_alpha(cr, &config->layout_bar_color_border, bar_alpha);
        draw_rounded_rectangle_path(cr, bar_x, bar_y, effective_bar_width, bar_height,
                                    params->corner_radius);
        cairo_set_line_width(cr, get_scaled_bar_border_width(config, params));
        cairo_stroke(cr);
    }

    // Bar fill (temperature-based)
    const int fill_width = calculate_temp_fill_width(temp_value, effective_bar_width, max_temp);

    if (fill_width > 0)
    {
        Color bar_color = get_slot_bar_color(config, slot_value, temp_value);
        set_cairo_color_alpha(cr, &bar_color, bar_alpha);

        cairo_save(cr);
        draw_rounded_rectangle_path(cr, bar_x, bar_y, effective_bar_width,
                                    bar_height, params->corner_radius);
        cairo_clip(cr);
        cairo_rectangle(cr, bar_x, bar_y, fill_width, bar_height);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    set_cairo_color(cr, value_color);
    SlotValueLayout value_layout = {0};
    if (value_box_height > 0.0)
    {
        double safe_x = bar_x;
        double safe_width = effective_bar_width;
        calculate_text_lane_bounds(config, params, value_box_y,
                                   value_box_height, 0, bar_x,
                                   effective_bar_width, &safe_x,
                                   &safe_width);
        layout_and_render_slot_value(cr, data, config, params, slot_value,
                                     temp_value, safe_x, value_box_y,
                                     safe_width, value_box_height, 0,
                                     1, &value_layout);

        /* Duty % rendered left-aligned at half temp font size */
        if (value_layout.active &&
            (strcmp(slot_value, "cpu") == 0 ||
             strcmp(slot_value, "gpu") == 0))
        {
            const sensor_entry_t *duty_s = find_channel_sensor_for_slot(
                data, slot_value, SENSOR_CATEGORY_DUTY);
            if (duty_s)
            {
                char duty_buf[16];
                snprintf(duty_buf, sizeof(duty_buf), "%.0f",
                         duty_s->value);

                double duty_font = value_layout.font_size * 0.5;
                double pct_font = duty_font / 2.05;
                cairo_font_extents_t duty_fext = {0};
                cairo_text_extents_t duty_text_ext = {0};
                cairo_text_extents_t pct_ext = {0};

                cairo_select_font_face(cr, config->font_face,
                                       CAIRO_FONT_SLANT_NORMAL,
                                       CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, duty_font);
                cairo_font_extents(cr, &duty_fext);
                cairo_text_extents(cr, duty_buf, &duty_text_ext);

                cairo_set_font_size(cr, pct_font);
                cairo_text_extents(cr, "%", &pct_ext);
                cairo_set_font_size(cr, duty_font);

                const double pct_spacing = fmax(1.0, scale_value_avg(params, 1.0));
                double duty_number_width =
                    fmax(duty_text_ext.x_advance, duty_text_ext.width);
                double duty_total_width = duty_number_width + pct_spacing +
                                          fmax(pct_ext.x_advance, pct_ext.width);

                /* Position: left margin, vertically centered with temp */
                const double left_margin_factor =
                    (config->layout_label_margin_left > 0)
                        ? (config->layout_label_margin_left / 100.0)
                        : 0.01;
                double duty_x = safe_x +
                                (safe_width * left_margin_factor);
                double duty_y = value_layout.baseline_y;

                /* Don't overlap with temperature block */
                double duty_right = duty_x + duty_total_width +
                                    scale_value_avg(params, 4.0);
                if (duty_right <= value_layout.block_left)
                {
                    set_cairo_color(cr, value_color);
                    cairo_move_to(cr, duty_x, duty_y);
                    cairo_show_text(cr, duty_buf);

                    /* Render % as superscript at ~49% of duty font */
                    const double num_top = duty_y + duty_text_ext.y_bearing;
                    const double pct_top = num_top +
                                           (duty_text_ext.height * 0.08);
                    const double pct_x = duty_x + duty_number_width +
                                         pct_spacing - pct_ext.x_bearing;
                    const double pct_y = pct_top - pct_ext.y_bearing;

                    cairo_set_font_size(cr, pct_font);
                    cairo_move_to(cr, pct_x, pct_y);
                    cairo_show_text(cr, "%");
                    cairo_set_font_size(cr, duty_font);
                }
            }
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
        const double label_inner_padding_y =
            fmax(2.0, scale_value_avg(params, 4.0));
        const double available_label_width =
            fmax(24.0, label_safe_width - label_left_padding);
        const double available_label_height =
            fmax(12.0, label_box_height - (2.0 * label_inner_padding_y));
        const double min_label_font_size =
            (config->font_size_labels > 0.0f)
                ? fmax(12.0, scale_value_avg(params,
                                             (double)config->font_size_labels) *
                                 0.70)
                : fmax(12.0, scale_value_avg(params, 12.0));

        cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);

        while (1)
        {
            cairo_set_font_size(cr, label_font_size);
            cairo_font_extents(cr, &label_font_ext);
            cairo_text_extents(cr, label_text, &label_text_ext);

            if ((fmax(label_text_ext.x_advance, label_text_ext.width) <=
                     available_label_width &&
                 (label_font_ext.ascent + label_font_ext.descent) <=
                     available_label_height) ||
                label_font_size <= min_label_font_size)
                break;

            label_font_size *= 0.94;
            if (label_font_size < min_label_font_size)
                label_font_size = min_label_font_size;
        }

        set_cairo_color(cr, label_color);

        double label_x = label_safe_x + (label_safe_width * left_margin_factor);
        double final_label_y =
            label_box_y + label_inner_padding_y + label_font_ext.ascent;

        // Apply user-defined offsets using the uniform layout scale.
        label_x += get_scaled_label_offset_x(config, params);
        final_label_y += get_scaled_label_offset_y(config, params);

        cairo_move_to(cr, label_x, final_label_y);
        cairo_show_text(cr, label_text);
    }

    // Draw extra info (freq/watts/RPM) below the label if enabled
    if (config->circle_show_extra_info)
    {
        char extra_buf[64];
        if (get_extra_info_text(data, slot_value, extra_buf, sizeof(extra_buf)))
        {
            double extra_font_size = label_font_size * 2.2;
            const double extra_padding_top = fmax(1.0, scale_value_avg(params, 3.0));

            cairo_font_extents_t extra_font_ext = {0};
            cairo_text_extents_t extra_text_ext = {0};

            cairo_select_font_face(cr, config->font_face,
                                   CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, extra_font_size);
            cairo_font_extents(cr, &extra_font_ext);
            cairo_text_extents(cr, extra_buf, &extra_text_ext);

            // Auto-shrink if text exceeds available width
            const double extra_available_width =
                fmax(24.0, (double)params->safe_bar_width * 0.96);
            const double min_extra_font = label_font_size * 0.8;
            while (fmax(extra_text_ext.x_advance, extra_text_ext.width) >
                       extra_available_width &&
                   extra_font_size > min_extra_font)
            {
                extra_font_size *= 0.94;
                cairo_set_font_size(cr, extra_font_size);
                cairo_font_extents(cr, &extra_font_ext);
                cairo_text_extents(cr, extra_buf, &extra_text_ext);
            }

            double extra_y = label_box_y + label_font_ext.ascent +
                             label_font_ext.descent +
                             extra_padding_top + extra_font_ext.ascent;

            // Only render if it fits within the display height
            if (extra_y + extra_font_ext.descent <= config->display_height)
            {
                const Color *value_col = &config->font_color_temp;
                set_cairo_color(cr, value_col);

                const double left_margin_factor =
                    (config->layout_label_margin_left > 0)
                        ? (config->layout_label_margin_left / 100.0)
                        : 0.01;
                double extra_x = (int)lround(params->safe_content_margin) +
                                 ((double)params->safe_bar_width * left_margin_factor) +
                                 get_scaled_label_offset_x(config, params);

                render_text_with_small_units(cr, extra_font_size,
                                             extra_x, extra_y, extra_buf);

                // Second line: fan RPM for CPU/GPU
                char line2_buf[64];
                if (get_extra_info_line2(data, slot_value, line2_buf,
                                         sizeof(line2_buf)))
                {
                    double line2_font_size = extra_font_size;
                    cairo_font_extents_t line2_font_ext = {0};
                    cairo_text_extents_t line2_text_ext = {0};

                    cairo_set_font_size(cr, line2_font_size);
                    cairo_font_extents(cr, &line2_font_ext);
                    cairo_text_extents(cr, line2_buf, &line2_text_ext);

                    // Auto-shrink for line 2
                    while (fmax(line2_text_ext.x_advance,
                                line2_text_ext.width) >
                               extra_available_width &&
                           line2_font_size > min_extra_font)
                    {
                        line2_font_size *= 0.94;
                        cairo_set_font_size(cr, line2_font_size);
                        cairo_font_extents(cr, &line2_font_ext);
                        cairo_text_extents(cr, line2_buf, &line2_text_ext);
                    }

                    double line2_y = extra_y + extra_font_ext.descent +
                                     extra_padding_top + line2_font_ext.ascent;

                    if (line2_y + line2_font_ext.descent <=
                        config->display_height)
                    {
                        set_cairo_color(cr, value_col);
                        render_text_with_small_units(cr, line2_font_size,
                                                     extra_x, line2_y,
                                                     line2_buf);
                    }
                }
            }
        }
    }
}

/**
 * @brief Render complete circle mode display.
 */
static void render_display_content(cairo_t *cr, const struct Config *config,
                                   const monitor_sensor_data_t *data,
                                   const ScalingParams *params)
{
    if (!cr || !config || !data || !params)
        return;

    paint_display_background(cr, config);

    // Update sensor mode (check if configured interval elapsed)
    update_sensor_mode(config);

    // Get current slot value and draw sensor
    const char *slot_value = get_slot_value_by_index(config, current_slot_index);
    draw_single_sensor(cr, config, params, data, slot_value);
}

/**
 * @brief Render circle mode display to PNG file.
 * @details Creates PNG image with single sensor, does NOT upload.
 */
static int render_circle_display(const struct Config *config,
                                 const monitor_sensor_data_t *data,
                                 const char *device_name)
{
    if (!config || !data)
    {
        log_message(LOG_ERROR, "Invalid parameters for circle mode rendering");
        return 0;
    }

    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);

    // Verbose logging only
    if (verbose_logging)
    {
        const char *slot_value = get_slot_value_by_index(config, current_slot_index);
        const char *label = get_slot_label(config, data, slot_value);
        float temp = get_slot_temperature(data, slot_value);
        log_message(LOG_INFO, "Circle mode: rendering %s (%.1f)",
                    label ? label : "unknown", temp);
    }

    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;

    render_display_content(cr, config, data, &params);

    cairo_surface_flush(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Cairo drawing error: %s",
                    cairo_status_to_string(cairo_status(cr)));
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return 0;
    }

    // Write PNG to file
    cairo_status_t write_status =
        cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    if (write_status != CAIRO_STATUS_SUCCESS)
    {
        log_message(LOG_ERROR, "Failed to write PNG: %s",
                    cairo_status_to_string(write_status));
        return 0;
    }

    return 1;
}

/**
 * @brief High-level entry point for circle mode rendering.
 * @details Collects sensor data, renders circle display using
 * render_circle_display(), and sends to LCD device.
 */
void draw_circle_image(const struct Config *config)
{
    if (!config)
    {
        log_message(LOG_ERROR, "Invalid config for circle mode");
        return;
    }

    // Get device information
    char device_uid[128] = {0};
    char device_name[128] = {0};
    int screen_width = 0, screen_height = 0;

    const int device_available =
        get_cached_lcd_device_data(config, device_uid, sizeof(device_uid),
                                   device_name, sizeof(device_name),
                                   &screen_width, &screen_height);

    // Get sensor data
    monitor_sensor_data_t data = {0};
    if (!get_sensor_monitor_data(config, &data))
    {
        log_message(LOG_WARNING, "Circle mode: Failed to get sensor data");
        return;
    }

    // Render circle display with device name for circular display detection
    if (!render_circle_display(config, &data, device_name))
    {
        log_message(LOG_ERROR, "Circle display rendering failed");
        return;
    }

    // Send to LCD if available
    if (is_session_initialized() && device_available && device_uid[0] != '\0')
    {
        const char *name =
            (device_name[0] != '\0') ? device_name : "Unknown Device";
        log_message(LOG_INFO, "Sending circle image to LCD: %s [%s]", name,
                    device_uid);

        send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);

        log_message(LOG_INFO, "Circle LCD image uploaded successfully");
    }
    else
    {
        log_message(LOG_WARNING, "Skipping circle LCD upload - device not available");
    }
}
