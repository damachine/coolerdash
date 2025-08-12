/**
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief CoolerControl API interface for LCD device communication and sensor data.
 * @details Provides functions for initializing, authenticating, communicating with CoolerControl LCD devices, and reading sensor values (CPU/GPU) via the REST API.
 */

// Include necessary headers
#ifndef COOLERCONTROL_H
#define COOLERCONTROL_H

// Include necessary headers
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Project headers
#include "config.h"

// Basic constants
#define CC_COOKIE_SIZE 512                              
#define CC_NAME_SIZE 128                                
#define CC_UID_SIZE 128                                 
#define CC_URL_SIZE 512                                 
#define CC_USERPWD_SIZE 128

// Forward declarations to reduce compilation dependencies
struct Config;
struct curl_slist;

/**
 * @brief Response buffer for libcurl HTTP operations.
 * @details Structure to hold HTTP response data with dynamic memory management for efficient data collection.
 */
typedef struct http_response {
    char *data;
    size_t size;
    size_t capacity;
} http_response;

/**
 * @brief Secure string copy with bounds checking.
 * @details Performs safe string copying with buffer overflow protection and null termination guarantee.
 */
static inline int cc_safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (!dest || !src || dest_size == 0) {
        return 0;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        return 0;
    }
    
    memcpy(dest, src, src_len);
    dest[src_len] = '\0';
    return 1;
}

/**
 * @brief Secure memory allocation with initialization.
 * @details Allocates memory using calloc to ensure zero-initialization and prevent uninitialized data access.
 */
static inline void* cc_secure_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    return calloc(1, size);
}

/**
 * @brief Initialize HTTP response buffer with specified capacity.
 * @details Allocates memory for HTTP response data with proper initialization.
 */
static inline int cc_init_response_buffer(struct http_response *response, size_t initial_capacity) {
    if (!response || initial_capacity == 0) {
        return 0;
    }
    
    response->data = malloc(initial_capacity);
    if (!response->data) {
        response->size = 0;
        response->capacity = 0;
        return 0;
    }
    
    response->size = 0;
    response->capacity = initial_capacity;
    response->data[0] = '\0';
    return 1;
}

/**
 * @brief Validate HTTP response buffer integrity.
 * @details Checks if response buffer is in valid state for operations.
 */
static inline int cc_validate_response_buffer(const struct http_response *response) {
    return (response && 
            response->data && 
            response->size <= response->capacity);
}

/**
 * @brief Cleanup HTTP response buffer and free memory.
 * @details Properly frees allocated memory and resets buffer state.
 */
static inline void cc_cleanup_response_buffer(struct http_response *response) {
    if (response) {
        if (response->data) {
            free(response->data);
            response->data = NULL;
        }
        response->size = 0;
        response->capacity = 0;
    }
}

/**
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is used by libcurl to store incoming HTTP response data into a dynamically allocated buffer. It reallocates the buffer as needed and appends the new data chunk. If memory allocation fails, it frees the buffer and returns 0 to signal an error to libcurl.
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response);

/**
 * @brief Initializes a CoolerControl session and authenticates with the daemon using configuration.
 * @details Must be called before any other CoolerControl API function. Sets up CURL session and performs authentication.
 */
int init_coolercontrol_session(const Config *config);

/**
 * @brief Returns whether the session is initialized.
 * @details Checks if the session is ready for communication with the CoolerControl daemon.
 */
int is_session_initialized(void);

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details Frees all resources and closes the session, including CURL cleanup and cookie file removal.
 */
void cleanup_coolercontrol_session(void);

/**
 * @brief Get Liquidctl device UID from CoolerControl API.
 * @details Reads the LCD device UID via API. Returns 1 on success, 0 on failure.
 */
int get_liquidctl_device_uid(const Config *config, char *device_uid, size_t uid_size);

/**
 * @brief Get Liquidctl device display info (screen_width and screen_height) from CoolerControl API.
 * @details Reads the LCD display dimensions via API. Returns 1 on success, 0 on failure.
 */
int get_liquidctl_display_info(const Config *config, int *screen_width, int *screen_height);

/**
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from cache.
 * @details Reads all LCD device information from cache (no API call). Returns 1 on success, 0 on failure.
 * Optimized for performance with cached data and no network overhead.
 * @example
 *     char uid[128], name[128]; int width, height;
 *     if (get_liquidctl_device_info(&config, uid, sizeof(uid), name, sizeof(name), &width, &height)) { ... }
 */
int get_liquidctl_device_info(const Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height);

/**
 * @brief Initialize device information cache.
 * @details Fetches and caches device information once at startup for better performance.
 */
int init_device_cache(const Config *config);

/**
 * @brief Sends an image directly to the LCD of the CoolerControl device.
 * @details Uploads an image to the LCD display using a multipart HTTP PUT request with brightness and orientation settings.
 */
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid);

/**
 * @brief Initialize monitor subsystem (CPU/GPU sensors).
 * @details Sets up all available temperature sensors (CPU, GPU, etc.) for data collection.
 */
int monitor_init(const Config *config);

#endif // COOLERCONTROL_H