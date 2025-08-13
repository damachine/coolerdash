/**
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief Monitor API for reading CPU and GPU temperatures via CoolerControl OpenAPI.
 * @details Provides functions to initialize the monitor subsystem and read CPU/GPU temperature values from the API. Liquidctl device handling is in coolercontrol.c.
 */

// Include necessary headers
#include <curl/curl.h>
#include <jansson.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Include project headers
#include "../include/monitor.h"
#include "../include/config.h"
#include "../include/coolercontrol.h"

/**
 * @brief Centralized logging function with consistent format.
 * @details Provides consistent logging style matching main.c implementation.
 */
static void log_message(log_level_t level, const char *format, ...) {
    // Skip INFO messages unless verbose logging is enabled
    // STATUS, WARNING, and ERROR messages are always shown
    if (level == LOG_INFO && !verbose_logging) {
        return;
    }
    
    // Log prefix and output stream
    const char *prefix[] = {"INFO", "STATUS", "WARNING", "ERROR"};
    FILE *output = (level == LOG_ERROR) ? stderr : stdout;
    
    // Log message
    fprintf(output, "[CoolerDash %s] ", prefix[level]);
    
    // Variable arguments
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    
    // Newline and flush
    fprintf(output, "\n");
    fflush(output);
}

/**
 * @brief Parse sensor JSON and extract temperatures from CPU and GPU devices.
 * @details Parses the JSON response from CoolerControl API to extract CPU and GPU temperature values from device status history.
 */
static int parse_temperature_json(const char *json, float *temp_1, float *temp_2) {
    // Validate input
    if (!json || strlen(json) == 0) {
        log_message(LOG_ERROR, "Empty or null JSON input");
        return 0;
    }
    
    // Initialize temperature variables
    if (temp_1) *temp_1 = 0.0f;
    if (temp_2) *temp_2 = 0.0f;

    // Parse JSON
    json_error_t json_error;
    json_t *root = json_loads(json, 0, &json_error);
    if (!root) {
        log_message(LOG_ERROR, "JSON parse error in monitor: %s at line %d", 
                    json_error.text, json_error.line);
        return 0;
    }

    // Get devices array
    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices)) {
        json_decref(root);
        return 0;
    }

    const size_t device_count = json_array_size(devices);
    int cpu_found = 0, gpu_found = 0;
    
    // Iterate over devices
    for (size_t i = 0; i < device_count && (!cpu_found || !gpu_found); i++) {
        json_t *dev = json_array_get(devices, i);
        if (!dev) continue;

        // Get device type
        json_t *type_val = json_object_get(dev, "type");
        if (!type_val || !json_is_string(type_val)) continue;

        // Extract device type string
        const char *type_str = json_string_value(type_val);

        // Check device type
        if ((type_str[0] != 'C' && type_str[0] != 'G') ||
            (strcmp(type_str, "CPU") != 0 && strcmp(type_str, "GPU") != 0)) continue;
        
        // Skip if already found
        if ((type_str[0] == 'C' && cpu_found) || (type_str[0] == 'G' && gpu_found)) continue;

        // Get status history
        json_t *status_history = json_object_get(dev, "status_history");
        if (!status_history || !json_is_array(status_history) || json_array_size(status_history) == 0) continue;

        // Get last status
        json_t *last_status = json_array_get(status_history, json_array_size(status_history) - 1);
        if (!last_status) continue;

        // Get temperatures
        json_t *temps = json_object_get(last_status, "temps");
        if (!temps || !json_is_array(temps)) continue;

        // Get temperature count
        const size_t temp_count = json_array_size(temps);
        
        // Iterate over temperatures
        for (size_t j = 0; j < temp_count; j++) {
            // Get temperature entry
            json_t *temp_entry = json_array_get(temps, j);
            if (!temp_entry) continue;

            // Get sensor name and temperature value
            json_t *name_val = json_object_get(temp_entry, "name");
            json_t *temp_val = json_object_get(temp_entry, "temp");
            
            // Validate temperature entry
            if (!name_val || !json_is_string(name_val) || !temp_val || !json_is_number(temp_val)) continue;

            // Extract sensor name and temperature value
            const char *sensor_name = json_string_value(name_val);
            const float temperature = (float)json_number_value(temp_val);
            
            // Basic temperature validation
            if (temperature < -50.0f || temperature > 150.0f) {
                log_message(LOG_WARNING, "Temperature %.1f°C out of range, skipping", temperature);
                continue;
            }

            // Check sensor type and assign temperature
            if (type_str[0] == 'C' && temp_1 && !cpu_found) { // CPU device
                if (sensor_name[0] == 't' && strcmp(sensor_name, "temp1") == 0) {
                    *temp_1 = temperature;
                    cpu_found = 1;
                    break;
                }
            } else if (type_str[0] == 'G' && temp_2 && !gpu_found) { // GPU device
                if ((sensor_name[0] == 'G' && strstr(sensor_name, "GPU")) ||
                    (sensor_name[0] == 'g' && strstr(sensor_name, "gpu"))) {
                    *temp_2 = temperature;
                    gpu_found = 1;
                    break;
                }
            }
        }
    }

    // Cleanup
    json_decref(root);
    return 1;
}

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Sends HTTP POST request to CoolerControl status endpoint and parses the JSON response to extract temperature values.
 */
