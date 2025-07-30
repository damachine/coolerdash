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
 * @brief Get all relevant sensor data (temperature and LCD UID).
 * @details Reads the current CPU and GPU temperatures and LCD UID via API. Returns 1 on success, 0 on failure. Exits if no Liquidctl device is found.
 * @example
 *     cc_sensor_data_t data;
 *     if (monitor_get_sensor_data(&config, &data)) { ... }
 */
int monitor_get_sensor_data(const Config *config, cc_sensor_data_t *data);

#endif // MONITOR_H
