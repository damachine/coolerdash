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
 * @details Reads all sensor data (temps, RPM, duty, watts, freq) from
 * /status endpoint for all device types.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/config.h"
#include "cc_conf.h"
#include "cc_main.h"
#include "cc_sensor.h"

/** @brief Cached CURL handle for reuse across polling cycles. */
static CURL *sensor_curl_handle = NULL;

/**
 * @brief Initialize or retrieve cached CURL handle for sensor polling.
 * @return Cached CURL handle, or NULL on failure
 */
static CURL *get_sensor_curl_handle(void)
{
    if (!sensor_curl_handle)
    {
        sensor_curl_handle = curl_easy_init();
        if (!sensor_curl_handle)
            log_message(LOG_ERROR, "Failed to initialize sensor CURL handle");
    }
    return sensor_curl_handle;
}

/**
 * @brief Cleanup cached sensor CURL handle.
 * @details Called during daemon shutdown to free resources.
 */
void cleanup_sensor_curl_handle(void)
{
    if (sensor_curl_handle)
    {
        curl_easy_cleanup(sensor_curl_handle);
        sensor_curl_handle = NULL;
    }
}

/**
 * @brief Add a sensor entry to the monitor data collection.
 * @details Helper to append a new sensor entry with bounds checking.
 * @return 1 if added, 0 if array full
 */
static int add_sensor_entry(monitor_sensor_data_t *data,
                            const char *name, const char *device_uid,
                            const char *device_type, sensor_category_t category,
                            float value, const char *unit, int use_decimal)
{
    if (data->sensor_count >= MAX_SENSORS)
        return 0;

    sensor_entry_t *entry = &data->sensors[data->sensor_count];
    cc_safe_strcpy(entry->name, sizeof(entry->name), name);
    cc_safe_strcpy(entry->device_uid, sizeof(entry->device_uid), device_uid);
    cc_safe_strcpy(entry->device_type, sizeof(entry->device_type), device_type);
    cc_safe_strcpy(entry->unit, sizeof(entry->unit), unit);

    /* Device name from cache */
    const char *dev_name = get_device_name_by_uid(device_uid);
    cc_safe_strcpy(entry->device_name, sizeof(entry->device_name), dev_name);

    entry->category = category;
    entry->value = value;
    entry->use_decimal = use_decimal;

    data->sensor_count++;
    return 1;
}

/**
 * @brief Collect all temperature sensors from a device's latest status.
 * @details Iterates temps[] array in the last status_history entry.
 */
static void collect_device_temps(const json_t *device, const char *device_uid,
                                 const char *device_type,
                                 monitor_sensor_data_t *data)
{
    const json_t *status_history = json_object_get(device, "status_history");
    if (!status_history || !json_is_array(status_history))
        return;

    size_t history_count = json_array_size(status_history);
    if (history_count == 0)
        return;

    const json_t *last_status = json_array_get(status_history, history_count - 1);
    if (!last_status)
        return;

    const json_t *temps = json_object_get(last_status, "temps");
    if (!temps || !json_is_array(temps))
        return;

    size_t temp_count = json_array_size(temps);
    for (size_t i = 0; i < temp_count; i++)
    {
        const json_t *temp_entry = json_array_get(temps, i);
        if (!temp_entry)
            continue;

        const json_t *name_val = json_object_get(temp_entry, "name");
        const json_t *temp_val = json_object_get(temp_entry, "temp");

        if (!name_val || !json_is_string(name_val) ||
            !temp_val || !json_is_number(temp_val))
            continue;

        float temperature = (float)json_number_value(temp_val);

        /* Skip invalid readings */
        if (temperature < -50.0f || temperature > 200.0f)
            continue;

        /* Decimal only for Liquidctl (coolant) sensors */
        int use_dec = (strcmp(device_type, "Liquidctl") == 0) ? 1 : 0;

        add_sensor_entry(data, json_string_value(name_val), device_uid,
                         device_type, SENSOR_CATEGORY_TEMP, temperature,
                         "\xC2\xB0"
                         "C",
                         use_dec);
    }
}

/**
 * @brief Collect all channel sensors from a device's latest status.
 * @details Iterates channels[] array and creates entries for RPM, duty,
 * watts, and freq values.
 */
