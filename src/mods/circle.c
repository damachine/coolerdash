/**
 * @author Christian Kühn (damachin3 at proton dot me)
 * @Maintainer: Christian Kühn (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Circle mode rendering (slot-based alternating display).
 * @details Single-sensor display that cycles through configured active slots.
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

static int ascii_tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

static int contains_ignore_case(const char *haystack, const char *needle)
{
    if (!haystack || !needle || needle[0] == '\0')
        return 0;

    for (const char *h = haystack; *h; h++)
    {
        const char *hp = h;
        const char *np = needle;
        while (*hp && *np && ascii_tolower((unsigned char)*hp) ==
                                 ascii_tolower((unsigned char)*np))
        {
            hp++;
            np++;
        }
        if (*np == '\0')
            return 1;
    }

    return 0;
}

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

        /* Prefer sensor with "pump" in the name. */
        if (contains_ignore_case(data->sensors[i].name, "pump"))
            return &data->sensors[i];

        if (!first_rpm)
            first_rpm = &data->sensors[i];
    }

    return first_rpm;
}

static void format_frequency(char *buf, size_t buf_size, float mhz)
{
    if (mhz >= 1000.0f)
        snprintf(buf, buf_size, "%.1f GHz", mhz / 1000.0f);
    else
        snprintf(buf, buf_size, "%.0f MHz", mhz);
}

static int format_freq_watts(const sensor_entry_t *freq,
                             const sensor_entry_t *watts,
                             char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0 || (!freq && !watts))
        return 0;

    if (freq && watts)
    {
        char freq_buf[24] = {0};
        format_frequency(freq_buf, sizeof(freq_buf), freq->value);
        snprintf(buf, buf_size, "%s  %.0fW", freq_buf, watts->value);
    }
    else if (freq)
    {
        format_frequency(buf, buf_size, freq->value);
    }
    else
    {
        snprintf(buf, buf_size, "%.0fW", watts->value);
    }

    return 1;
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
static int get_extra_info_text(const Config *config,
                               const monitor_sensor_data_t *data,
                               const char *slot_value,
                               char *buf, size_t buf_size)
{
    if (!config || !data || !slot_value || !buf || buf_size == 0)
        return 0;

    buf[0] = '\0';

    if (strcmp(slot_value, "cpu") == 0)
    {
        const sensor_entry_t *freq = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_FREQ);
        if (!config->circle_show_frequency) freq = NULL;
        const sensor_entry_t *watts = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_WATTS);
        if (!config->circle_show_watts) watts = NULL;
        return format_freq_watts(freq, watts, buf, buf_size);
    }

    if (strcmp(slot_value, "gpu") == 0)
    {
        const sensor_entry_t *freq = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_FREQ);
        if (!config->circle_show_frequency) freq = NULL;
        const sensor_entry_t *watts = find_channel_sensor_for_slot(
            data, slot_value, SENSOR_CATEGORY_WATTS);
        if (!config->circle_show_watts) watts = NULL;
        return format_freq_watts(freq, watts, buf, buf_size);
    }

    if (strcmp(slot_value, "liquid") == 0)
    {
        const sensor_entry_t *rpm = find_liquid_pump_rpm(data);
        if (!config->circle_show_rpm || !rpm)
            return 0;
        snprintf(buf, buf_size, "%.0f RPM", rpm->value);
        return 1;
    }

    /* Dynamic slot: try freq → watts → rpm */
    const sensor_entry_t *s = find_channel_sensor_for_slot(
        data, slot_value, SENSOR_CATEGORY_FREQ);
    if (s && config->circle_show_frequency)
    {
        format_frequency(buf, buf_size, s->value);
        return 1;
    }

    s = find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_WATTS);
    if (s && config->circle_show_watts)
    {
        snprintf(buf, buf_size, "%.0fW", s->value);
        return 1;
    }

    s = find_channel_sensor_for_slot(data, slot_value, SENSOR_CATEGORY_RPM);
    if (s && config->circle_show_rpm)
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

typedef struct
{
    int active;
    char number[16];
    double font_size;
    double percent_font_size;
    double number_width;
    double percent_width;
    double spacing;
    double total_width;
    cairo_text_extents_t number_ext;
    cairo_text_extents_t percent_ext;
} DutyLayout;

