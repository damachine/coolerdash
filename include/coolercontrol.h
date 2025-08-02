/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief CoolerControl API interface for LCD device communication and sensor data.
 * @details Provides functions for initializing, authenticating, communicating with CoolerControl LCD devices, and reading sensor values (CPU/GPU) via the REST API.
 * @example
 *     See function documentation for usage examples.
 */

// Function prototypes
#ifndef COOLERCONTROL_H
#define COOLERCONTROL_H

// Include necessary headers with minimal dependencies
#include <stddef.h>
#include <stdint.h>
#include <string.h>  // For memset in inline functions
#include <stdlib.h>  // For malloc in inline functions
#include <signal.h>  // For sig_atomic_t

// Forward declarations to reduce compilation dependencies
struct Config;
struct curl_slist;

// Error codes
#define CC_ERROR_INVALID_RESPONSE -1

// Constants optimized for cache alignment, security and performance
#define CC_COOKIE_SIZE   256
#define CC_DEVICE_SECTION_SIZE 4096
#define CC_HTTP_CONNECT_TIMEOUT_SEC 5L
#define CC_HTTP_TIMEOUT_SEC 10L
#define CC_MAX_RESPONSE_SIZE (10 * 1024 * 1024)
#define CC_MAX_RETRIES 3
#define CC_NAME_SIZE     128
#define CC_UID_SIZE      128
#define CC_URL_SIZE      512
#define CC_USERPWD_SIZE  128

// Include project headers
#include "config.h"

/**
 * @brief Structure to hold CoolerControl device information.
 * @details Used to store CoolerControl-specific device data like UID. Optimized for cache alignment and minimal memory footprint.
 * @example
 *     cc_device_data_t device_data;
 *     if (get_liquidctl_device_uid(&config, device_data.device_uid, sizeof(device_data.device_uid))) { ... }
 */
typedef struct __attribute__((aligned(16))) {  // 16-byte alignment for better cache performance
    char device_uid[CC_UID_SIZE];     // Device UID (128 bytes)
    // Total: 128 bytes (cache-friendly size, aligned to 16-byte boundary)
} cc_device_data_t;

/**
 * @brief Initializes a CoolerControl session and authenticates with the daemon using configuration.
 * @details Must be called before any other CoolerControl API function.
 * @example
 *     if (!init_coolercontrol_session(&config)) {
 *         // handle error
 *     }
 */
int init_coolercontrol_session(const Config *config);

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details Frees all resources and closes the session.
 * @example
 *     cleanup_coolercontrol_session();
 */
void cleanup_coolercontrol_session(void);

/**
 * @brief Returns whether the session is initialized.
 * @details Checks if the session is ready for communication.
 * @example
 *     if (is_session_initialized()) {
 *         // session is ready
 *     }
 */
int is_session_initialized(void);

/**
 * @brief Sends an image directly to the LCD of the CoolerControl device.
 * @details Uploads an image to the LCD display using a multipart HTTP PUT request.
 * @example
 *     send_image_to_lcd(&config, "/opt/coolerdash/images/coolerdash.png", uid);
 */
int send_image_to_lcd(const Config *config, const char* image_path, const char* device_uid);

/**
 * @brief Signal handler for clean daemon termination with shutdown image.
 * @details Sends a shutdown image to the LCD (if not already sent), removes the PID file, and sets the running flag to 0 for clean termination.
 * @example
 *     signal(SIGTERM, cleanup_and_exit);
 */
void cleanup_and_exit(int sig, const Config *config, volatile sig_atomic_t *shutdown_sent_ptr, volatile sig_atomic_t *running_ptr);

/**
 * @brief Initialize monitor subsystem (CPU/GPU sensors).
 * @details Sets up all available temperature sensors (CPU, GPU, etc.).
 * @example
 *     if (monitor_init(&config)) { ... }
 */
int monitor_init(const Config *config);

/**
 * @brief Get Liquidctl device UID from CoolerControl API.
 * @details Reads the LCD device UID via API. Returns 1 on success, 0 on failure.
 * @example
 *     char uid[128];
 *     if (get_liquidctl_device_uid(&config, uid, sizeof(uid))) { ... }
 */
int get_liquidctl_device_uid(const Config *config, char *device_uid, size_t uid_size);

