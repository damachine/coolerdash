/**
 * @author damachine (christkue79@gmail.com)
 * @Maintainer: damachine <christkue79@gmail.com>
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 *    This software is provided "as is", without warranty of any kind, express or implied.
 *    I do not guarantee that it will work as intended on your system.
 */

/**
 * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
 * @details Provides functions to initialize the monitor subsystem and read CPU/GPU temperature values from the API. Liquidctl device handling is in coolercontrol.c.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "config.h"
#include "monitor.h"
#include "coolercontrol.h"

/**
 * @brief Extract temperature from device status history
 * @details Helper function to get temperature from the latest status entry
 */
static float extract_device_temperature(const json_t *device, const char *device_type)
{
    // Get status history
    const json_t *status_history = json_object_get(device, "status_history");
    if (!status_history || !json_is_array(status_history))
        return 0.0f;

    size_t history_count = json_array_size(status_history);
    if (history_count == 0)
        return 0.0f;

    // Get latest status
    const json_t *last_status = json_array_get(status_history, history_count - 1);
    if (!last_status)
        return 0.0f;

    // Get temperatures array
    const json_t *temps = json_object_get(last_status, "temps");
    if (!temps || !json_is_array(temps))
        return 0.0f;

    // Search for appropriate temperature sensor
    size_t temp_count = json_array_size(temps);
    for (size_t i = 0; i < temp_count; i++)
    {
        const json_t *temp_entry = json_array_get(temps, i);
        if (!temp_entry)
            continue;

        const json_t *name_val = json_object_get(temp_entry, "name");
        const json_t *temp_val = json_object_get(temp_entry, "temp");

        if (!name_val || !json_is_string(name_val) || !temp_val || !json_is_number(temp_val))
            continue;

        const char *sensor_name = json_string_value(name_val);
        float temperature = (float)json_number_value(temp_val);

        // Validate temperature range
        if (temperature < -50.0f || temperature > 150.0f)
            continue;

        // Check sensor name based on device type
        if (strcmp(device_type, "CPU") == 0 && strcmp(sensor_name, "temp1") == 0)
        {
            return temperature;
        }
        else if (strcmp(device_type, "GPU") == 0 &&
                 (strstr(sensor_name, "GPU") || strstr(sensor_name, "gpu")))
        {
            return temperature;
        }
    }

    return 0.0f;
}

/**
 * @brief Parse sensor JSON and extract temperatures from CPU and GPU devices.
 * @details Simplified JSON parsing to extract CPU and GPU temperature values.
 */
static int parse_temperature_data(const char *json, float *temp_cpu, float *temp_gpu)
{
    if (!json || json[0] == '\0')
    {
        log_message(LOG_ERROR, "Invalid JSON input");
        return 0;
    }

    // Initialize outputs
    if (temp_cpu)
        *temp_cpu = 0.0f;
    if (temp_gpu)
        *temp_gpu = 0.0f;

    // Parse JSON
    json_error_t json_error;
    json_t *root = json_loads(json, 0, &json_error);
    if (!root)
    {
        log_message(LOG_ERROR, "JSON parse error: %s", json_error.text);
        return 0;
    }

    // Get devices array
    const json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices))
    {
        json_decref(root);
        return 0;
    }

    // Search for CPU and GPU devices
    size_t device_count = json_array_size(devices);
    int cpu_found = 0, gpu_found = 0;

    for (size_t i = 0; i < device_count && (!cpu_found || !gpu_found); i++)
    {
        const json_t *device = json_array_get(devices, i);
        if (!device)
            continue;

        const char *device_type = extract_device_type_from_json(device);
        if (!device_type)
            continue;

        if (!cpu_found && strcmp(device_type, "CPU") == 0)
        {
            if (temp_cpu)
            {
                *temp_cpu = extract_device_temperature(device, "CPU");
                cpu_found = 1;
            }
        }
        else if (!gpu_found && strcmp(device_type, "GPU") == 0)
        {
            if (temp_gpu)
            {
                *temp_gpu = extract_device_temperature(device, "GPU");
                gpu_found = 1;
            }
        }
    }

    json_decref(root);
    return 1;
}

/**
 * @brief Configure CURL for status API request
 * @details Helper function to set up CURL options for temperature data request
 */
static void configure_status_request(CURL *curl, const char *url, struct http_response *response)
{
    // Basic CURL configuration
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CoolerDash/1.0");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // POST data for status request
    const char *post_data = "{\"all\":false,\"since\":\"1970-01-01T00:00:00.000Z\"}";
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
}

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Simplified HTTP request to get temperature data from status endpoint.
 */
static int get_temperature_data(const Config *config, float *temp_cpu, float *temp_gpu)
{
    if (!config || !temp_cpu || !temp_gpu)
        return 0;

    // Initialize outputs
    *temp_cpu = 0.0f;
    *temp_gpu = 0.0f;

    if (config->daemon_address[0] == '\0')
    {
        log_message(LOG_ERROR, "No daemon address configured");
        return 0;
    }

    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        log_message(LOG_ERROR, "Failed to initialize CURL");
        return 0;
    }

    // Build URL
    char url[256];
    int url_len = snprintf(url, sizeof(url), "%s/status", config->daemon_address);
    if (url_len < 0 || url_len >= (int)sizeof(url))
    {
        curl_easy_cleanup(curl);
        return 0;
    }

    // Initialize response buffer
    struct http_response response = {0};
    if (!cc_init_response_buffer(&response, 8192))
    {
        log_message(LOG_ERROR, "Failed to allocate response buffer");
        curl_easy_cleanup(curl);
        return 0;
    }

    // Configure request
    configure_status_request(curl, url, &response);

    // Enable SSL verification for HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Perform request and parse response
    int result = 0;
    CURLcode curl_result = curl_easy_perform(curl);
    if (curl_result == CURLE_OK)
    {
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if (response_code == 200)
        {
            result = parse_temperature_data(response.data, temp_cpu, temp_gpu);
        }
        else
        {
            log_message(LOG_ERROR, "HTTP error: %ld when fetching temperature data", response_code);
        }
    }
    else
    {
        log_message(LOG_ERROR, "CURL error: %s", curl_easy_strerror(curl_result));
    }

    // Cleanup
    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result;
}

/**
 * @brief Get all relevant sensor data (CPU/GPU temperature and LCD UID).
 * @details Reads the current CPU and GPU temperatures and LCD UID via API.
 */
int get_temperature_monitor_data(const Config *config, monitor_sensor_data_t *data)
{
    // Check if config and data pointers are valid
    if (!config || !data)
        return 0;

    // Get temperature data from monitor module
    return get_temperature_data(config, &data->temp_cpu, &data->temp_gpu);
}

/**
 * @brief Initialize the monitor component with the given configuration.
 * @details Currently does nothing but returns success. Future implementations may include initialization logic.
 */
int init_monitr(const Config *config)
{
    (void)config;
    return 1;
}
