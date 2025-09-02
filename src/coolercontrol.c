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
 * @brief CoolerControl API interface for LCD device communication and sensor data.
 * @details Provides functions for initializing, authenticating, communicating with CoolerControl LCD devices, and reading sensor values (CPU/GPU) via the REST API.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L

// Include necessary headers
// cppcheck-suppress missingInclude
#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Include project headers
#include "../include/config.h"
#include "../include/coolercontrol.h"

/**
 * @brief Structure to hold CoolerControl session state.
 * @details Contains the CURL handle, cookie jar path, and session initialization status.
 */
typedef struct {
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
} CoolerControlSession;

/**
 * @brief Global CoolerControl session state.
 * @details Holds the state of the CoolerControl session, including the CURL handle and session initialization status.
*/
static CoolerControlSession cc_session = {
    .curl_handle = NULL,
    .cookie_jar = {0},
    .session_initialized = 0
};

/**
 * @brief Static cache for device information (never changes during runtime).
 * @details Holds the device UID, name, and display dimensions once fetched from the API. Used to avoid redundant API calls and improve performance.
 */
static struct {
    int initialized;
    char device_uid[CC_UID_SIZE];
    char device_name[CC_NAME_SIZE];
    int screen_width;
    int screen_height;
} device_cache = {0}; // Strings always written via snprintf/cc_safe_strcpy (bounded)

/**
 * @brief Initialize device cache by fetching device information once.
 * @details Populates the static cache with device UID, name, and display dimensions.
 */
static int initialize_device_cache(const Config *config);

/**
 * @brief Parse devices JSON and extract LCD UID, display info and device name from Liquidctl devices.
 * @details This function parses the JSON response from the CoolerControl API to find Liquidctl devices, extracting their UID, display dimensions, and device name.
 */
static int parse_liquidctl_data(const char *json, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size);

/**
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is called by libcurl to write the response data into a dynamically allocated buffer with automatic reallocation when needed.
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response) {
    // Validate input
    const size_t realsize = size * nmemb;
    const size_t new_size = response->size + realsize + 1;

    // Reallocate if needed
    if (new_size > response->capacity) {
        // Grow capacity
        const size_t new_capacity = (new_size > response->capacity * 3 / 2) ? new_size : response->capacity * 3 / 2;

        // Reallocate
        char *ptr = realloc(response->data, new_capacity);
        if (!ptr) {
            log_message(LOG_ERROR, "Memory allocation failed for response data: %zu bytes", new_capacity);
            free(response->data);
            response->data = NULL;
            response->size = 0;
            response->capacity = 0;
            return 0;
        }

        // Clear new memory
        response->data = ptr;
        response->capacity = new_capacity;
    }

    // Copy data with bounds safety (capacity ausreichend geprüft)
    if (realsize > 0) {
        memmove(response->data + response->size, contents, realsize);
        response->size += realsize;
        response->data[response->size] = '\0';
    }

    // Return size
    return realsize;
}

/**
 * @brief Parse devices JSON and extract LCD UID, display info and device name from Liquidctl devices.
 * @details This function parses the JSON response from the CoolerControl API to find Liquidctl devices, extracting their UID, display dimensions, and device name.
 */
static int parse_liquidctl_data(const char *json, char *lcd_uid, size_t uid_size, int *found_liquidctl, int *screen_width, int *screen_height, char *device_name, size_t name_size) {
    // Validate input
    if (!json) return 0;

    // Initialize output variables
    if (lcd_uid && uid_size > 0) lcd_uid[0] = '\0';
    if (found_liquidctl) *found_liquidctl = 0;
    if (screen_width) *screen_width = 0;
    if (screen_height) *screen_height = 0;
    if (device_name && name_size > 0) device_name[0] = '\0';

    // Parse JSON
    json_error_t error;
    json_t *root = json_loads(json, 0, &error);
    if (!root) {
        log_message(LOG_ERROR, "JSON parse error: %s", error.text);
        return 0;
    }

    // Get devices array
    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices)) {
        json_decref(root);
        return 0;
    }

    // Get device count
    const size_t device_count = json_array_size(devices);

    // Check each device
    for (size_t i = 0; i < device_count; i++) {
        // Get device entry
        json_t *dev = json_array_get(devices, i);
        if (!dev) continue;

        // Get device type
        json_t *type_val = json_object_get(dev, "type");
        if (!type_val || !json_is_string(type_val)) continue;

        // Extract device type string
        const char *type_str = json_string_value(type_val);

        // Check device type
        const char *liquid_types[] = {"Liquidctl"};
        const size_t num_types = sizeof(liquid_types) / sizeof(liquid_types[0]);
        
        // Check if device type is supported
        int is_supported_device = 0;
        for (size_t j = 0; j < num_types; j++) {
            if (strcmp(type_str, liquid_types[j]) == 0) {
                is_supported_device = 1;
                break;
            }
        }
        
        // Skip if not supported
        if (!is_supported_device) continue;

        // Found Liquidctl device
        if (found_liquidctl) *found_liquidctl = 1;

        // Extract UID
        if (lcd_uid && uid_size > 0) {
            json_t *uid_val = json_object_get(dev, "uid");
            if (uid_val && json_is_string(uid_val)) {
                const char *uid_str = json_string_value(uid_val);
                snprintf(lcd_uid, uid_size, "%s", uid_str);
            }
        }

        // Extract device name
        if (device_name && name_size > 0) {
            json_t *name_val = json_object_get(dev, "name");
            if (name_val && json_is_string(name_val)) {
                const char *name_str = json_string_value(name_val);
                snprintf(device_name, name_size, "%s", name_str);
            }
        }

        // Extract LCD dimensions
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

        // Found device, exit early
        json_decref(root);
        return 1;
    }

    // No Liquidctl device found
    json_decref(root);
    return 1;
}