/**
 * @brief Get Liquidctl device display info (screen_width and screen_height) from CoolerControl API.
 * @details Reads the LCD display dimensions via API. Returns 1 on success, 0 on failure.
 * @example
 *     int width, height;
 *     if (get_liquidctl_display_info(&config, &width, &height)) { ... }
 */
int get_liquidctl_display_info(const Config *config, int *screen_width, int *screen_height);

/**
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from CoolerControl API.
 * @details Reads all LCD device information via API in one call. Optimized for performance with minimal API calls and efficient JSON parsing. Enhanced with input validation and buffer overflow protection.
 * @example
 *     char uid[CC_UID_SIZE], name[CC_NAME_SIZE]; int width, height;
 *     if (get_liquidctl_device_info(&config, uid, sizeof(uid), name, sizeof(name), &width, &height)) { ... }
 */
int get_liquidctl_device_info(const Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height);

/**
 * @brief Secure string copy with bounds checking.
 * @details Safe alternative to strcpy/strncpy with guaranteed null termination.
 * @example
 *     char buffer[64];
 *     if (cc_safe_strcpy(buffer, sizeof(buffer), source_string)) { ... }
 */
static inline int cc_safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (!dest || !src || dest_size == 0) {
        return 0;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        return 0; // Source too large
    }
    
    memcpy(dest, src, src_len);
    dest[src_len] = '\0';
    return 1;
}

/**
 * @brief Secure memory allocation with initialization.
 * @details Allocates memory and initializes it to zero for security.
 * @example
 *     char *buffer = cc_secure_malloc(256);
 */
static inline void* cc_secure_malloc(size_t size) {
    if (size == 0 || size > CC_MAX_RESPONSE_SIZE) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}



/**
 * @brief Get device UID from CoolerControl API.
 * @details Reads the LCD device UID via API. Returns 1 on success, 0 on failure.
 * @example
 *     cc_device_data_t device_data;
 *     if (get_device_uid(&config, &device_data)) { ... }
 */
int get_device_uid(const Config *config, cc_device_data_t *data);

/**
 * @brief Response buffer for libcurl HTTP operations.
 * @details Used to collect HTTP response data in memory. Optimized for performance with capacity tracking to reduce reallocations.
 * Cache-aligned for better memory performance.
 */
typedef struct __attribute__((aligned(8))) http_response {
    char *data;          // Response data buffer
    size_t size;         // Current data size
    size_t capacity;     // Buffer capacity (reduces realloc calls)
    uint32_t magic;      // Magic number for corruption detection (0xDEADBEEF)
} http_response;

// Magic number for buffer integrity checking
#define CC_RESPONSE_MAGIC 0xDEADBEEF

// Inline helper functions for performance-critical operations with safety checks
static inline int cc_init_response_buffer(struct http_response *response, size_t initial_capacity) {
    if (!response || initial_capacity == 0 || initial_capacity > CC_MAX_RESPONSE_SIZE) {
        return 0; // Invalid parameters
    }
    
    response->data = malloc(initial_capacity);
    if (!response->data) {
        response->size = 0;
        response->capacity = 0;
        response->magic = 0;
        return 0; // Allocation failed
    }
    
    response->size = 0;
    response->capacity = initial_capacity;
    response->magic = CC_RESPONSE_MAGIC;
    response->data[0] = '\0'; // Initialize with null terminator
    return 1; // Success
}

static inline int cc_validate_response_buffer(const struct http_response *response) {
    return (response && 
            response->magic == CC_RESPONSE_MAGIC && 
            response->data && 
            response->size <= response->capacity &&
            response->capacity <= CC_MAX_RESPONSE_SIZE);
}

static inline void cc_cleanup_response_buffer(struct http_response *response) {
    if (response) {
        if (response->data) {
            // Security: Clear sensitive data before freeing
            memset(response->data, 0, response->capacity);
            free(response->data);
            response->data = NULL;
        }
        response->size = 0;
        response->capacity = 0;
        response->magic = 0; // Clear magic number
    }
}

/**
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is used by libcurl to store incoming HTTP response data into a dynamically allocated buffer. It reallocates the buffer as needed and appends the new data chunk. If memory allocation fails, it frees the buffer and returns 0 to signal an error to libcurl.
 * @example
 *     struct http_response resp = {0};
 *     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
 *     curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response);

#endif // COOLERCONTROL_H
