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

// Include necessary headers
#ifndef COOLERCONTROL_H
#define COOLERCONTROL_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <stdint.h>
#include <jansson.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "config.h"

// Basic constants
#define CC_COOKIE_SIZE 512
#define CC_NAME_SIZE 128
#define CC_UID_SIZE 128
#define CC_URL_SIZE 512
#define CC_USERPWD_SIZE 128

// Maximum safe allocation size to prevent overflow
#define CC_MAX_SAFE_ALLOC_SIZE (SIZE_MAX / 2)

// Forward declarations to reduce compilation dependencies
struct Config;
struct curl_slist;

/**
 * @brief Response buffer for libcurl HTTP operations.
 * @details Structure to hold HTTP response data with dynamic memory management for effiziente Datensammlung.
 */
typedef struct http_response
{
    char *data;
    size_t size;
    size_t capacity;
} http_response;

/**
 * @brief Secure string copy with bounds checking.
 * @details Performs safe string copying with buffer overflow protection and null termination guarantee.
 */
int cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src);

/**
 * @brief Secure memory allocation with initialization.
 * @details Allocates memory using calloc to ensure zero-initialization and prevent uninitialized data access.
 */
void *cc_secure_malloc(size_t size);

/**
 * @brief Initialize HTTP response buffer with specified capacity.
 * @details Allocates memory for HTTP response data with proper initialization.
 */
int cc_init_response_buffer(http_response *response, size_t initial_capacity);

/**
 * @brief Validate HTTP response buffer integrity.
 * @details Checks if response buffer is in valid state for operations.
 */
int cc_validate_response_buffer(const http_response *response);

/**
 * @brief Cleanup HTTP response buffer and free memory.
 * @details Properly frees allocated memory and resets buffer state.
 */
void cc_cleanup_response_buffer(http_response *response);

/**
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is used by libcurl to store incoming HTTP response data into a dynamically allocated buffer. It reallocates the buffer as needed and appends the new data chunk. If memory allocation fails, it frees the buffer and returns 0 to signal an error to libcurl.
 */
// size_t write_callback(void *contents, size_t size, size_t nmemb, http_response *response); (bereits oben deklariert)

/**
 * @brief Initializes a CoolerControl session and authenticates with the daemon using configuration.
 * @details Must be called before any other CoolerControl API function. Sets up CURL session and performs authentication.
 */
int init_coolercontrol_session(const struct Config *config);

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
 * @brief Get complete Liquidctl device information (UID, name, screen dimensions) from cache.
 * @details Reads all LCD device information from cache (no API call).
 */
int get_liquidctl_data(const struct Config *config, char *device_uid, size_t uid_size, char *device_name, size_t name_size, int *screen_width, int *screen_height);

/**
 * @brief Initialize device information cache.
 * @details Fetches and caches device information once at startup for better performance.
 */
int init_device_cache(const struct Config *config);

/**
 * @brief Sends an image directly to the LCD of the CoolerControl device.
 * @details Uploads an image to the LCD display using a multipart HTTP PUT request with brightness and orientation settings.
 */
int send_image_to_lcd(const struct Config *config, const char *image_path, const char *device_uid);

/**
 * @brief Extract device type from JSON device object.
 * @details Common helper function to extract device type string from JSON device object.
 */
const char *extract_device_type_from_json(const json_t *dev);
size_t write_callback(const void *contents, size_t size, size_t nmemb, http_response *response);

#endif // COOLERCONTROL_H