static void collect_device_channels(const json_t *device,
                                    const char *device_uid,
                                    const char *device_type,
                                    monitor_sensor_data_t *data)
{
    const json_t *status_history = json_object_get(device, "status_history");
    if (!status_history || !json_is_array(status_history))
        return;

    size_t history_count = json_array_size(status_history);
    if (history_count == 0)
        return;

    const json_t *last_status = json_array_get(status_history, history_count - 1);
    if (!last_status)
        return;

    const json_t *channels = json_object_get(last_status, "channels");
    if (!channels || !json_is_array(channels))
        return;

    size_t channel_count = json_array_size(channels);
    for (size_t i = 0; i < channel_count; i++)
    {
        const json_t *ch = json_array_get(channels, i);
        if (!ch)
            continue;

        const json_t *name_val = json_object_get(ch, "name");
        if (!name_val || !json_is_string(name_val))
            continue;

        const char *ch_name = json_string_value(name_val);
        char sensor_name[SENSOR_NAME_LEN];

        /* RPM */
        const json_t *rpm_val = json_object_get(ch, "rpm");
        if (rpm_val && json_is_number(rpm_val))
        {
            float rpm = (float)json_number_value(rpm_val);
            if (rpm >= 0.0f)
            {
                int n = snprintf(sensor_name, sizeof(sensor_name),
                                 "%s RPM", ch_name);
                if (n > 0 && (size_t)n < sizeof(sensor_name))
                    add_sensor_entry(data, sensor_name, device_uid,
                                     device_type, SENSOR_CATEGORY_RPM,
                                     rpm, "RPM", 0);
            }
        }

        /* Duty cycle */
        const json_t *duty_val = json_object_get(ch, "duty");
        if (duty_val && json_is_number(duty_val))
        {
            float duty = (float)json_number_value(duty_val);
            int n = snprintf(sensor_name, sizeof(sensor_name),
                             "%s Duty", ch_name);
            if (n > 0 && (size_t)n < sizeof(sensor_name))
                add_sensor_entry(data, sensor_name, device_uid,
                                 device_type, SENSOR_CATEGORY_DUTY,
                                 duty, "%", 1);
        }

        /* Watts */
        const json_t *watts_val = json_object_get(ch, "watts");
        if (watts_val && json_is_number(watts_val))
        {
            float watts = (float)json_number_value(watts_val);
            int n = snprintf(sensor_name, sizeof(sensor_name),
                             "%s Power", ch_name);
            if (n > 0 && (size_t)n < sizeof(sensor_name))
                add_sensor_entry(data, sensor_name, device_uid,
                                 device_type, SENSOR_CATEGORY_WATTS,
                                 watts, "W", 1);
        }

        /* Frequency */
        const json_t *freq_val = json_object_get(ch, "freq");
        if (freq_val && json_is_number(freq_val))
        {
            float freq = (float)json_number_value(freq_val);
            int n = snprintf(sensor_name, sizeof(sensor_name),
                             "%s Freq", ch_name);
            if (n > 0 && (size_t)n < sizeof(sensor_name))
                add_sensor_entry(data, sensor_name, device_uid,
                                 device_type, SENSOR_CATEGORY_FREQ,
                                 freq, "MHz", 0);
        }
    }
}

/**
 * @brief Parse /status JSON and collect all sensors from all devices.
 * @details Iterates all devices and collects temperature and channel data
 * into the monitor_sensor_data_t structure.
 */
static int parse_all_sensor_data(const char *json, monitor_sensor_data_t *data)
{
    if (!json || json[0] == '\0' || !data)
    {
        log_message(LOG_ERROR, "Invalid input for sensor parsing");
        return 0;
    }

    data->sensor_count = 0;

    json_error_t json_error;
    json_t *root = json_loads(json, 0, &json_error);
    if (!root)
    {
        log_message(LOG_ERROR, "JSON parse error: %s", json_error.text);
        return 0;
    }

    const json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_array(devices))
    {
        json_decref(root);
        return 0;
    }

    size_t device_count = json_array_size(devices);
    for (size_t i = 0; i < device_count; i++)
    {
        const json_t *device = json_array_get(devices, i);
        if (!device)
            continue;

        const char *device_type = extract_device_type_from_json(device);
        if (!device_type)
            continue;

        /* Extract device UID */
        const json_t *uid_val = json_object_get(device, "uid");
        if (!uid_val || !json_is_string(uid_val))
            continue;

        const char *device_uid = json_string_value(uid_val);

        /* Collect all temps and channels from this device */
        collect_device_temps(device, device_uid, device_type, data);
        collect_device_channels(device, device_uid, device_type, data);
    }

    json_decref(root);
    log_message(LOG_INFO, "Parsed %d sensors from %zu devices",
                data->sensor_count, device_count);
    return 1;
}

/**
 * @brief Configure CURL for status API request.
 * @details Helper function to set up CURL options for temperature data request.
 */
static void configure_status_request(CURL *curl, const char *url,
                                     struct http_response *response)
{
    // Basic CURL configuration
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     (curl_write_callback)write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CoolerDash/1.0");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // POST data for status request
    const char *post_data =
        "{\"all\":false,\"since\":\"1970-01-01T00:00:00.000Z\"}";
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
}

