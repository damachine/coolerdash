/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_RING_H
#define COOLERDASH_RING_H

#include "display.h"

void paint_widget_ring(cairo_t *cr, const CustomElement *element, double progress,
                       double width, double height, double scale);

/** Draw the optional sensor-value ring shared by every display mode. */
void draw_sensor_ring(cairo_t *cr, const struct Config *config,
                      const monitor_sensor_data_t *data,
                      const ScalingParams *params);

/** Draw the optional sensor-value ring for a specific sensor slot. */
void draw_sensor_ring_for_slot(cairo_t *cr, const struct Config *config,
                               const monitor_sensor_data_t *data,
                               const ScalingParams *params,
                               const char *slot_value);

/** Draw two threshold-colored ring halves for a layout sensor pair. */
void draw_split_sensor_ring(cairo_t *cr, const struct Config *config,
                            const monitor_sensor_data_t *data,
                            const ScalingParams *params,
                            const char *left_slot, const char *right_slot);

#endif
