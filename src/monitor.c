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
#ifdef __has_include
#if __has_include(<curl/curl.h>)
#include <curl/curl.h>
#elif __has_include(<curl.h>)
#include <curl.h>
#else
#error "libcurl development headers not found. Install libcurl (e.g. sudo apt install libcurl4-openssl-dev)"
#endif
#else
#include <curl/curl.h>
#endif
#include <jansson.h>

#ifdef __has_include
#if __has_include(<stdio.h>)
#include <stdio.h>
#else
#include <stddef.h>
// Fallback minimal stdio declarations for static analysis environments without system headers
typedef struct _FILE FILE;
int fprintf(FILE *stream, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
#endif
#else
#include <stdio.h>
#endif

// Standard library headers
#ifdef __has_include
#if __has_include(<stdlib.h>)
#include <stdlib.h>
#else
// Fallback minimal stdlib declarations for static analysis environments
#include <stddef.h>
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
#endif
#else
#include <stdlib.h>
#endif

#ifdef __has_include
#if __has_include(<stdarg.h>)
#include <stdarg.h>
#else
// Fallback: minimal stdarg declarations for static analysis environments
typedef __builtin_va_list va_list;
#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
#endif
#else
#include <stdarg.h>
#endif

// For Cppcheck static analysis which doesn't need standard headers
#ifdef CPPCHECK
// Cppcheck doesn't require standard headers
#ifndef malloc
void *malloc(size_t size);
#endif
#ifndef calloc
void *calloc(size_t nmemb, size_t size);
#endif
#ifndef realloc
void *realloc(void *ptr, size_t size);
#endif
#ifndef free
void free(void *ptr);
#endif
#endif
#ifdef CPPCHECK
// Fallback for Cppcheck: minimal string function prototypes
#include <stddef.h>
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strstr(const char *haystack, const char *needle);
#else
#ifdef __has_include
#if __has_include(<string.h>)
#include <string.h>
#else
// Fallback: minimal prototypes if <string.h> is unavailable (e.g. static analysis environment)
#include <stddef.h>
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strstr(const char *haystack, const char *needle);
#endif
#else
#include <string.h>
#endif
#endif

// Include project headers
#include "../include/config.h"
#include "../include/monitor.h"
#include "../include/coolercontrol.h"

/**
 * @brief Parse sensor JSON and extract temperatures from CPU and GPU devices.
 * @details Parses the JSON response from CoolerControl API to extract CPU and GPU temperature values from device status history.
 */
static int parse_temperature_data(const char *json, float *temp_cpu, float *temp_gpu) {
    // Validate input (avoid potential over-read if json not guaranteed NUL within large space)
    if (!json) {
        log_message(LOG_ERROR, "Null JSON input");
        return 0;
    }
    // Quick emptiness check: first char only
    if (json[0] == '\0') {
        log_message(LOG_ERROR, "Empty JSON input");
        return 0;
    }
    
    // Initialize temperature variables
    if (temp_cpu) *temp_cpu = 0.0f;
    if (temp_gpu) *temp_gpu = 0.0f;

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
            if (type_str[0] == 'C' && temp_cpu && !cpu_found) { // CPU device
                if (sensor_name[0] == 't' && strcmp(sensor_name, "temp1") == 0) {
                    *temp_cpu = temperature;
                    cpu_found = 1;
                    break;
                }
            } else if (type_str[0] == 'G' && temp_gpu && !gpu_found) { // GPU device
                if ((sensor_name[0] == 'G' && strstr(sensor_name, "GPU")) ||
                    (sensor_name[0] == 'g' && strstr(sensor_name, "gpu"))) {
                    *temp_gpu = temperature;
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
int get_temperature_data(const Config *config, float *temp_cpu, float *temp_gpu) {
    // Validate input parameters
    if (!config || !temp_cpu || !temp_gpu) return 0;
    
    // Initialize temperature variables
    *temp_cpu = 0.0f;
    *temp_gpu = 0.0f;
    
    // Validate daemon address (array member cannot be NULL, just check first char)
    if (config->daemon_address[0] == '\0') {
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
            result = parse_temperature_data(chunk.data, &cpu_temp, &gpu_temp);
            if (result) {
                *temp_cpu = cpu_temp;
                *temp_gpu = gpu_temp;
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
int get_temperature_monitor_data(const Config *config, monitor_sensor_data_t *data) {
    // Check if config and data pointers are valid
    if (!config || !data) return 0;

    // Get temperature data from monitor module
    return get_temperature_data(config, &data->temp_cpu, &data->temp_gpu);
}

/**
 * @brief Initialize the monitor component with the given configuration.
 * @details Currently does nothing but returns success. Future implementations may include initialization logic.
 */
int init_monitr(const Config *config) {
    (void)config;
    return 1;
}

