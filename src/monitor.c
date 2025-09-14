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
#include "../include/config.h"
#include "../include/monitor.h"
#include "../include/coolercontrol.h"

/**
 * @brief Validate temperature sensor name for device type
 * @details Helper function to check if sensor name matches device type
 */
static int is_valid_sensor_for_device(const char *sensor_name, const char *device_type)
{
    if (strcmp(device_type, "CPU") == 0 && strcmp(sensor_name, "temp1") == 0)
    {
        return 1;
    }
    
    if (strcmp(device_type, "GPU") == 0 &&
        (strstr(sensor_name, "GPU") || strstr(sensor_name, "gpu")))
    {
        return 1;
    }
    
    return 0;
}

/**
 * @brief Validate temperature value is within acceptable range
 * @details Helper function to check temperature bounds
 */
static int is_temperature_valid(float temperature)
{
    return temperature >= -50.0f && temperature <= 150.0f;
}

/**
 * @brief Extract temperature from single sensor entry
 * @details Helper function to process one temperature sensor entry
 */
static float extract_temperature_from_sensor(json_t *temp_entry, const char *device_type)
{
    json_t *name_val = json_object_get(temp_entry, "name");
    json_t *temp_val = json_object_get(temp_entry, "temp");

    if (!name_val || !json_is_string(name_val) || !temp_val || !json_is_number(temp_val))
        return 0.0f;

    const char *sensor_name = json_string_value(name_val);
    float temperature = (float)json_number_value(temp_val);

    if (!is_temperature_valid(temperature))
        return 0.0f;

    if (is_valid_sensor_for_device(sensor_name, device_type))
        return temperature;

    return 0.0f;
}

/**
 * @brief Get latest status from device history
 * @details Helper function to extract the most recent status entry
 */
static json_t *get_latest_device_status(json_t *device)
{
    json_t *status_history = json_object_get(device, "status_history");
    if (!status_history || !json_is_array(status_history))
        return NULL;

    size_t history_count = json_array_size(status_history);
    if (history_count == 0)
        return NULL;

    return json_array_get(status_history, history_count - 1);
}

/**
 * @brief Extract temperature from device status history
 * @details Helper function to get temperature from the latest status entry
 */
static float extract_device_temperature(json_t *device, const char *device_type)
{
    json_t *last_status = get_latest_device_status(device);
    if (!last_status)
        return 0.0f;

    json_t *temps = json_object_get(last_status, "temps");
    if (!temps || !json_is_array(temps))
        return 0.0f;

    size_t temp_count = json_array_size(temps);
    for (size_t i = 0; i < temp_count; i++)
    {
        json_t *temp_entry = json_array_get(temps, i);
        if (!temp_entry)
            continue;

        float temperature = extract_temperature_from_sensor(temp_entry, device_type);
        if (temperature > 0.0f)
            return temperature;
    }

    return 0.0f;
}

/**
 * @brief Initialize temperature output values
 * @details Helper function to set initial temperature values
 */
static void initialize_temperature_outputs(float *temp_cpu, float *temp_gpu)
{
    if (temp_cpu)
        *temp_cpu = 0.0f;
    if (temp_gpu)
        *temp_gpu = 0.0f;
}

/**
 * @brief Parse JSON root and get devices array
 * @details Helper function to parse JSON and extract devices array
 */
static json_t *parse_json_and_get_devices(const char *json)
{
    json_error_t json_error;
    json_t *root = json_loads(json, 0, &json_error);
    if (!root)
    {
        log_message(LOG_ERROR, "JSON parse error: %s", json_error.text);
        return NULL;
    }

    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices))
    {
        json_decref(root);
        return NULL;
    }

    return root;
}

/**
 * @brief Process device and extract temperature if matching type
 * @details Helper function to check device type and extract temperature
 */
static void process_device_temperature(json_t *device, const char *target_type, 
                                     float *temperature, int *found_flag)
{
    const char *device_type = extract_device_type_from_json(device);
    if (!device_type || *found_flag)
        return;

    if (strcmp(device_type, target_type) == 0)
    {
        if (temperature)
        {
            *temperature = extract_device_temperature(device, target_type);
            *found_flag = 1;
        }
    }
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

    initialize_temperature_outputs(temp_cpu, temp_gpu);

    json_t *root = parse_json_and_get_devices(json);
    if (!root)
        return 0;

    json_t *devices = json_object_get(root, "devices");
    size_t device_count = json_array_size(devices);
    int cpu_found = 0, gpu_found = 0;

    for (size_t i = 0; i < device_count && (!cpu_found || !gpu_found); i++)
    {
        json_t *device = json_array_get(devices, i);
        if (!device)
            continue;

        process_device_temperature(device, "CPU", temp_cpu, &cpu_found);
        process_device_temperature(device, "GPU", temp_gpu, &gpu_found);
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
int get_temperature_data(const Config *config, float *temp_cpu, float *temp_gpu)
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
