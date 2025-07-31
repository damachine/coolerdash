/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
 * @details Provides functions to initialize the monitor subsystem and read sensor values (CPU/GPU) from the API.
 * @example
 *     float cpu, gpu;
 *     if (monitor_get_sensor_data(&config, &cpu, &gpu)) { ... }
 */

// Include project headers
#include "../include/monitor.h"
#include "../include/config.h"
#include "../include/coolercontrol.h"

// Include necessary headers
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>


int monitor_init(const Config *config) {
    (void)config;
    return 1;
}

/**
 * @brief Parse sensor JSON and extract temperatures and LCD UID.
 * @details Helper for monitor_get_sensor_data. Returns 1 on success, 0 on failure.
 * @example
 *     float cpu, gpu; char uid[128];
 *     parse_sensor_json(json, &cpu, &gpu, uid, sizeof(uid), &found_liquidctl);
 */
static int parse_sensor_json(const char *json, float *temp_1, float *temp_2, char *lcd_uid, size_t uid_size, int *found_liquidctl) {
    if (!json) return 0; // Check if JSON is valid
    int result = 0;
    if (temp_1) *temp_1 = 0.0f;
    if (temp_2) *temp_2 = 0.0f;
    if (lcd_uid && uid_size > 0) lcd_uid[0] = '\0';
    if (found_liquidctl) *found_liquidctl = 0;
    json_t *root = json_loads(json, 0, NULL); // Load JSON from string
    if (root) {
        json_t *devices = json_object_get(root, "devices"); // Get the "devices" array
        if (devices && json_is_array(devices)) {
            size_t i;
            json_t *dev;
            json_array_foreach(devices, i, dev) {
                json_t *type_val = json_object_get(dev, "type"); // Check if type is present and is a string
                if (!type_val || !json_is_string(type_val)) continue;
                const char *type_str = json_string_value(type_val);
                // Check for Liquidctl device type is present, mostly this is the LCD devices 
                if (strcmp(type_str, "Liquidctl") == 0) {
                    if (found_liquidctl) *found_liquidctl = 1;
                    if (lcd_uid && uid_size > 0) {
                        json_t *uid_val = json_object_get(dev, "uid");
                        if (uid_val && json_is_string(uid_val)) {
                            strncpy(lcd_uid, json_string_value(uid_val), uid_size-1);
                            lcd_uid[uid_size-1] = '\0';
                        }
                    }
                }
                // Extract temp_1 temperatures
                if (strcmp(type_str, "CPU") == 0) {
                    json_t *status_history = json_object_get(dev, "status_history");
                    if (!status_history || !json_is_array(status_history) || json_array_size(status_history) == 0) continue;
                    json_t *last_status = json_array_get(status_history, json_array_size(status_history)-1);
                    if (!last_status) continue;
                    json_t *temps = json_object_get(last_status, "temps");
                    if (!temps || !json_is_array(temps)) continue;
                    size_t j;
                    json_t *temp_entry;
                    json_array_foreach(temps, j, temp_entry) {
                        json_t *name_val = json_object_get(temp_entry, "name"); // Check if name is present and is a string
                        json_t *temp_val = json_object_get(temp_entry, "temp"); // Check if temp is present and is a number
                        // Check if name and temp are valid
                        if (!name_val || !json_is_string(name_val) || !temp_val || !json_is_number(temp_val)) continue;
                        // Check if sensor is the config_temp_1 in the .ini file
                        if (strcmp(json_string_value(name_val), "temp1") == 0 && temp_1) {
                            *temp_1 = (float)json_number_value(temp_val);
                        }
                    }
                }
                // Extract temp_2 temperature
                if (strcmp(type_str, "GPU") == 0) {
                    json_t *status_history = json_object_get(dev, "status_history");
                    if (!status_history || !json_is_array(status_history) || json_array_size(status_history) == 0) continue;
                    json_t *last_status = json_array_get(status_history, json_array_size(status_history)-1);
                    if (!last_status) continue;
                    json_t *temps = json_object_get(last_status, "temps");
                    if (!temps || !json_is_array(temps)) continue;
                    size_t j;
                    json_t *temp_entry;
                    json_array_foreach(temps, j, temp_entry) {
                        json_t *name_val = json_object_get(temp_entry, "name"); // Check if name is present and is a string
                        json_t *temp_val = json_object_get(temp_entry, "temp"); // Check if temp is present and is a number
                        // Check if name and temp are valid
                        if (!name_val || !json_is_string(name_val) || !temp_val || !json_is_number(temp_val)) continue;
                        const char *sensor_name = json_string_value(name_val);
                        // Check if sensor is the config_temp_2 in the .ini file
                        if ((strstr(sensor_name, "GPU") || strstr(sensor_name, "gpu")) && temp_2) {
                            *temp_2 = (float)json_number_value(temp_val);
                        }
                    }
                }
            }
            result = 1;
        }
        json_decref(root);
    }
    return result;
}

/**
 * @brief Get all relevant sensor data (CPU/GPU temperature and LCD UID).
 * @details Reads the current CPU and GPU temperatures and LCD UID via API. Returns 1 on success, 0 on failure.
 * @example
 *     cc_sensor_data_t data;
 *     if (monitor_get_sensor_data(&config, &data)) { ... }
 */
int monitor_get_sensor_data(const Config *config, cc_sensor_data_t *data) {
    if (!config || !data) return 0; // Check if config and data pointers are valid
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    char url[256];
    snprintf(url, sizeof(url), "%s/status", config->daemon_address);
    struct http_response chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L); // Set a timeout to avoid hanging
    curl_easy_setopt(curl, CURLOPT_POST, 1L); // Use POST to ensure we get the latest data
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
    const char *post_data = "{\"all\":false,\"since\":\"1970-01-01T00:00:00.000Z\"}"; // Empty POST body
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data); // Send POST request with empty body
    int found_liquidctl = 0; // Flag to check if Liquidctl device is found
    float temp_1 = 0.0f, temp_2 = 0.0f; // Initialize temperatures
    char lcd_uid[128] = ""; // Buffer for LCD UID
    int result = 0;
    if (curl_easy_perform(curl) == CURLE_OK) {
        result = parse_sensor_json(chunk.data, &temp_1, &temp_2, lcd_uid, sizeof(lcd_uid), &found_liquidctl);
    }
    // Clean up
    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (!found_liquidctl) {
        fprintf(stderr, "[coolerdash] ERROR: No Liquidctl device found. Exiting.\n");
        return 0;
    }
    // Fill the cc_sensor_data_t structure with the parsed data
    data->temp_1 = temp_1;
    data->temp_2 = temp_2;
    strncpy(data->device_uid, lcd_uid, sizeof(data->device_uid)-1);
    data->device_uid[sizeof(data->device_uid)-1] = '\0';
    return result;
}
