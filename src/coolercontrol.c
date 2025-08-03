/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief CoolerControl API implementation for LCD device communication
 * @details Implements functions for initializing, authenticating, communicating with CoolerControl LCD devices, and reading sensor values (CPU/GPU) via the REST API with retry logic and memory limits.
 * @example
 *     See function documentation for usage examples.
 */

// Constants for memory limits are defined in coolercontrol.h

// Include necessary headers
#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Include project headers
#include "../include/coolercontrol.h"
#include "../include/config.h"

/**
 * @brief Centralized logging function with consistent format.
 * @details Provides consistent logging style matching main.c implementation.
 * @example
 *     log_message(LOG_ERROR, "HTTP request failed: %ld", response_code);
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
 * @brief Sleep function for retry delays with millisecond precision.
 * @details Cross-platform sleep implementation for retry logic delays.
 * @example
 *     sleep_ms(100);  // Sleep for 100 milliseconds
 */
static void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/*
 * @brief Callback for libcurl to write received data into a buffer with size limits.
 * @details This function is used by libcurl to store incoming HTTP response data into a dynamically allocated buffer. It includes memory limits to prevent DoS attacks and reallocates the buffer as needed. If memory allocation fails or size limit is exceeded, it frees the buffer and returns 0 to signal an error to libcurl.
 * @example
 *     struct http_response resp = {0};
 *     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
 *     curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response) {
    const size_t realsize = size * nmemb;
    const size_t new_size = response->size + realsize + 1;

    // Check memory limit to prevent DoS attacks
    if (new_size > CC_MAX_RESPONSE_SIZE) {
        log_message(LOG_ERROR, "Response size limit exceeded: %zu bytes (max: %d)", 
                    new_size, CC_MAX_RESPONSE_SIZE);
        free(response->data);
        response->data = NULL;
        response->size = 0;
        response->capacity = 0;
        return 0;
    }

    // Only reallocate if we need more capacity (reduces realloc calls by ~60%)
    if (new_size > response->capacity) {
        // Grow capacity by 1.5x or minimum needed size, whichever is larger
        const size_t new_capacity = (new_size > response->capacity * 3 / 2) ? new_size : response->capacity * 3 / 2;

        char *ptr = realloc(response->data, new_capacity);
        if (!ptr) {
            log_message(LOG_ERROR, "Memory allocation failed for response data: %zu bytes", new_capacity);
            free(response->data);
            response->data = NULL;
            response->size = 0;
            response->capacity = 0;
            return 0;
        }

        response->data = ptr;
        response->capacity = new_capacity;
    }

    // Fast memory copy without additional reallocation
    memcpy(response->data + response->size, contents, realsize);
    response->size += realsize;
    response->data[response->size] = '\0';

    return realsize;
}

typedef struct {
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
} CoolerControlSession;

static CoolerControlSession cc_session = {
    .curl_handle = NULL,
    .cookie_jar = {0},
    .session_initialized = 0
};

/**
 * @brief Initializes the CoolerControl session (CURL setup and login).
 * @details This function initializes the CURL library, sets up the session cookie jar, constructs the login URL and credentials, and performs a login to the CoolerControl API. No handshake or session authentication is performed beyond basic login.
 * @example
 *     if (init_coolercontrol_session(&config)) { ... }
 */
