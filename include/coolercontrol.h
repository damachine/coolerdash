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

#ifndef COOLERCONTROL_H
#define COOLERCONTROL_H

#include "config.h"
#include <stddef.h>
#include <signal.h>

#define CC_UID_SIZE      128
#define CC_NAME_SIZE     128
#define CC_COOKIE_SIZE   256
#define CC_URL_SIZE      256
#define CC_USERPWD_SIZE  128
#define CC_DEVICE_SECTION_SIZE 4096

/**
 * @brief Structure to hold sensor data (CPU/GPU temperature).
 * @details Used to aggregate all relevant sensor values from CoolerControl API.
 * @example
 *     cc_sensor_data_t data;
 *     if (cc_get_sensor_data(&config, &data)) { ... }
 */
typedef struct {
    float temp_1; ///< formerly cpu_temp, temperature in degrees Celsius
    float temp_2; ///< formerly gpu_temp, temperature in degrees Celsius
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
 * @brief Reads current sensor data (CPU/GPU) from CoolerControl API.
 * @details Fills the cc_sensor_data_t struct with the latest values. Returns 1 on success, 0 on failure.
 * @example
 *     cc_sensor_data_t data;
 *     if (cc_get_sensor_data(&config, &data)) { ... }
 */
int cc_get_sensor_data(const Config *config, cc_sensor_data_t *data);

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
 * @param config Pointer to configuration struct.
 * @return 1 on success, 0 on failure.
 * @example
 *     if (monitor_init(&config)) { ... }
 */
int monitor_init(const Config *config);

/**
 * @brief Get current temperatures from all available sensors.
 * @details Reads the current CPU and GPU temperatures. Returns 1 on success, 0 on failure.
 * @param temp_1 Pointer to float for CPU temperature (may be NULL).
 * @param temp_2 Pointer to float for GPU temperature (may be NULL).
 * @return 1 on success, 0 on failure.
 * @example
 *     float t1, t2;
 *     if (monitor_get_temperatures(&t1, &t2)) { ... }
 */
int monitor_get_temperatures(float *temp_1, float *temp_2);

#endif // COOLERCONTROL_H
