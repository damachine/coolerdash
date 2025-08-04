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

// Function prototypes
#ifndef COOLERCONTROL_H
#define COOLERCONTROL_H

// Basic constants
#define CC_COOKIE_SIZE 512                              
#define CC_NAME_SIZE 128                                
#define CC_UID_SIZE 128                                 
#define CC_URL_SIZE 512                                 
#define CC_USERPWD_SIZE 128

// Include necessary headers with minimal dependencies
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Include project headers
#include "config.h"

// Forward declarations to reduce compilation dependencies
struct Config;
struct curl_slist;

/**
 * @brief Structure to hold CoolerControl device information.
 * @details Contains device UID, name, and other relevant data for LCD device communication.
 */
typedef struct {
    char device_uid[CC_UID_SIZE];
} cc_device_data_t;

/**
 * @brief Initializes a CoolerControl session and authenticates with the daemon using configuration.
 * @details Must be called before any other CoolerControl API function. Sets up CURL session and performs authentication.
 */
int init_coolercontrol_session(const Config *config);

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details Frees all resources and closes the session, including CURL cleanup and cookie file removal.
 */
void cleanup_coolercontrol_session(void);

/**
 * @brief Returns whether the session is initialized.
 * @details Checks if the session is ready for communication with the CoolerControl daemon.
 */
int is_session_initialized(void);

/**
 * @brief Sends an image directly to the LCD of the CoolerControl device.
 * @details Uploads an image to the LCD display using a multipart HTTP PUT request with brightness and orientation settings.
 */
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid);

/**
 * @brief Signal handler for clean daemon termination with shutdown image.
 * @details Sends a shutdown image to the LCD (if not already sent), removes the PID file, and sets the running flag to 0 for clean termination.
 */
void cleanup_and_exit(int sig, const Config *config, volatile sig_atomic_t *shutdown_sent_ptr, volatile sig_atomic_t *running_ptr);

/**
 * @brief Initialize monitor subsystem (CPU/GPU sensors).
 * @details Sets up all available temperature sensors (CPU, GPU, etc.) for data collection.
 */
int monitor_init(const Config *config);

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
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from CoolerControl API.
 * @details Reads all LCD device information via API in one call. Optimized for performance with minimal API calls and efficient JSON parsing. Enhanced with input validation and buffer overflow protection.
 */
int get_liquidctl_device_info(const Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height);

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
 * @brief Get device UID from CoolerControl API.
 * @details Reads the LCD device UID via API. Returns 1 on success, 0 on failure.
 */
int get_device_uid(const Config *config, cc_device_data_t *data);

/**
 * @brief Response buffer for libcurl HTTP operations.
 * @details Structure to hold HTTP response data with dynamic memory management for efficient data collection.
 */
typedef struct http_response {
    char *data;
    size_t size;
    size_t capacity;
} http_response;

// Simple helper functions
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

static inline int cc_validate_response_buffer(const struct http_response *response) {
    return (response && 
            response->data && 
            response->size <= response->capacity);
}

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

#endif // COOLERCONTROL_H
