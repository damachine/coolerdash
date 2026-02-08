/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Session management, authentication, LCD image upload.
 * @details HTTP client for CoolerControl REST API.
 */

#ifndef CC_MAIN_H
#define CC_MAIN_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
#include <stdint.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"

// Basic constants
#define CC_COOKIE_SIZE 512
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
 * @details Structure to hold HTTP response data with dynamic memory management
 * for effiziente Datensammlung.
 */
typedef struct http_response
{
    char *data;
    size_t size;
    size_t capacity;
} http_response;

/**
 * @brief Initialize HTTP response buffer with specified capacity.
 * @details Allocates memory for HTTP response data with proper initialization.
 */
int cc_init_response_buffer(http_response *response, size_t initial_capacity);

/**
 * @brief Cleanup HTTP response buffer and free memory.
 * @details Properly frees allocated memory and resets buffer state.
 */
void cc_cleanup_response_buffer(http_response *response);

/**
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is used by libcurl to store incoming HTTP response
 * data into a dynamically allocated buffer.
 */
size_t write_callback(const void *contents, size_t size, size_t nmemb,
                      http_response *response);

/**
 * @brief Initializes a CoolerControl session and authenticates with the daemon
 * using configuration.
 * @details Must be called before any other CoolerControl API function. Sets up
 * CURL session and performs authentication.
 */
int init_coolercontrol_session(const struct Config *config);

/**
 * @brief Returns whether the session is initialized.
 * @details Checks if the session is ready for communication with the
 * CoolerControl daemon.
 */
int is_session_initialized(void);

/**
 * @brief Cleans up and terminates the CoolerControl session.
 * @details Frees all resources and closes the session, including CURL cleanup
 * and cookie file removal.
 */
void cleanup_coolercontrol_session(void);

/**
 * @brief Sends an image directly to the LCD of the CoolerControl device.
 * @details Uploads an image to the LCD display using a multipart HTTP PUT
 * request with brightness and orientation settings.
 */
int send_image_to_lcd(const struct Config *config, const char *image_path,
                      const char *device_uid);

/**
 * @brief Blocking variant of `send_image_to_lcd` with timeout and retries.
 * @details Used for shutdown path to ensure the image upload completes
 * synchronously. Sets temporary CURLOPT_TIMEOUT/CURLOPT_CONNECTTIMEOUT for
 * the duration of the operation and restores defaults afterwards.
 */
int send_image_to_lcd_blocking(const struct Config *config,
                               const char *image_path,
                               const char *device_uid,
                               int timeout_seconds,
                               int retries);

#endif // CC_MAIN_H