/**
 * @brief Get all sensor data from CoolerControl /status API.
 * @details Polls the API and collects all sensors into the data structure.
 */
static int get_sensor_data_from_api(const Config *config,
                                    monitor_sensor_data_t *data)
{
    if (!config || !data)
        return 0;

    data->sensor_count = 0;

    if (config->daemon_address[0] == '\0')
    {
        log_message(LOG_ERROR, "No daemon address configured");
        return 0;
    }

    CURL *curl = get_sensor_curl_handle();
    if (!curl)
        return 0;

    curl_easy_reset(curl);

    char url[256];
    int url_len = snprintf(url, sizeof(url), "%s/status", config->daemon_address);
    if (url_len < 0 || url_len >= (int)sizeof(url))
        return 0;

    struct http_response response = {0};
    if (!cc_init_response_buffer(&response, 8192))
    {
        log_message(LOG_ERROR, "Failed to allocate response buffer");
        return 0;
    }

    configure_status_request(curl, url, &response);

    cc_apply_tls_to_curl(curl, config);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    headers = cc_apply_auth_to_curl(headers, config);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    int result = 0;
    CURLcode curl_result = curl_easy_perform(curl);
    if (curl_result == CURLE_OK)
    {
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if (response_code == 200)
        {
            result = parse_all_sensor_data(response.data, data);
        }
        else
        {
            log_message(LOG_ERROR, "HTTP error: %ld when fetching sensor data",
                        response_code);
        }
    }
    else
    {
        log_message(LOG_ERROR, "CURL error: %s", curl_easy_strerror(curl_result));
    }

    cc_cleanup_response_buffer(&response);
    if (headers)
        curl_slist_free_all(headers);

    return result;
}

// ============================================================================
// Slot Resolution Functions
// ============================================================================

/**
 * @brief Check if a slot value is a legacy type.
 */
int is_legacy_sensor_slot(const char *slot_value)
{
    if (!slot_value)
        return 0;
    return (strcmp(slot_value, "cpu") == 0 ||
            strcmp(slot_value, "gpu") == 0 ||
            strcmp(slot_value, "liquid") == 0 ||
            strcmp(slot_value, "none") == 0);
}

/**
 * @brief Resolve a legacy slot value to matching sensor entry.
 * @details Maps "cpu"→first CPU temp, "gpu"→first GPU temp,
 * "liquid"→first Liquidctl temp.
 */
static const sensor_entry_t *resolve_legacy_slot(
    const monitor_sensor_data_t *data, const char *slot_value)
{
    if (!data || !slot_value)
        return NULL;

    const char *target_type = NULL;
    if (strcmp(slot_value, "cpu") == 0)
        target_type = "CPU";
    else if (strcmp(slot_value, "gpu") == 0)
        target_type = "GPU";
    else if (strcmp(slot_value, "liquid") == 0)
        target_type = "Liquidctl";
    else
        return NULL;

    /* Find first temperature sensor matching the device type */
    for (int i = 0; i < data->sensor_count; i++)
    {
        if (data->sensors[i].category == SENSOR_CATEGORY_TEMP &&
            strcmp(data->sensors[i].device_type, target_type) == 0)
        {
            return &data->sensors[i];
        }
    }

    return NULL;
}

/**
 * @brief Find sensor entry matching a slot value.
 * @details Handles both legacy ("cpu","gpu","liquid") and dynamic
 * ("uid:sensor_name") slot resolution.
 */
const sensor_entry_t *find_sensor_for_slot(const monitor_sensor_data_t *data,
                                           const char *slot_value)
{
    if (!data || !slot_value || strcmp(slot_value, "none") == 0)
        return NULL;

    /* Legacy slot resolution */
    if (is_legacy_sensor_slot(slot_value))
        return resolve_legacy_slot(data, slot_value);

    /* Dynamic slot: "device_uid:sensor_name" */
    const char *separator = strchr(slot_value, ':');
    if (!separator || separator == slot_value)
        return NULL;

    size_t uid_len = (size_t)(separator - slot_value);
    const char *sensor_name = separator + 1;

    for (int i = 0; i < data->sensor_count; i++)
    {
        if (strlen(data->sensors[i].device_uid) == uid_len &&
            strncmp(data->sensors[i].device_uid, slot_value, uid_len) == 0 &&
            strcmp(data->sensors[i].name, sensor_name) == 0)
        {
            return &data->sensors[i];
        }
    }

    return NULL;
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Get all sensor data from CoolerControl API.
 */
int get_sensor_monitor_data(const Config *config, monitor_sensor_data_t *data)
{
    if (!config || !data)
        return 0;

    return get_sensor_data_from_api(config, data);
}