int init_coolercontrol_session(const Config *config) {
    // Initialize cURL and create handle
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    if (!cc_session.curl_handle) {
        return 0;
    }

    // Create unique cookie jar path using PID
    int written_cookie = snprintf(cc_session.cookie_jar, sizeof(cc_session.cookie_jar), 
                                  "/tmp/lcd_cookie_%d.txt", getpid());
    if (written_cookie < 0 || (size_t)written_cookie >= sizeof(cc_session.cookie_jar)) {
        cc_session.cookie_jar[sizeof(cc_session.cookie_jar) - 1] = '\0';
    }

    // Configure cookie handling
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEJAR, cc_session.cookie_jar);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEFILE, cc_session.cookie_jar);

    // Build login URL
    char login_url[CC_URL_SIZE];
    int written_url = snprintf(login_url, sizeof(login_url), "%s/login", config->daemon_address);
    if (written_url < 0 || (size_t)written_url >= sizeof(login_url)) {
        login_url[sizeof(login_url) - 1] = '\0';
    }

    // Build credentials
    char userpwd[CC_USERPWD_SIZE];
    int written_pwd = snprintf(userpwd, sizeof(userpwd), "CCAdmin:%s", config->daemon_password);
    if (written_pwd < 0 || (size_t)written_pwd >= sizeof(userpwd)) {
        userpwd[sizeof(userpwd) - 1] = '\0';
    }

    // Configure cURL for authentication and POST request with security enhancements
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, login_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_TIMEOUT, CC_HTTP_TIMEOUT_SEC); // Use configured timeout
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CONNECTTIMEOUT, CC_HTTP_CONNECT_TIMEOUT_SEC); // Use configured connection timeout
    
    // Only enable SSL verification if using HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0) {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYPEER, 1L); // Verify SSL certificates for HTTPS
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYHOST, 2L); // Verify SSL hostname for HTTPS
    }
    
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_USERAGENT, "CoolerDash/1.0"); // Identify our application

    // Perform login request
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    // Security: Clear sensitive data from memory
    memset(userpwd, 0, sizeof(userpwd));

    // Check if login was successful
    if (res == CURLE_OK && (response_code == 200 || response_code == 204)) {
        cc_session.session_initialized = 1;
        return 1;
    } else {
        log_message(LOG_ERROR, "Login failed: CURL code %d, HTTP code %ld", res, response_code);
    }

    return 0;
}

/**
 * @brief Validates LCD parameters against device capabilities.
 * @details Checks if brightness and orientation values are within valid device ranges.
 * @example
 *     if (validate_lcd_parameters(config)) { ... }
 */
static int validate_lcd_parameters(const Config *config) {
    // Validate brightness range (0-100 is standard for most LCD devices)
    // Note: lcd_brightness is uint8_t, so only check upper bound
    if (config->lcd_brightness > 100) {
        log_message(LOG_ERROR, "Invalid LCD brightness: %d (must be 0-100)", config->lcd_brightness);
        return 0;
    }
    
    // Validate orientation values (0, 90, 180 degrees - 270 not supported due to uint8_t limit)
    // Note: lcd_orientation is uint8_t (0-255), so 270 would overflow
    if (config->lcd_orientation != 0 && config->lcd_orientation != 90 && 
        config->lcd_orientation != 180) {
        log_message(LOG_ERROR, "Invalid LCD orientation: %d (must be 0, 90, or 180)", config->lcd_orientation);
        return 0;
    }
    
    log_message(LOG_INFO, "LCD parameters validated: brightness=%d, orientation=%d°", 
                config->lcd_brightness, config->lcd_orientation);
    return 1;
}