/**
 * @brief Initialize device cache by fetching device information once.
 * @details Populates the static cache with device UID, name, and display dimensions.
 */
static int initialize_device_cache(const Config *config) {
    // Check if already initialized
    if (device_cache.initialized) {
        return 1; // Already initialized
    }

    // Validate input
    if (!config) return 0;

    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    // Construct URL for devices endpoint
    char url[512]; // Increased buffer size to prevent truncation
    int ret = snprintf(url, sizeof(url), "%s/devices", config->daemon_address);
    if (ret >= (int)sizeof(url) || ret < 0) {
        return 0; // URL too long or formatting error
    }

    // Initialize response buffer with optimized capacity
    struct http_response chunk = {0};
    const size_t initial_capacity = 4096; // Start with 4KB (typical JSON response size)
    chunk.data = malloc(initial_capacity);
    if (!chunk.data) {
        curl_easy_cleanup(curl);
        return 0;
    }
    chunk.size = 0;
    chunk.capacity = initial_capacity;

    // Configure curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Initialize parsing variables
    int found_liquidctl = 0;
    int result = 0;

    // Perform request and parse response
    if (curl_easy_perform(curl) == CURLE_OK) {
    result = parse_liquidctl_data(chunk.data,
                                            device_cache.device_uid, sizeof(device_cache.device_uid),
                                            &found_liquidctl,
                                            &device_cache.screen_width, &device_cache.screen_height,
                                            device_cache.device_name, sizeof(device_cache.device_name));
        // Initialize cache if Liquidctl device found
        if (result && found_liquidctl) {
            device_cache.initialized = 1;
            log_message(LOG_STATUS, "Device cache initialized: %s (%dx%d pixel)", 
                       device_cache.device_name, device_cache.screen_width, device_cache.screen_height);
        }
    }

    // Clean up resources
    free(chunk.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // Return success status
    return device_cache.initialized;
}

/**
 * @brief Initializes the CoolerControl session (CURL setup and login).
 * @details This function initializes the CURL library, sets up the session cookie jar, constructs the login URL and credentials, and performs a login to the CoolerControl API. No handshake or session authentication is performed beyond basic login.
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
                                  "/tmp/coolerdash_cookie_%d.txt", getpid());
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

    // Configure cURL options
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, login_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, NULL);
    
    // Set HTTP headers for login request
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);
    
    // Enable SSL verification for HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0) {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    // Perform login request
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    // Clear sensitive data
    memset(userpwd, 0, sizeof(userpwd));

    // Cleanup headers
    if (headers) curl_slist_free_all(headers);

    // Check if login was successful
    if (res == CURLE_OK && (response_code == 200 || response_code == 204)) {
        cc_session.session_initialized = 1;
        return 1;
    } else {
        log_message(LOG_ERROR, "Login failed: CURL code %d, HTTP code %ld", res, response_code);
    }

    // Login failed
    return 0;
}

/**
 * @brief Checks if the CoolerControl session is initialized.
 * @details This function returns whether the session has been successfully initialized and is ready for use.
 */
int is_session_initialized(void) {
    return cc_session.session_initialized;
}

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details This function performs cleanup of the CURL handle, removes the cookie jar file, and marks the session as uninitialized. It ensures that all resources are properly released and no memory leaks occur.
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

/**
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from CoolerControl API.
 * @details This function retrieves the UID, name, and screen dimensions of the Liquidctl device by calling the main device info function.
 */
int get_liquidctl_data(const Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height) {
    // Initialize cache if not already done
    if (!initialize_device_cache(config)) {
        return 0;
    }

    // Copy values from cache to output buffers with safe string handling
    if (device_uid && uid_size > 0) {
        cc_safe_strcpy(device_uid, uid_size, device_cache.device_uid);
    }
    if (device_name && name_size > 0) {
        cc_safe_strcpy(device_name, name_size, device_cache.device_name);
    }
    if (screen_width) *screen_width = device_cache.screen_width;
    if (screen_height) *screen_height = device_cache.screen_height;

    return 1;
}

/**
 * @brief Initialize device cache - public interface.
 * @details Public function to initialize the device cache for external callers.
 */
int init_device_cache(const Config *config) {
    return initialize_device_cache(config);
}

/**
 * @brief Sends an image to the LCD display.
 * @details This function uploads an image to the LCD display using a multipart HTTP PUT request. It requires the session to be initialized and the device UID to be available.
 */
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid) {
    // Basic validation only
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized) {
        log_message(LOG_ERROR, "Invalid parameters or session not initialized");
        return 0;
    }
    
    // Build upload URL
    char upload_url[CC_URL_SIZE];
    snprintf(upload_url, sizeof(upload_url), "%s/devices/%s/settings/lcd/lcd/images", 
             config->daemon_address, device_uid);
    
    // Initialize multipart form
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    if (!form) {
        log_message(LOG_ERROR, "Failed to initialize multipart form");
        return 0;
    }
    
    // Initialize form fields
    curl_mimepart *field;
    CURLcode mime_result;
    
    // Add mode field with error checking
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create mode field");
        curl_mime_free(form);
        return 0;
    }
    
    // Set mode field name
    mime_result = curl_mime_name(field, "mode");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set mode field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Set mode field data
    mime_result = curl_mime_data(field, "image", CURL_ZERO_TERMINATED);
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set mode field data: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Add brightness field
    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d", config->lcd_brightness);
    
    // Add brightness field
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create brightness field");
        curl_mime_free(form);
        return 0;
    }
    
    // Set brightness field name
    mime_result = curl_mime_name(field, "brightness");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set brightness field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Set brightness field data
    mime_result = curl_mime_data(field, brightness_str, CURL_ZERO_TERMINATED);
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set brightness field data: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Add orientation field
    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d", config->lcd_orientation);
    
    // Add orientation field
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create orientation field");
        curl_mime_free(form);
        return 0;
    }
    
    // Set orientation field name
    mime_result = curl_mime_name(field, "orientation");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set orientation field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Set orientation field data
    mime_result = curl_mime_data(field, orientation_str, CURL_ZERO_TERMINATED);
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set orientation field data: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Add image file
    field = curl_mime_addpart(form);
    if (!field) {
        log_message(LOG_ERROR, "Failed to create image field");
        curl_mime_free(form);
        return 0;
    }
    
    // Set image field name
    mime_result = curl_mime_name(field, "images[]");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set image field name: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Set image file data
    mime_result = curl_mime_filedata(field, image_path);
    if (mime_result != CURLE_OK) {
        // Only log error if the file actually exists
        struct stat file_stat;
        if (stat(image_path, &file_stat) == 0) {
            log_message(LOG_ERROR, "Failed to set image file data: %s", curl_easy_strerror(mime_result));
        }
        curl_mime_free(form);
        return 0;
    }
    
    // Set image MIME type
    mime_result = curl_mime_type(field, "image/png");
    if (mime_result != CURLE_OK) {
        log_message(LOG_ERROR, "Failed to set image MIME type: %s", curl_easy_strerror(mime_result));
        curl_mime_free(form);
        return 0;
    }
    
    // Initialize response buffer
    struct http_response response = {0};
    if (!cc_init_response_buffer(&response, 4096)) {
        log_message(LOG_ERROR, "Failed to initialize response buffer");
        curl_mime_free(form);
        return 0;
    }
    
    // Configure curl options
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    
    // Enable SSL verification for HTTPS
    if (strncmp(config->daemon_address, "https://", 8) == 0) {
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(cc_session.curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    
    // Set write callback
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_WRITEDATA, &response);
    
    // Add headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: CoolerDash/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPHEADER, headers);
    
    // Perform request
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    
    // Get response info
    long http_response_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);

    // Check response
    int success = 0;
    if (res == CURLE_OK && http_response_code == 200) {
        success = 1;
    } else {
        log_message(LOG_ERROR, "LCD upload failed: CURL code %d, HTTP code %ld", res, http_response_code);
        if (response.data && response.size > 0) {
            response.data[response.size] = '\0';
            log_message(LOG_ERROR, "Server response: %s", response.data);
        }
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
    
    // Return success
    return success;
}
