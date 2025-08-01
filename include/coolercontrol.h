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

// Error codes
#define CC_ERROR_INVALID_RESPONSE -1

// Buffer size constants
#define CC_UID_SIZE      128
#define CC_NAME_SIZE     128
#define CC_COOKIE_SIZE   256
#define CC_URL_SIZE      256
#define CC_USERPWD_SIZE  128
#define CC_DEVICE_SECTION_SIZE 4096

// Include necessary headers
#include <stddef.h>
#include <signal.h>

// Include project headers
#include "config.h"

/**
 * @brief Structure to hold sensor data (CPU/GPU temperature).
 * @details Used to aggregate all relevant sensor values from CoolerControl API.
 * @example
 *     cc_sensor_data_t data;
 *     if (monitor_get_sensor_data(&config, &data)) { ... }
 */
typedef struct {
    float temp_1; // temperature 1 in degrees Celsius
    float temp_2; // temperature 2 in degrees Celsius
    char device_uid[CC_UID_SIZE]; ///< Device UID
} cc_sensor_data_t;

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

/*
 * @brief Response buffer for libcurl HTTP operations.
 * @details Used to collect HTTP response data in memory.
 */
typedef struct http_response {
    char *data;
    size_t size;
} http_response;

/*
 * @brief Callback for libcurl to write received data into a buffer.
 * @details This function is used by libcurl to store incoming HTTP response data into a dynamically allocated buffer. It reallocates the buffer as needed and appends the new data chunk. If memory allocation fails, it frees the buffer and returns 0 to signal an error to libcurl.
 * @example
 *     struct http_response resp = {0};
 *     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
 *     curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
 */
size_t write_callback(void *contents, size_t size, size_t nmemb, struct http_response *response);

#endif // COOLERCONTROL_H
