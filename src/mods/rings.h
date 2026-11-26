/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_RINGS_H
#define COOLERDASH_RINGS_H

#include "display.h"

int paint_circle_rings_overlay(cairo_t *cr, const Config *config,
                               const monitor_sensor_data_t *data,
                               const ScalingParams *params);

#endif