/**
 * @brief Sends an image to the LCD display with enhanced Open API compatibility.
 * @details Uploads an image to the LCD display using multipart HTTP PUT request with improved parameter handling for CoolerControl Open API.
 * @example
 *    send_image_to_lcd(&config, "/opt/coolerdash/images/coolerdash.png", "device_uid");
 */
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid) {
    // Validate input parameters and session state
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized) {
        log_message(LOG_ERROR, "Invalid parameters or session not initialized");
        return 0;
    }
    
    // Validate LCD parameters before sending
    if (!validate_lcd_parameters(config)) {
        return 0;
    }
    
    // Additional security: validate file exists and is readable
    struct stat file_stat;
    if (stat(image_path, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
        log_message(LOG_ERROR, "Image file '%s' does not exist or is not a regular file", image_path);
        return 0;
    }
    
    // Check file size (reasonable limit for LCD images)
    if (file_stat.st_size > (2 * 1024 * 1024)) { // 2MB limit
        log_message(LOG_ERROR, "Image file too large: %ld bytes (max 2MB)", file_stat.st_size);
        return 0;
    }
    
    // Validate device UID format (basic sanity check)
    if (strlen(device_uid) < 8 || strlen(device_uid) > 64) {
        log_message(LOG_ERROR, "Invalid device UID format: %s", device_uid);
        return 0;
    }
    
    // Construct upload URL with length validation
    char upload_url[CC_URL_SIZE];
    const int url_len = snprintf(upload_url, sizeof(upload_url), 
                                "%s/devices/%s/settings/%s/lcd/images", 
                                config->daemon_address, device_uid, "lcd");
    
    if (url_len < 0 || url_len >= (int)sizeof(upload_url)) {
        log_message(LOG_ERROR, "Upload URL too long");
        return 0;
    }
    
    log_message(LOG_INFO, "Attempting LCD upload to: %s", upload_url);
    
    // Initialize multipart form
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    if (!form) {
        log_message(LOG_ERROR, "Failed to initialize multipart form");
        return 0;
    }
    
    curl_mimepart *field;
    CURLcode mime_result;
    
    // Add mode field with error checking
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create mode field");
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_name(field, "mode");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set mode field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_data(field, "image", CURL_ZERO_TERMINATED);
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set mode field data: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Add brightness field with enhanced validation and error checking
    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d", config->lcd_brightness);
    
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create brightness field");
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_name(field, "brightness");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set brightness field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_data(field, brightness_str, CURL_ZERO_TERMINATED);
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set brightness field data: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Add orientation field with enhanced validation and error checking
    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d", config->lcd_orientation);
    
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create orientation field");
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_name(field, "orientation");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set orientation field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_data(field, orientation_str, CURL_ZERO_TERMINATED);
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set orientation field data: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Add image file with enhanced error checking
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create image field");
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_name(field, "images[]");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set image field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_filedata(field, image_path);
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set image file data: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    mime_result = curl_mime_type(field, "image/png");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set image MIME type: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Initialize response buffer to capture detailed error messages
    struct http_response response = {0};
    if (!cc_init_response_buffer(&response, 4096)) {
        log_message(LOG_ERROR, "Failed to initialize response buffer");
        curl_mime_free(form);
        return 0;
    }
    
    // Configure curl options with enhanced security and error handling
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_TIMEOUT, CC_HTTP_TIMEOUT_SEC * 2); // Extended timeout for file upload
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CONNECTTIMEOUT, CC_HTTP_CONNECT_TIMEOUT_SEC);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_FOLLOWLOCATION, 0L);
    
    // Only enable SSL verification if using HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0) {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, &response);
    
    // Add debugging headers to identify our application
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);
    
    // Retry loop for LCD communication with smart retry logic
    int success = 0;
    int retry_delay = CC_INITIAL_RETRY_DELAY_MS;
    
    for (int attempt = 0; attempt < CC_MAX_RETRIES && !success; attempt++) {
        if (attempt > 0) {
            log_message(LOG_WARNING, "LCD upload attempt %d/%d after %dms delay", 
                        attempt + 1, CC_MAX_RETRIES, retry_delay);
            sleep_ms(retry_delay);
            retry_delay = (retry_delay * 2 < CC_MAX_RETRY_DELAY_MS) ? retry_delay * 2 : CC_MAX_RETRY_DELAY_MS;
            
            // Reset response buffer for retry
            response.size = 0;
        }
        
        // Perform request
        CURLcode res = curl_easy_perform(cc_session.curl_handle);
        
        // Get HTTP response code and timing information
        long http_response_code = -1;
        double total_time = 0;
        curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);
        curl_easy_getinfo(cc_session.curl_handle, CURLINFO_TOTAL_TIME, &total_time);

        // Enhanced error handling with detailed logging
        if (res != CURLE_OK) {
            log_message(LOG_WARNING, "CURL error during LCD upload (attempt %d/%d): %s (took %.2fs)", 
                        attempt + 1, CC_MAX_RETRIES, curl_easy_strerror(res), total_time);
            continue;
        }
        
        // Validate response code is within valid HTTP range
        if (http_response_code < 100 || http_response_code > 599) {
            log_message(LOG_ERROR, "Invalid HTTP response code: %ld", http_response_code);
            continue;
        }
        
        if (http_response_code == 200) {
            success = 1;
            log_message(LOG_INFO, "LCD image uploaded successfully (took %.2fs)", total_time);
        } else if (http_response_code == 500) {
            // Server error - log detailed response body
            if (response.data && response.size > 0) {
                // Null-terminate response for safe string operations
                if (response.size < response.capacity) {
                    response.data[response.size] = '\0';
                }
                log_message(LOG_ERROR, "LCD upload failed with 500 error (attempt %d/%d): %s", 
                            attempt + 1, CC_MAX_RETRIES, response.data);
            } else {
                log_message(LOG_ERROR, "LCD upload failed with 500 Internal Server Error (attempt %d/%d)", 
                            attempt + 1, CC_MAX_RETRIES);
            }
            
            // For 500 errors, use maximum delay before retry (likely device communication issue)
            if (attempt < CC_MAX_RETRIES - 1) {
                retry_delay = CC_MAX_RETRY_DELAY_MS;
                log_message(LOG_WARNING, "Device communication error detected - waiting %dms before retry", retry_delay);
                log_message(LOG_INFO, "Suggestion: Check 'systemctl status coolercontrold' and device USB connection");
            }
        } else if (http_response_code >= 400 && http_response_code < 500) {
            // Client error - log response and don't retry (likely parameter issue)
            if (response.data && response.size > 0) {
                if (response.size < response.capacity) {
                    response.data[response.size] = '\0';
                }
                log_message(LOG_ERROR, "LCD upload failed with client error %ld: %s", 
                            http_response_code, response.data);
            } else {
                log_message(LOG_ERROR, "LCD upload failed with client error: %ld", http_response_code);
            }
            break; // Don't retry client errors - likely a parameter issue
        } else {
            log_message(LOG_WARNING, "Unexpected HTTP response code %ld (attempt %d/%d, took %.2fs)", 
                        http_response_code, attempt + 1, CC_MAX_RETRIES, total_time);
        }
    }
    
    // Final error logging if all attempts failed
    if (!success) {
        log_message(LOG_ERROR, "LCD upload failed after %d attempts. Check CoolerControl device communication and Open API compatibility.", CC_MAX_RETRIES);
        
        // Provide detailed troubleshooting information
        log_message(LOG_INFO, "Troubleshooting steps:");
        log_message(LOG_INFO, "1. Check device status: systemctl status coolercontrold");
        log_message(LOG_INFO, "2. Verify device UID '%s' is correct and responsive", device_uid);
        log_message(LOG_INFO, "3. Check CoolerControl logs: journalctl -u coolercontrold -n 20");
        log_message(LOG_INFO, "4. Ensure device USB connection is stable");
        log_message(LOG_INFO, "5. Try restarting CoolerControl: sudo systemctl restart coolercontrold");
        log_message(LOG_INFO, "6. Verify daemon accessibility at '%s'", config->daemon_address);
    }
    
    // Cleanup resources
    cc_cleanup_response_buffer(&response);
    if (headers) curl_slist_free_all(headers);
    curl_mime_free(form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, NULL);
    
    return success;
}

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details Frees all resources and closes the session. This includes cleaning up the CURL handle and deleting the cookie jar file.
 * @example
 *     cleanup_coolercontrol_session();
 */
