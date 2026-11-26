/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_CIRCLE_RENDER_H
#define COOLERDASH_CIRCLE_RENDER_H

#include "display.h"

void paint_circle_sensor(cairo_t *cr, const Config *config, const ScalingParams *params,
                         const monitor_sensor_data_t *data, const char *slot_value,
                         const char *slot_name);

#endif
