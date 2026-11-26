/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_TEXT_H
#define COOLERDASH_TEXT_H

#include "display.h"

/**
 * @brief Measured layout for a rendered sensor value block.
 */
typedef struct
{
    int active;
    double block_left;
    double block_right;
    double block_top;
    double block_bottom;
    double baseline_y;
    double font_size;
} SlotValueLayout;

/** @brief Proportionally fit text into a box by changing only its font size. */
double fit_text_font_size(cairo_t *cr, const char *text,
                          double preferred_size, double max_width,
                          double max_height, double minimum_size);

/**
 * @brief Format a sensor value using the configured decimal mode.
 */
void format_slot_value_text(char *buffer, size_t buffer_size,
                            const monitor_sensor_data_t *data,
                            const char *slot_value, float temp_value);

/**
 * @brief Measure and optionally render a right-aligned sensor value block.
 * @details Fits the font size to the available box width and height and places
 * the value against the right edge of the provided box. Offsets remain as a
 * user-facing fine-tuning layer.
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
                                  SlotValueLayout *layout);

void render_centered_text(cairo_t *cr, const char *text, double center_x, double center_y,
                          double preferred_size, double max_width, double max_height,
                          const Color *color);
void render_centered_value_with_small_unit(cairo_t *cr, const char *text, double center_x,
                                           double center_y, double preferred_size,
                                           double max_width, double max_height,
                                           const Color *color, const ScalingParams *params);
double render_centered_temperature(cairo_t *cr, const char *number, double center_x,
                                   double center_y, double preferred_size, double max_width,
                                   double max_height, const Color *color, const ScalingParams *params);

#endif
