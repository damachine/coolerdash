/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
 * @details Provides functions to initialize the monitor subsystem and read CPU/GPU temperature values from the API. Liquidctl device handling is in coolercontrol.c.
 * @example
 *     float temp_1, temp_2;
 *     if (get_temperature_data(&config, &temp_1, &temp_2)) { ... }
 */

// Include necessary headers
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Include project headers
#include "../include/monitor.h"
#include "../include/config.h"
#include "../include/coolercontrol.h"

/**
 * @brief Parse sensor JSON and extract temperatures from CPU and GPU devices.
 * @details Helper for monitor_get_sensor_data. Returns 1 on success, 0 on failure.
 * @example
 *     float cpu, gpu;
 *     parse_temperature_json(json, &cpu, &gpu);
 */
static int parse_temperature_json(const char *json, float *temp_1, float *temp_2) {
    // Validate input and initialize output parameters
    if (!json) return 0;
    
    if (temp_1) *temp_1 = 0.0f;
    if (temp_2) *temp_2 = 0.0f;

    // Parse JSON
    json_t *root = json_loads(json, 0, NULL);
    if (!root) return 0;

    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices)) {
        json_decref(root);
        return 0;
    }

    size_t i;
    json_t *dev;
    json_array_foreach(devices, i, dev) {
        json_t *type_val = json_object_get(dev, "type");
        if (!type_val || !json_is_string(type_val)) continue;
        
        const char *type_str = json_string_value(type_val);

        // Extract temperatures from CPU or GPU devices
        if (strcmp(type_str, "CPU") != 0 && strcmp(type_str, "GPU") != 0) continue;

        json_t *status_history = json_object_get(dev, "status_history");
        if (!status_history || !json_is_array(status_history) || json_array_size(status_history) == 0) continue;

        json_t *last_status = json_array_get(status_history, json_array_size(status_history) - 1);
        if (!last_status) continue;

        json_t *temps = json_object_get(last_status, "temps");
        if (!temps || !json_is_array(temps)) continue;

        size_t j;
        json_t *temp_entry;
        json_array_foreach(temps, j, temp_entry) {
            json_t *name_val = json_object_get(temp_entry, "name");
            json_t *temp_val = json_object_get(temp_entry, "temp");
            
            if (!name_val || !json_is_string(name_val) || !temp_val || !json_is_number(temp_val)) continue;

            const char *sensor_name = json_string_value(name_val);
            float temperature = (float)json_number_value(temp_val);

            // Extract temp_1 from CPU device (temp1 sensor)
            if (strcmp(type_str, "CPU") == 0 && strcmp(sensor_name, "temp1") == 0 && temp_1) {
                *temp_1 = temperature;
            }
            // Extract temp_2 from GPU device (sensors containing "GPU" or "gpu")
            else if (strcmp(type_str, "GPU") == 0 && (strstr(sensor_name, "GPU") || strstr(sensor_name, "gpu")) && temp_2) {
                *temp_2 = temperature;
            }
        }
    }

    json_decref(root);
    return 1;
}

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Reads the current CPU and GPU temperatures via API. Returns 1 on success, 0 on failure.
 * @example
 *     float temp_1, temp_2;
 *     if (get_temperature_data(&config, &temp_1, &temp_2)) { ... }
 */
int get_temperature_data(const Config *config, float *temp_1, float *temp_2) {
    // Check if config and temperature pointers are valid
    if (!config || !temp_1 || !temp_2) return 0;
    
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    
    // Construct URL
    char url[256];
    snprintf(url, sizeof(url), "%s/status", config->daemon_address);
    
    // Initialize response buffer
    struct http_response chunk = {0};
    chunk.data = malloc(1);
    if (!chunk.data) {
        curl_easy_cleanup(curl);
        return 0;
    }
    chunk.size = 0;
    
    // Configure curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L); // Set a timeout to avoid hanging
    curl_easy_setopt(curl, CURLOPT_POST, 1L); // Use POST to ensure we get the latest data
    
    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Send POST request with empty body
    const char *post_data = "{\"all\":false,\"since\":\"1970-01-01T00:00:00.000Z\"}";
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    
    // Initialize parsing variables
    float cpu_temp = 0.0f, gpu_temp = 0.0f; // Initialize temperatures
    int result = 0;
    
    // Perform request and parse response
    if (curl_easy_perform(curl) == CURLE_OK) {
        result = parse_temperature_json(chunk.data, &cpu_temp, &gpu_temp);
    }
    
    // Clean up resources
    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    // Set output values
    *temp_1 = cpu_temp;
    *temp_2 = gpu_temp;
    
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
    // Check if config and data pointers are valid
    if (!config || !data) return 0;

    // Get temperature data from monitor module
    float temp_1, temp_2;
    if (!get_temperature_data(config, &temp_1, &temp_2)) {
        return 0;
    }

    // If device_uid is already set, use it; otherwise get it from API
    if (data->device_uid[0] == '\0') {
        // Get LCD device UID from coolercontrol module
        char device_uid[CC_UID_SIZE];
        if (!get_liquidctl_device_uid(config, device_uid, sizeof(device_uid))) {
            return 0;
        }
        strncpy(data->device_uid, device_uid, sizeof(data->device_uid) - 1);
        data->device_uid[sizeof(data->device_uid) - 1] = '\0';
    }

    // Fill the temperature data
    data->temp_1 = temp_1;
    data->temp_2 = temp_2;

    return 1;
}

/**
 * @brief Initialize the monitor component with the given configuration.
 * @details Currently does nothing but returns success. Future implementations may include initialization logic.
 * @example
 *     if (monitor_init(&config)) { ... }
 */
int monitor_init(const Config *config) {
    (void)config;
    return 1;
}