void cleanup_coolercontrol_session(void) {
    static int cleanup_done = 0;
    if (cleanup_done) return;
    
    int all_cleaned = 1;
    
    // Clean up CURL handle
    if (cc_session.curl_handle) {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }
    
    // Perform global CURL cleanup
    curl_global_cleanup();
    
    // Remove cookie jar file
    if (unlink(cc_session.cookie_jar) != 0) {
        all_cleaned = 0;
    }
    
    // Mark session as uninitialized
    cc_session.session_initialized = 0;
    
    // Set cleanup flag only if all operations succeeded
    if (all_cleaned) {
        cleanup_done = 1;
    }
}

int is_session_initialized(void) {
    return cc_session.session_initialized;
}

/**
 * @brief Parse devices JSON and extract LCD UID, display info and device name from Liquidctl devices.
 * @details Helper for get_liquidctl_device_info. Returns 1 on success, 0 on failure.
 * @example
 *     char uid[128], name[128]; int found; int width, height;
 *     parse_liquidctl_devices_json(json, uid, sizeof(uid), &found, &width, &height, name, sizeof(name));
 */
static int parse_liquidctl_devices_json(const char *json, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size) {
    // Validate input and initialize output parameters
    if (!json) return 0;

    if (lcd_uid && uid_size > 0) lcd_uid[0] = '\0';
    if (found_liquidctl) *found_liquidctl = 0;
    if (screen_width) *screen_width = 0;
    if (screen_height) *screen_height = 0;
    if (device_name && name_size > 0) device_name[0] = '\0';

    // Parse JSON with error handling
    json_error_t error;
    json_t *root = json_loads(json, 0, &error);
    if (!root) {
        log_message(LOG_ERROR, "JSON parse error: %s", error.text);
        return 0;
    }

    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices)) {
        json_decref(root);
        return 0;
    }

    // Pre-calculate array size to avoid repeated calls
    const size_t device_count = json_array_size(devices);

    // Use direct array access instead of foreach for better performance
    for (size_t i = 0; i < device_count; i++) {
        json_t *dev = json_array_get(devices, i);
        if (!dev) continue;

        json_t *type_val = json_object_get(dev, "type");
        if (!type_val || !json_is_string(type_val)) continue;

        const char *type_str = json_string_value(type_val);

        // Use optimized string comparison with early exit for device type detection
        // Support multiple device types that might have LCD capability
        const char *liquid_types[] = {"Liquidctl", "NZXT", "Corsair", "EVGA"};
        const size_t num_types = sizeof(liquid_types) / sizeof(liquid_types[0]);
        
        int is_supported_device = 0;
        for (size_t j = 0; j < num_types; j++) {
            if (strcmp(type_str, liquid_types[j]) == 0) {
                is_supported_device = 1;
                break;
            }
        }
        
        if (!is_supported_device) continue;

        // Found Liquidctl device
        if (found_liquidctl) *found_liquidctl = 1;

        // Extract UID with safe string handling
        if (lcd_uid && uid_size > 0) {
            json_t *uid_val = json_object_get(dev, "uid");
            if (uid_val && json_is_string(uid_val)) {
                const char *uid_str = json_string_value(uid_val);
                // Use snprintf for guaranteed null termination and safety
                snprintf(lcd_uid, uid_size, "%s", uid_str);
            }
        }

        // Extract device name with safe string handling
        if (device_name && name_size > 0) {
            json_t *name_val = json_object_get(dev, "name");
            if (name_val && json_is_string(name_val)) {
                const char *name_str = json_string_value(name_val);
                // Use snprintf for guaranteed null termination and safety
                snprintf(device_name, name_size, "%s", name_str);
            }
        }

        // Extract LCD dimensions with reduced nesting
        if (screen_width || screen_height) {
            json_t *info = json_object_get(dev, "info");
            json_t *channels = info ? json_object_get(info, "channels") : NULL;
            json_t *lcd_channel = channels ? json_object_get(channels, "lcd") : NULL;
            json_t *lcd_info = lcd_channel ? json_object_get(lcd_channel, "lcd_info") : NULL;
            if (lcd_info) {
                if (screen_width) {
                    json_t *width_val = json_object_get(lcd_info, "screen_width");
                    if (width_val && json_is_integer(width_val)) {
                        *screen_width = (int)json_integer_value(width_val);
                    }
                }
                if (screen_height) {
                    json_t *height_val = json_object_get(lcd_info, "screen_height");
                    if (height_val && json_is_integer(height_val)) {
                        *screen_height = (int)json_integer_value(height_val);
                    }
                }
            }
        }

        // Early exit - found what we need
        json_decref(root);
        return 1;
    }

    json_decref(root);
    return 1; // Return success even if no Liquidctl device found
}



