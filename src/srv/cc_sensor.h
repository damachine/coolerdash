/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief Sensor monitoring via CoolerControl API.
 * @details Reads all sensor data (temperatures, RPM, duty, watts, frequency)
 * from /status endpoint for all device types (CPU, GPU, Liquidctl, Hwmon,
 * CustomSensors).
 */

#ifndef CC_SENSOR_H
#define CC_SENSOR_H

// cppcheck-suppress-begin missingIncludeSystem
#include <stddef.h>
// cppcheck-suppress-end missingIncludeSystem

// Forward declaration
struct Config;

// ============================================================================
// Sensor Data Model Constants
// ============================================================================

#define MAX_SENSORS 64
#define SENSOR_NAME_LEN 48
#define SENSOR_UID_LEN 96
#define SENSOR_DEVICE_NAME_LEN 64
#define SENSOR_DEVICE_TYPE_LEN 16
#define SENSOR_UNIT_LEN 8

// ============================================================================
// Sensor Category Enum
// ============================================================================

/**
 * @brief Category of a sensor value.
 * @details Determines default thresholds, display formatting, and unit.
 */
typedef enum
{
    SENSOR_CATEGORY_TEMP = 0, /**< Temperature in °C */
    SENSOR_CATEGORY_RPM,      /**< Fan/Pump speed in RPM */
    SENSOR_CATEGORY_DUTY,     /**< Duty cycle in % */
    SENSOR_CATEGORY_WATTS,    /**< Power consumption in W */
    SENSOR_CATEGORY_FREQ      /**< Frequency in MHz */
} sensor_category_t;

// ============================================================================
// Sensor Entry
// ============================================================================

/**
 * @brief Single sensor data entry from CoolerControl API.
 * @details Represents one sensor value with its metadata. Names match
 * CoolerControl's display names for maximum UI synchronicity.
 */
typedef struct
{
    char name[SENSOR_NAME_LEN];               /**< CC sensor name (e.g. "temp1", "Liquid Temperature") */
    char device_uid[SENSOR_UID_LEN];          /**< CC device UID */
    char device_name[SENSOR_DEVICE_NAME_LEN]; /**< CC device name (e.g. "NZXT Kraken Z73") */
    char device_type[SENSOR_DEVICE_TYPE_LEN]; /**< CC device type ("CPU","GPU","Liquidctl","Hwmon","CustomSensors") */
    sensor_category_t category;               /**< Sensor value category */
    float value;                              /**< Current sensor value */
    char unit[SENSOR_UNIT_LEN];               /**< Display unit ("°C","RPM","%","W","MHz") */
    int use_decimal;                          /**< 1=show decimal (e.g. 31.5), 0=integer (e.g. 1200) */
} sensor_entry_t;

// ============================================================================
// Monitor Sensor Data (runtime collection)
// ============================================================================

/**
 * @brief Collection of all sensor values from one API poll.
 * @details Contains all discovered sensors across all CoolerControl devices.
 */
typedef struct
{
    sensor_entry_t sensors[MAX_SENSORS]; /**< Array of all discovered sensors */
    int sensor_count;                    /**< Number of valid entries in sensors[] */
} monitor_sensor_data_t;

// ============================================================================
// Sensor Slot Resolution Functions
// ============================================================================

/**
 * @brief Find sensor entry matching a slot value.
 * @details Resolves legacy ("cpu","gpu","liquid") and dynamic ("uid:name")
 * slot values to the corresponding sensor_entry_t in the data array.
 * @param data Current sensor data collection
 * @param slot_value Slot configuration value
 * @return Pointer to matching sensor entry, or NULL if not found
 */
const sensor_entry_t *find_sensor_for_slot(const monitor_sensor_data_t *data,
                                           const char *slot_value);

/**
 * @brief Check if a slot value is a legacy type.
 * @param slot_value Slot configuration value
 * @return 1 if "cpu", "gpu", "liquid", or "none"; 0 otherwise
 */
int is_legacy_sensor_slot(const char *slot_value);

// ============================================================================
// Data Retrieval
// ============================================================================

/**
 * @brief Get all sensor data from CoolerControl API.
 * @details Polls /status endpoint and collects all sensors (temps + channels)
 * from all device types into the monitor_sensor_data_t structure.
 * @param config Configuration with daemon address
 * @param data Output sensor data structure
 * @return 1 on success, 0 on failure
 */
int get_sensor_monitor_data(const struct Config *config,
                            monitor_sensor_data_t *data);

/**
 * @brief Cleanup cached sensor CURL handle.
 * @details Called during daemon shutdown to free resources.
 */
void cleanup_sensor_curl_handle(void);

#endif // CC_SENSOR_H
