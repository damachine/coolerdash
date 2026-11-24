/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_CUSTOM_H
#define COOLERDASH_CUSTOM_H

#include "display.h"

int paint_custom_overlay(cairo_t *cr, const Config *config,
                         const monitor_sensor_data_t *data,
                         const ScalingParams *params);

int paint_activity_rings_overlay(cairo_t *cr, const Config *config,
                                 const monitor_sensor_data_t *data,
                                 const ScalingParams *params);

#endif