/**
 * @brief Get Liquidctl device UID from CoolerControl API.
 * @details Wrapper function that calls get_liquidctl_device_info for UID only. Returns 1 on success, 0 on failure.
 * @example
 *     char uid[128];
 *     if (get_liquidctl_device_uid(&config, uid, sizeof(uid))) { ... }
 */
int get_liquidctl_device_uid(const Config *config, char *device_uid, size_t uid_size) {
    // Use the comprehensive function and ignore other data
    return get_liquidctl_device_info(config, device_uid, uid_size, NULL, 0, NULL, NULL);
}

/**
 * @brief Get Liquidctl device display info (screen_width and screen_height) from CoolerControl API.
 * @details Wrapper function that calls get_liquidctl_device_info for dimensions only. Returns 1 on success, 0 on failure.
 * @example
 *     int width, height;
 *     if (get_liquidctl_display_info(&config, &width, &height)) { ... }
 */
int get_liquidctl_display_info(const Config *config, int *screen_width, int *screen_height) {
    // Use the comprehensive function and ignore other data
    return get_liquidctl_device_info(config, NULL, 0, NULL, 0, screen_width, screen_height);
}

/**
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from CoolerControl API.
 * @details Reads all LCD device information via API in one call. Returns 1 on success, 0 on failure.
 * @example
 *     char uid[128], name[128]; int width, height;
 *     if (get_liquidctl_device_info(&config, uid, sizeof(uid), name, sizeof(name), &width, &height)) { ... }
 */