int get_temperature_data(const Config *config, float *temp_1, float *temp_2) {
    // Validate input parameters
    if (!config || !temp_1 || !temp_2) return 0;
    
    // Initialize temperature variables
    *temp_1 = 0.0f;
    *temp_2 = 0.0f;
    
    // Validate daemon address
    if (strlen(config->daemon_address) == 0) {
        log_message(LOG_ERROR, "No daemon address configured");
        return 0;
    }
    
    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        log_message(LOG_ERROR, "Failed to initialize CURL");
        return 0;
    }
    
    // Construct URL
    char url[256];
    const int url_len = snprintf(url, sizeof(url), "%s/status", config->daemon_address);
    if (url_len < 0 || url_len >= (int)sizeof(url)) {
        curl_easy_cleanup(curl);
        return 0;
    }
    
    // Initialize response buffer
    struct http_response chunk = {0};
    const size_t initial_capacity = 8192;
    chunk.data = malloc(initial_capacity);
    if (!chunk.data) {
        log_message(LOG_ERROR, "Failed to allocate response buffer");
        curl_easy_cleanup(curl);
        return 0;
    }
    chunk.size = 0;
    chunk.capacity = initial_capacity;
    
    // Configure curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    
    // Enable SSL verification for HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    
    // Set user agent and request type
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CoolerDash/1.0");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    
    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Send POST request
    const char *post_data = "{\"all\":false,\"since\":\"1970-01-01T00:00:00.000Z\"}";
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    
    // Initialize temperature variables
    float cpu_temp = 0.0f, gpu_temp = 0.0f;
    int result = 0;
    
    // Perform request
    CURLcode curl_result = curl_easy_perform(curl);
    if (curl_result == CURLE_OK) {
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 200) {
            result = parse_temperature_json(chunk.data, &cpu_temp, &gpu_temp);
            if (result) {
                *temp_1 = cpu_temp;
                *temp_2 = gpu_temp;
            }
        } else {
            log_message(LOG_ERROR, "HTTP error: %ld when fetching temperature data", response_code);
        }
    } else {
        log_message(LOG_ERROR, "CURL error: %s", curl_easy_strerror(curl_result));
    }
    
    // Cleanup
    if (chunk.data) free(chunk.data);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return result;
}

/**
 * @brief Get all relevant sensor data (CPU/GPU temperature and LCD UID).
 * @details Reads the current CPU and GPU temperatures and LCD UID via API.
 */
int monitor_get_temperature_data(const Config *config, monitor_sensor_data_t *data) {
    // Check if config and data pointers are valid
    if (!config || !data) return 0;

    // Get temperature data from monitor module
    return get_temperature_data(config, &data->temp_1, &data->temp_2);
}

/**
 * @brief Initialize the monitor component with the given configuration.
 * @details Currently does nothing but returns success. Future implementations may include initialization logic.
 */
int monitor_init(const Config *config) {
    (void)config;
    return 1;
}

