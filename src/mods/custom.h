/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_CUSTOM_H
#define COOLERDASH_CUSTOM_H

#include <stdint.h>

#include "display.h"

void reset_custom_state(void);
int64_t custom_screen_deadline_us(const Config *config);

int paint_custom_overlay(cairo_t *cr, const Config *config,
                         const monitor_sensor_data_t *data,
                         const ScalingParams *params);

int paint_activity_rings_overlay(cairo_t *cr, const Config *config,
                                 const monitor_sensor_data_t *data,
                                 const ScalingParams *params);

#endif