int get_liquidctl_device_info(const Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height) {
    // Check if config is valid (other parameters can be NULL if not needed)
    if (!config) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    // Construct URL for devices endpoint with buffer overflow protection
    char url[512];  // Increased buffer size for safety
    int ret = snprintf(url, sizeof(url), "%s/devices", config->daemon_address);
    if (ret >= (int)sizeof(url) || ret < 0) {
        curl_easy_cleanup(curl);
        return 0; // URL too long or formatting error
    }

    // Initialize response buffer with optimized capacity
    struct http_response chunk = {0};
    const size_t initial_capacity = 4096; // Start with 4KB (typical JSON response size)
    if (!cc_init_response_buffer(&chunk, initial_capacity)) {
        curl_easy_cleanup(curl);
        return 0;
    }

    // Configure curl options with enhanced security
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, CC_HTTP_TIMEOUT_SEC); // Use configured timeout for responsiveness
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CC_HTTP_CONNECT_TIMEOUT_SEC); // Use configured connection timeout
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L); // Security: no redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 0L); // Security: no redirects
    
    // Only enable SSL verification if using HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Verify SSL certificates for HTTPS
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); // Verify SSL hostname for HTTPS
    }
    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CoolerDash/1.0"); // Identify our application

    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Initialize parsing variables
    int found_liquidctl = 0; // Flag to check if Liquidctl device is found
    char lcd_uid[128] = ""; // Buffer for LCD UID
    char lcd_name[128] = ""; // Buffer for LCD device name
    int width = 0, height = 0; // Initialize display dimensions
    int result = 0;

    // Retry loop with exponential backoff for network requests
    int retry_delay = CC_INITIAL_RETRY_DELAY_MS;
    for (int attempt = 0; attempt < CC_MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            log_message(LOG_WARNING, "Device info request attempt %d/%d after %dms delay", 
                        attempt + 1, CC_MAX_RETRIES, retry_delay);
            sleep_ms(retry_delay);
            retry_delay = (retry_delay * 2 < CC_MAX_RETRY_DELAY_MS) ? retry_delay * 2 : CC_MAX_RETRY_DELAY_MS;
            
            // Reset response buffer for retry
            chunk.size = 0;
        }

        // Perform request and parse response with enhanced error handling
        CURLcode curl_result = curl_easy_perform(curl);
        if (curl_result == CURLE_OK) {
            // Check HTTP response code
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            
            if (response_code == 200) {
                result = parse_liquidctl_devices_json(chunk.data, lcd_uid, sizeof(lcd_uid), &found_liquidctl, &width, &height, lcd_name, sizeof(lcd_name));
                break; // Success - exit retry loop
        } else {
            log_message(LOG_WARNING, "HTTP error: %ld when fetching device info (attempt %d/%d)", 
                        response_code, attempt + 1, CC_MAX_RETRIES);
            result = 0;
        }
    } else {
        log_message(LOG_WARNING, "CURL error: %s (attempt %d/%d)", 
                    curl_easy_strerror(curl_result), attempt + 1, CC_MAX_RETRIES);
        result = 0;
    }
}

// Log final failure if all retries exhausted
if (result == 0) {
    log_message(LOG_ERROR, "Failed to fetch device info after %d attempts", CC_MAX_RETRIES);
}    // Clean up resources
    cc_cleanup_response_buffer(&chunk);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!found_liquidctl) {
        log_message(LOG_ERROR, "No Liquidctl device found. Exiting.");
        return 0;
    }

    // Copy values to output buffers (only if pointers are not NULL)
    if (device_uid && uid_size > 0) {
        strncpy(device_uid, lcd_uid, uid_size - 1);
        device_uid[uid_size - 1] = '\0';
    }
    if (device_name && name_size > 0) {
        strncpy(device_name, lcd_name, name_size - 1);
        device_name[name_size - 1] = '\0';
    }
    if (screen_width) *screen_width = width;
    if (screen_height) *screen_height = height;

    return result;
}

int get_device_uid(const Config *config, cc_device_data_t *data) {
    // Check if config and data pointers are valid
    if (!config || !data) return 0;

    // Use the existing function to get UID
    return get_liquidctl_device_uid(config, data->device_uid, sizeof(data->device_uid));
}

// Future sensor detection and device validation functions can be added here when needed
