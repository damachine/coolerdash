/* SPDX-License-Identifier: MIT */
#ifndef COOLERDASH_READINGS_H
#define COOLERDASH_READINGS_H

#include "../device/config.h"

typedef struct
{
    sensor_entry_t sensor;
    double gib, memory_load;
    int memory_status;
} WidgetReadings;

int reading_is_valid(const sensor_entry_t *sensor);
int read_processor_power(const monitor_sensor_data_t *data, double *watts, const char **label);
int read_used_memory(const Config *config, double *gib, double *progress);
const sensor_entry_t *get_widget_reading(const Config *config, const monitor_sensor_data_t *data,
                                         const CustomElement *element, WidgetReadings *readings);
double widget_progress(const CustomElement *element, double value);

#endif
