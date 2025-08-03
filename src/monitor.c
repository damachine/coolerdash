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
 *     See function documentation for usage examples.
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
 * @example
 *     log_message(LOG_ERROR, "Temperature out of range: %.1f°C", temp);
 */
static void log_message(log_level_t level, const char *format, ...) {
    // Skip INFO messages unless verbose logging is enabled
    // STATUS, WARNING, and ERROR messages are always shown
    if (level == LOG_INFO && !verbose_logging) {
        return;
    }
    
    const char *prefix[] = {"INFO", "STATUS", "WARNING", "ERROR"};
    FILE *output = (level == LOG_ERROR) ? stderr : stdout;
    
    fprintf(output, "[CoolerDash %s] ", prefix[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    
    fprintf(output, "\n");
    fflush(output);
}

/**
 * @brief Sleep for the specified number of milliseconds.
 * @details Portable sleep function using nanosleep for precise timing.
 * @example
 *     sleep_ms(500); // Sleep for 500 milliseconds
 */
static void sleep_ms(int milliseconds) {
    if (milliseconds <= 0) return;
    
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/**
 * @brief Parse sensor JSON and extract temperatures from CPU and GPU devices.
 * @details Helper for monitor_get_sensor_data. Returns 1 on success, 0 on failure.
 * @example
 *     float cpu, gpu;
 *     parse_temperature_json(json, &cpu, &gpu);
 */
static int parse_temperature_json(const char *json, float *temp_1, float *temp_2) {
    // Validate input and initialize output parameters
    if (!json || strlen(json) == 0) {
        log_message(LOG_ERROR, "Empty or null JSON input");
        return 0;
    }
    
    // Security: Check for reasonable JSON size (prevent memory exhaustion)
    const size_t json_len = strlen(json);
    if (json_len > 10 * 1024 * 1024) { // 10MB limit - more realistic for status data
        log_message(LOG_ERROR, "JSON response too large (%zu bytes)", json_len);
        return 0;
    }
    
    if (temp_1) *temp_1 = 0.0f;
    if (temp_2) *temp_2 = 0.0f;

    // Parse JSON with error handling for better debugging
    json_error_t json_error;
    json_t *root = json_loads(json, 0, &json_error);
    if (!root) {
        log_message(LOG_ERROR, "JSON parse error in monitor: %s at line %d", 
                    json_error.text, json_error.line);
        return 0;
    }

    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices)) {
        json_decref(root);
        return 0;
    }

    // Pre-calculate array size for performance
    const size_t device_count = json_array_size(devices);
    
    // Track found temperatures for early exit optimization
    int cpu_found = 0, gpu_found = 0;
    
    // Use direct array access for better performance
    for (size_t i = 0; i < device_count && (!cpu_found || !gpu_found); i++) {
        json_t *dev = json_array_get(devices, i);
        if (!dev) continue;

        json_t *type_val = json_object_get(dev, "type");
        if (!type_val || !json_is_string(type_val)) continue;
        
        const char *type_str = json_string_value(type_val);

        // Early exit optimization: check first character before full comparison
        if ((type_str[0] != 'C' && type_str[0] != 'G') ||
            (strcmp(type_str, "CPU") != 0 && strcmp(type_str, "GPU") != 0)) continue;
        
        // Skip if we already found this type
        if ((type_str[0] == 'C' && cpu_found) || (type_str[0] == 'G' && gpu_found)) continue;

        json_t *status_history = json_object_get(dev, "status_history");
        if (!status_history || !json_is_array(status_history) || json_array_size(status_history) == 0) continue;

        json_t *last_status = json_array_get(status_history, json_array_size(status_history) - 1);
        if (!last_status) continue;

        json_t *temps = json_object_get(last_status, "temps");
        if (!temps || !json_is_array(temps)) continue;

        // Pre-calculate array size for performance
        const size_t temp_count = json_array_size(temps);
        
        // Use direct array access for better performance
        for (size_t j = 0; j < temp_count; j++) {
            json_t *temp_entry = json_array_get(temps, j);
            if (!temp_entry) continue;

            json_t *name_val = json_object_get(temp_entry, "name");
            json_t *temp_val = json_object_get(temp_entry, "temp");
            
            if (!name_val || !json_is_string(name_val) || !temp_val || !json_is_number(temp_val)) continue;

            const char *sensor_name = json_string_value(name_val);
            const float temperature = (float)json_number_value(temp_val);
            
            // Validate temperature range (reasonable limits for CPU/GPU)
            if (temperature < -50.0f || temperature > 150.0f) {
                log_message(LOG_WARNING, "Temperature %.1f°C out of range, skipping", temperature);
                continue;
            }

            // Optimized string comparison with early exit
            if (type_str[0] == 'C' && temp_1 && !cpu_found) { // CPU device
                if (sensor_name[0] == 't' && strcmp(sensor_name, "temp1") == 0) {
                    *temp_1 = temperature;
                    cpu_found = 1;
                    break; // Exit inner loop early
                }
            } else if (type_str[0] == 'G' && temp_2 && !gpu_found) { // GPU device
                // Check for GPU sensor (case-insensitive)
                if ((sensor_name[0] == 'G' && strstr(sensor_name, "GPU")) ||
                    (sensor_name[0] == 'g' && strstr(sensor_name, "gpu"))) {
                    *temp_2 = temperature;
                    gpu_found = 1;
                    break; // Exit inner loop early
                }
            }
        }
    }

    json_decref(root);
    return 1;
}

/**
 * @brief Get CPU and GPU temperature data from CoolerControl API.
 * @details Reads the current CPU and GPU temperatures via API.
 * @example
 *     float temp_1, temp_2;
 *     if (get_temperature_data(&config, &temp_1, &temp_2)) { ... }
 */
int get_temperature_data(const Config *config, float *temp_1, float *temp_2) {
    // Check if config and temperature pointers are valid
    if (!config || !temp_1 || !temp_2) return 0;
    
    // Initialize output values to safe defaults
    *temp_1 = 0.0f;
    *temp_2 = 0.0f;
    
    // Validate daemon address is present
    if (strlen(config->daemon_address) == 0) {
        log_message(LOG_ERROR, "No daemon address configured");
        return 0;
    }
    
    // Retry loop with exponential backoff
    int retry_delay = MONITOR_INITIAL_RETRY_DELAY_MS;
    
    for (int attempt = 1; attempt <= MONITOR_MAX_RETRIES; attempt++) {
        CURL *curl = curl_easy_init();
        if (!curl) {
            log_message(LOG_ERROR, "Failed to initialize CURL (attempt %d/%d)", attempt, MONITOR_MAX_RETRIES);
            if (attempt < MONITOR_MAX_RETRIES) {
                sleep_ms(retry_delay);
                retry_delay = (retry_delay * 2 > MONITOR_MAX_RETRY_DELAY_MS) ? MONITOR_MAX_RETRY_DELAY_MS : retry_delay * 2;
            }
            continue;
        }
        
        // Construct URL with size validation
        char url[256];
        const int url_len = snprintf(url, sizeof(url), "%s/status", config->daemon_address);
        if (url_len < 0 || url_len >= (int)sizeof(url)) {
            curl_easy_cleanup(curl);
            return 0;
        }
        
        // Initialize response buffer with reasonable starting capacity
        struct http_response chunk = {0};
        const size_t initial_capacity = 8192;
        chunk.data = malloc(initial_capacity);
        if (!chunk.data) {
            log_message(LOG_ERROR, "Failed to allocate response buffer (attempt %d/%d)", attempt, MONITOR_MAX_RETRIES);
            curl_easy_cleanup(curl);
            if (attempt < MONITOR_MAX_RETRIES) {
                sleep_ms(retry_delay);
                retry_delay = (retry_delay * 2 > MONITOR_MAX_RETRY_DELAY_MS) ? MONITOR_MAX_RETRY_DELAY_MS : retry_delay * 2;
            }
            continue;
        }
        chunk.size = 0;
        chunk.capacity = initial_capacity;
        
        // Configure curl options with enhanced security and error handling
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // Reasonable timeout for status check
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L); // Quick connection timeout
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L); // Security: no redirects
        
        // Only enable SSL verification if using HTTPS
        if (strncmp(config->daemon_address, "https://", 8) == 0) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Verify SSL certificates for HTTPS
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); // Verify SSL hostname for HTTPS
        }
        
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "CoolerDash/1.0"); // Identify our application
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
        
        // Perform request and parse response with enhanced error handling
        CURLcode curl_result = curl_easy_perform(curl);
        if (curl_result == CURLE_OK) {
            // Check HTTP response code
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            
            if (response_code == 200) {
                result = parse_temperature_json(chunk.data, &cpu_temp, &gpu_temp);
                if (result) {
                    // Success! Set output values and clean up
                    *temp_1 = cpu_temp;
                    *temp_2 = gpu_temp;
                    
                    if (chunk.data) {
                        free(chunk.data);
                        chunk.data = NULL;
                    }
                    if (headers) {
                        curl_slist_free_all(headers);
                        headers = NULL;
                    }
                    curl_easy_cleanup(curl);
                    
                    if (attempt > 1) {
                        log_message(LOG_INFO, "Temperature data retrieved successfully on attempt %d", attempt);
                    }
                    return 1;
                } else {
                    log_message(LOG_WARNING, "Failed to parse temperature JSON (attempt %d/%d)", attempt, MONITOR_MAX_RETRIES);
                }
            } else {
                log_message(LOG_ERROR, "HTTP error: %ld when fetching temperature data (attempt %d/%d)", 
                           response_code, attempt, MONITOR_MAX_RETRIES);
            }
        } else {
            log_message(LOG_ERROR, "CURL error: %s (attempt %d/%d)", 
                       curl_easy_strerror(curl_result), attempt, MONITOR_MAX_RETRIES);
        }
        
        // Clean up resources for this attempt
        if (chunk.data) {
            free(chunk.data);
            chunk.data = NULL;
        }
        if (headers) {
            curl_slist_free_all(headers);
            headers = NULL;
        }
        curl_easy_cleanup(curl);
        
        // Wait before next attempt (if not the last attempt)
        if (attempt < MONITOR_MAX_RETRIES) {
            log_message(LOG_INFO, "Retrying temperature data request in %dms...", retry_delay);
            sleep_ms(retry_delay);
            retry_delay = (retry_delay * 2 > MONITOR_MAX_RETRY_DELAY_MS) ? MONITOR_MAX_RETRY_DELAY_MS : retry_delay * 2;
        }
    }
    
    log_message(LOG_ERROR, "Failed to get temperature data after %d attempts", MONITOR_MAX_RETRIES);
    return 0;
}

/**
 * @brief Get all relevant sensor data (CPU/GPU temperature and LCD UID).
 * @details Reads the current CPU and GPU temperatures and LCD UID via API.
 * @example
 *     cc_sensor_data_t data;
 *     if (monitor_get_sensor_data(&config, &data)) { ... }
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
 * @example
 *     if (monitor_init(&config)) { ... }
 */
int monitor_init(const Config *config) {
    (void)config;
    return 1;
}