static void calculate_duty_layout(cairo_t *cr, const struct Config *config,
                                  const ScalingParams *params,
                                  const monitor_sensor_data_t *data,
                                  const char *slot_value, double max_width,
                                  double max_height,
                                  double preferred_font_size,
                                  DutyLayout *layout)
{
    if (!cr || !config || !params || !data || !slot_value || !layout)
        return;

    memset(layout, 0, sizeof(*layout));
    if (strcmp(slot_value, "cpu") != 0 && strcmp(slot_value, "gpu") != 0)
        return;

    const sensor_entry_t *duty = find_channel_sensor_for_slot(
        data, slot_value, SENSOR_CATEGORY_DUTY);
    if (!duty)
        return;

    snprintf(layout->number, sizeof(layout->number), "%.0f", duty->value);
    const double configured_temp = get_slot_font_size(config, slot_value);
    const double preferred_temp =
        configured_temp > 0.0f
            ? scale_value_avg(params, configured_temp)
            : fmax(scale_value_avg(params, 220.0), max_height * 7.4);
    layout->font_size = preferred_font_size > 0.0
                            ? preferred_font_size
                            : (config->font_size_duty > 0.0f
                                   ? scale_value_avg(params,
                                                     config->font_size_duty)
                                   : preferred_temp * 0.5);
    layout->spacing = fmax(1.0, scale_value_avg(params, 1.0));

    while (layout->font_size > 1.0)
    {
        layout->percent_font_size = layout->font_size / 2.05;
        cairo_set_font_size(cr, layout->font_size);
        cairo_text_extents(cr, layout->number, &layout->number_ext);
        cairo_set_font_size(cr, layout->percent_font_size);
        cairo_text_extents(cr, "%", &layout->percent_ext);

        layout->number_width =
            fmax(layout->number_ext.x_advance, layout->number_ext.width);
        layout->percent_width =
            fmax(layout->percent_ext.x_advance, layout->percent_ext.width);
        layout->total_width = layout->number_width + layout->spacing +
                              layout->percent_width;
        const double height =
            fmax(layout->number_ext.height,
                 layout->number_ext.height * 0.08 + layout->percent_ext.height);
        if (layout->total_width <= max_width && height <= max_height)
            break;
        layout->font_size *= 0.92;
    }

    layout->font_size = fmax(1.0, layout->font_size);
    layout->percent_font_size = layout->font_size / 2.05;
    cairo_set_font_size(cr, layout->font_size);
    cairo_text_extents(cr, layout->number, &layout->number_ext);
    cairo_set_font_size(cr, layout->percent_font_size);
    cairo_text_extents(cr, "%", &layout->percent_ext);
    layout->number_width =
        fmax(layout->number_ext.x_advance, layout->number_ext.width);
    layout->percent_width =
        fmax(layout->percent_ext.x_advance, layout->percent_ext.width);
    layout->total_width = layout->number_width + layout->spacing +
                          layout->percent_width;
    layout->active = layout->total_width > 0.0;
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
 * @param draw_output Render when nonzero, otherwise only measure.
 * @return Total advance width with the same unit sizing as the rendered text.
 */
static double render_text_with_small_units(cairo_t *cr, double full_size,
                                         double x, double y,
                                         const char *text, int draw_output)
{
    if (!cr || !text || text[0] == '\0')
        return 0.0;

    const double unit_size = full_size * (2.0 / 3.0);
    const char *p = text;
    char segment[64];
    double width = 0.0;

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
            cairo_text_extents_t ext;
            cairo_text_extents(cr, segment, &ext);
            width += ext.x_advance;
            if (draw_output)
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
            cairo_text_extents_t ext;
            cairo_text_extents(cr, segment, &ext);
            width += ext.x_advance;
            if (draw_output)
                cairo_show_text(cr, segment);

            p += len;
        }
    }
    return width;
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

    LayoutContext geometry = {0};
    if (!calculate_layout_context(config, params, &geometry))
        return;

    // Skip if slot is not active
    if (!slot_is_active(slot_value))
        return;

    // Get temperature and label for current slot
    const float temp_value = get_slot_temperature(data, slot_value);
    const char *label_text = get_slot_label(config, data, slot_value);
    const float max_temp = get_slot_max_scale(config, slot_value);
    const int centered = strcmp(config->circle_layout, "centered") == 0;

    char extra_buf[64] = {0};
    char line2_buf[64] = {0};
    const int has_extra_line =
        get_extra_info_text(config, data, slot_value, extra_buf, sizeof(extra_buf));
    const int has_rpm_line =
        config->circle_show_rpm &&
        get_extra_info_line2(data, slot_value, line2_buf, sizeof(line2_buf));
    const int extra_lines = has_extra_line + has_rpm_line;
    const int adaptive = !config->circle_show_bar || !config->circle_show_load ||
                         !config->circle_show_frequency || !config->circle_show_watts ||
                         !config->circle_show_rpm;

    int effective_bar_width = params->safe_bar_width;
    const int bar_height = config->circle_show_bar
        ? get_scaled_slot_bar_height(config, params, get_slot_name_by_index(current_slot_index))
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

    if (verbose_logging)
    {
        log_message(
            LOG_INFO,
            "Circle layout: slot=%s logical(height=%u gap=%u) "
            "scaled(height=%d gap=%.1f) bar_y=%d center_y=%.1f "
            "value_gap=%.1f value_box=%.1fx%.1f label_box_y=%.1f "
            "label_box_h=%.1f safe_width=%d",
            get_slot_name_by_index(current_slot_index),
            get_slot_bar_height(config, get_slot_name_by_index(current_slot_index)),
            config->layout_bar_gap, bar_height, region_gap, bar_y,
            geometry.center_y, value_bar_gap, value_box_y, value_box_height, label_box_y,
            label_box_height, effective_bar_width);
    }

    const Color *value_color = &config->font_color_temp;

    if (config->circle_show_bar)
    {
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
            calculate_duty_layout(cr, config, params, data, slot_value,
                                  row_width, value_box_height,
                                  natural_value_layout.font_size * 0.5,
                                  &duty_layout);

        double duty_region_width = 0.0;
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
            duty_region_width =
                natural_total > 0.0
                    ? available_width *
                          (duty_layout.total_width / natural_total)
                    : available_width * 0.5;
            duty_region_width =
                fmax(1.0, fmin(available_width - 1.0, duty_region_width));

            calculate_duty_layout(cr, config, params, data, slot_value,
                                  duty_region_width, value_box_height,
                                  natural_value_layout.font_size * 0.5,
                                  &duty_layout);
            value_x = row_x + duty_region_width + element_gap;
            value_width = fmax(1.0, row_x + row_width - value_x);

            layout_and_render_slot_value(cr, data, config, params, slot_value,
                                         temp_value, value_x, value_box_y,
                                         value_width, value_box_height, 1,
                                         0, &natural_value_layout);
            calculate_duty_layout(cr, config, params, data, slot_value,
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

            cairo_set_font_size(cr, extra_font_size);
            cairo_font_extents(cr, &font_ext);
            cairo_text_extents(cr, has_extra_line ? extra_buf : "",
                               &extra_text_ext);
            cairo_text_extents(cr, has_rpm_line ? line2_buf : "",
                               &line2_text_ext);
            const double scaled_line_height =
                font_ext.ascent + font_ext.descent;
            const double scaled_block_top =
                fmax(label_box_y, label_ink_bottom) + extra_padding_top;
            extra_y = scaled_block_top + font_ext.ascent;
            line2_y = has_extra_line
                          ? extra_y + scaled_line_height + extra_padding_top
                          : extra_y;
            rendered_bottom =
                scaled_block_top + (scaled_line_height * line_count) +
                (extra_padding_top * (line_count - 1));

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
                        const double width = render_text_with_small_units(
                            cr, extra_font_size, 0.0, 0.0, lines[i], 0);
                        line_x = extra_safe_x + (extra_available_width - width) * 0.5 +
                                 get_scaled_label_offset_x(config, params);
                        line_x = fmax(extra_safe_x, fmin(
                            extra_safe_x + extra_available_width - width, line_x));
                    }
                    render_text_with_small_units(cr, extra_font_size,
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
        log_message(LOG_INFO,
                    "Circle mode: slot=%s label=%s temp=%.1f extra_info=%s font=%s",
                    slot_value ? slot_value : "none",
                    label ? label : "unknown", temp,
                    (config->circle_show_frequency || config->circle_show_watts ||
                     config->circle_show_rpm) ? "on" : "off",
                    config->font_face[0] ? config->font_face : "default");
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
