/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

#ifndef MONITOR_H
#define MONITOR_H

#include "config.h"
#include "coolercontrol.h"

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Reads the current CPU and GPU temperatures via API. Returns 1 on success, 0 on failure.
 * @example
 *     float temp_1, temp_2;
 *     if (get_temperature_data(&config, &temp_1, &temp_2)) { ... }
 */
int get_temperature_data(const Config *config, float *temp_1, float *temp_2);

#endif // MONITOR_H
