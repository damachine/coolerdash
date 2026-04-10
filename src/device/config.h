/**
 * @author damachine (damachin3 at proton dot me)
 * @Maintainer: damachine (damachin3 at proton dot me)
 * @website https://github.com/damachine/coolerdash
 * @copyright (c) 2025 damachine
 * @license MIT
 *    This software is provided "as is", without warranty of any kind...
 */

/**
 * @brief JSON configuration loader with hardcoded defaults.
 */

#ifndef CONFIG_H
#define CONFIG_H

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stdint.h>
// cppcheck-suppress-end missingIncludeSystem

// Configuration constants
#define CONFIG_MAX_STRING_LEN 256
#define CONFIG_MAX_TOKEN_LEN 64
#define CONFIG_MAX_PATH_LEN 512
#define CONFIG_MAX_FONT_NAME_LEN 64
#define CONFIG_MAX_SENSOR_SLOT_LEN 256
#define CONFIG_MAX_PATTERN_LIST_LEN 512

/** @brief RGB color; is_set=0 uses default, is_set=1 uses custom value. */
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t is_set; // 0 = use default, 1 = user-defined (allows all RGB values)
} Color;

/** @brief Log levels: INFO (verbose), STATUS/WARNING/ERROR (always shown). */
typedef enum
{
    LOG_INFO,
    LOG_STATUS,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

// ============================================================================
// Sensor Configuration (per-sensor thresholds, colors, offsets)
// ============================================================================

#define MAX_SENSOR_CONFIGS 32
#define SENSOR_CONFIG_ID_LEN 256

/** @brief Per-sensor thresholds, colors, label and display offsets. */
typedef struct
{
    char sensor_id[SENSOR_CONFIG_ID_LEN]; /**< Sensor identifier */
    float threshold_1;                    /**< Low threshold */
    float threshold_2;                    /**< Medium threshold */
    float threshold_3;                    /**< High threshold */
    float max_scale;                      /**< Maximum bar scale value */
    Color threshold_1_bar;                /**< Bar color below threshold_1 */
    Color threshold_2_bar;                /**< Bar color at threshold_1..2 */
    Color threshold_3_bar;                /**< Bar color at threshold_2..3 */
    Color threshold_4_bar;                /**< Bar color above threshold_3 */
    int offset_x;                         /**< Display X offset for value text */
    int offset_y;                         /**< Display Y offset for value text */
    float font_size_temp;                 /**< Per-sensor temp font size (0 = use global) */
    char label[32];                       /**< Custom display label (empty = auto) */
    float value_to_bar_gap;               /**< Gap above bar as % of available_height (0 = auto) */
    float label_to_bar_gap;               /**< Gap below bar as % of available_height (0 = auto) */
} SensorConfig;

/** @brief All CoolerDash settings: daemon, paths, display, theme, sensors. */
typedef struct Config
{
    // Daemon configuration
    char daemon_address[CONFIG_MAX_STRING_LEN];
    char access_token[CONFIG_MAX_TOKEN_LEN];

    // Paths configuration
    char paths_images[CONFIG_MAX_PATH_LEN];
    char paths_image_coolerdash[CONFIG_MAX_PATH_LEN];
    char paths_image_background[CONFIG_MAX_PATH_LEN];
    char paths_image_shutdown[CONFIG_MAX_PATH_LEN];

    // LCD device detection configuration
    char device_detection_mode[16];
    char device_detection_allowlist[CONFIG_MAX_PATTERN_LIST_LEN];
    char device_detection_blocklist[CONFIG_MAX_PATTERN_LIST_LEN];

    // Display configuration
    uint16_t display_width;
    uint16_t display_height;
    float display_refresh_interval;
    uint8_t lcd_brightness;
    uint8_t lcd_orientation;
    char display_mode[16];
    char display_background_image_fit[16];
    uint16_t circle_switch_interval;
    int circle_show_extra_info;
    float display_content_scale_factor;
    float display_background_overlay_opacity;

    // Sensor slot configuration (flexible sensor assignment)
    char sensor_slot_1[CONFIG_MAX_SENSOR_SLOT_LEN]; // "cpu", "gpu", "liquid", "none"
    char sensor_slot_2[CONFIG_MAX_SENSOR_SLOT_LEN]; // "cpu", "gpu", "liquid", "none"
    char sensor_slot_3[CONFIG_MAX_SENSOR_SLOT_LEN]; // "cpu", "gpu", "liquid", "none"

    // Layout configuration
    uint16_t layout_bar_height;
    uint16_t layout_bar_height_1; // Individual bar height for slot 1
    uint16_t layout_bar_height_2; // Individual bar height for slot 2
    uint16_t layout_bar_height_3; // Individual bar height for slot 3
    uint16_t layout_bar_gap;
    float layout_bar_border;
    float layout_bar_opacity;
    int layout_bar_border_enabled; // 1=enabled, 0=disabled, -1=auto (use default)
    uint8_t layout_bar_width;
    uint8_t layout_label_margin_left;
    uint8_t layout_label_margin_bar;
    Color display_background_color; // Main display background color
    Color layout_bar_color_background;
    Color layout_bar_color_border;

    // Font configuration
    char font_face[CONFIG_MAX_FONT_NAME_LEN];
    float font_size_temp;
    float font_size_labels;
    float font_growth_factor;
    Color font_color_temp;
    Color font_color_label;

    // Display positioning overrides (global)
    int display_degree_spacing;
    int display_label_offset_x;
    int display_label_offset_y;
    int display_margin_top;    /**< Vertical top margin (0 = auto-detect) */
    int display_margin_bottom; /**< Vertical bottom margin (0 = auto-detect) */

    // Per-sensor configuration (thresholds, colors, offsets)
    SensorConfig sensor_configs[MAX_SENSOR_CONFIGS]; /**< Sensor-specific configs */
    int sensor_config_count;                         /**< Number of valid entries */
} Config;

/** @brief Global log function (implemented in config.c, used everywhere). */
void log_message(log_level_t level, const char *format, ...);

/** @brief Verbose logging flag (set by --verbose CLI arg). */
extern int verbose_logging;

// ============================================================================
// Helper Macros
// ============================================================================

/** @brief Safe strcpy: copies src into dest[dest_size] with null-termination. */
#define SAFE_STRCPY(dest, src)                       \
    do                                               \
    {                                                \
        cc_safe_strcpy((dest), sizeof(dest), (src)); \
    } while (0)

/** @brief Returns 1 if orientation is 0, 90, 180, or 270. */
static inline int is_valid_orientation(int orientation)
{
    return (orientation == 0 || orientation == 90 || orientation == 180 ||
            orientation == 270);
}

// ============================================================================
// Plugin Configuration Functions
// ============================================================================

/**
 * @brief Load complete configuration from config.json with hardcoded defaults.
 * @param config Pointer to Config struct to populate
 * @param config_path Optional path to config.json (NULL = use default location)
 * @return 1 on success (config loaded), 0 if using defaults only
 *
 * This function:
 * 1. Initializes config with hardcoded defaults
 * 2. Tries to load config.json from plugin directory
 * 3. Applies all settings with validation
 * 4. Returns ready-to-use config (always succeeds)
 *
 * Default config.json location:
 *   /etc/coolercontrol/plugins/coolerdash/config.json
 */
int load_plugin_config(Config *config, const char *config_path);

/** @brief Find SensorConfig by sensor_id; returns NULL if not found. */
const SensorConfig *get_sensor_config(const Config *config, const char *sensor_id);

/**
 * @brief Initialize a SensorConfig with default values for a given category.
 * @param sc SensorConfig to initialize
 * @param sensor_id Sensor identifier to set
 * @param category 0=temp, 1=rpm, 2=duty, 3=watts, 4=freq
 */
void init_default_sensor_config(SensorConfig *sc, const char *sensor_id, int category);

#endif // CONFIG_H
