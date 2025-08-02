/*
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */

/**
 * @brief INI parser handler for CoolerDash configuration.
 * @details Parses the configuration file and sets values in the Config struct.
 * @example
 *     See function documentation for usage examples.
 */

// Helper function for safe string copying
#define SAFE_STRCPY(dest, src) do { \
    strncpy(dest, src, sizeof(dest) - 1); \
    dest[sizeof(dest) - 1] = '\0'; \
} while(0)

// Include necessary headers
#include <errno.h>
#include <ini.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include project headers
#include "../include/config.h"
#include "../include/coolercontrol.h"

/**
 * @brief Log levels for consistent logging across modules.
 * @details Matches the logging style from main.c for consistency.
 * @example
 *     log_message(LOG_ERROR, "Failed to parse config: %s", error_msg);
 */
typedef enum {
    LOG_INFO,
    LOG_WARNING, 
    LOG_ERROR
} log_level_t;

/**
 * @brief Centralized logging function with consistent format.
 * @details Provides consistent logging style matching main.c implementation.
 * @example
 *     log_message(LOG_WARNING, "Config file not found: %s", path);
 */
static void log_message(log_level_t level, const char *format, ...) {
    const char *prefix[] = {"INFO", "WARNING", "ERROR"};
    FILE *output = (level == LOG_ERROR) ? stderr : stdout;
    
    fprintf(output, "[CoolerDash %s] ", prefix[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    
    fprintf(output, "\n");
    fflush(output);
}

/**
 * @brief Secure helper functions for string parsing with validation.
 * @details Replaces unsafe atoi/atof with secure parsing that validates input.
 * @example
 *     int val = safe_atoi("123", 0);
 */

// Safe integer parsing with validation
static inline int safe_atoi(const char *str, int default_value) {
    if (!str || !str[0]) return default_value;
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || *endptr != '\0') return default_value; // Invalid conversion
    if (val < INT_MIN || val > INT_MAX) return default_value;   // Overflow
    return (int)val;
}

// Safe float parsing with validation  
static inline float safe_atof(const char *str, float default_value) {
    if (!str || !str[0]) return default_value;
    char *endptr;
    float val = strtof(str, &endptr);
    if (endptr == str || *endptr != '\0') return default_value; // Invalid conversion
    return val;
}

/**
 * @brief Helper function for secure color parsing with validation.
 * @details Parses RGB color values and automatically validates with uint8_t type safety.
 * @example
 *     parse_color_component(value, &config->font_color_temp.r);
 */
static void parse_color_component(const char *value, uint8_t *component) {
    if (!value || !component) return;
    int val = safe_atoi(value, 0);
    *component = (val < 0) ? 0 : (val > 255) ? 255 : (uint8_t)val;
}

/**
 * @brief Handle daemon section configuration.
 * @details Processes daemon-related configuration keys (address, password).
 * @example
 *     handle_daemon_section(config, "address", "http://localhost:11987");
 */
static int handle_daemon_section(Config *config, const char *name, const char *value) {
    if (strcmp(name, "address") == 0) {
        if (value && value[0] != '\0') {
            SAFE_STRCPY(config->daemon_address, value);
        }
    } else if (strcmp(name, "password") == 0) {
        if (value && value[0] != '\0') {
            SAFE_STRCPY(config->daemon_password, value);
        }
    }
    return 1;
}

/**
 * @brief Handle paths section configuration.
 * @details Processes path-related configuration keys (images, pid, etc.).
 * @example
 *     handle_paths_section(config, "images", "/opt/coolerdash/images");
 */
static int handle_paths_section(Config *config, const char *name, const char *value) {
    if (strcmp(name, "images") == 0) {
        if (value && value[0] != '\0') {
            SAFE_STRCPY(config->paths_images, value);
        }
    } else if (strcmp(name, "image_coolerdash") == 0) {
        if (value && value[0] != '\0') {
            SAFE_STRCPY(config->paths_image_coolerdash, value);
        }
    } else if (strcmp(name, "image_shutdown") == 0) {
        if (value && value[0] != '\0') {
            SAFE_STRCPY(config->paths_image_shutdown, value);
        }
    } else if (strcmp(name, "pid") == 0) {
        if (value && value[0] != '\0') {
            SAFE_STRCPY(config->paths_pid, value);
        }
    }
    return 1;
}

/**
 * @brief Handle display section configuration.
 * @details Processes display-related configuration keys (width, height, brightness, etc.).
 * @example
 *     handle_display_section(config, "width", "240");
 */
static int handle_display_section(Config *config, const char *name, const char *value) {
    if (strcmp(name, "width") == 0) {
        int width = safe_atoi(value, 0);
        config->display_width = (width > 0) ? (uint16_t)width : 0;
    } else if (strcmp(name, "height") == 0) {
        int height = safe_atoi(value, 0);
        config->display_height = (height > 0) ? (uint16_t)height : 0;
    } else if (strcmp(name, "refresh_interval_sec") == 0) {
        config->display_refresh_interval_sec = safe_atoi(value, 0);
    } else if (strcmp(name, "refresh_interval_nsec") == 0) {
        config->display_refresh_interval_nsec = safe_atoi(value, 0);
    } else if (strcmp(name, "brightness") == 0) {
        int brightness = safe_atoi(value, 0);
        config->lcd_brightness = (brightness >= 0 && brightness <= 100) ? (uint8_t)brightness : 0;
    } else if (strcmp(name, "orientation") == 0) {
        config->lcd_orientation = safe_atoi(value, 0);
    } else if (strcmp(name, "temp_1_update_threshold") == 0) {
        config->temp_1_update_threshold = safe_atof(value, 0.0f);
    } else if (strcmp(name, "temp_2_update_threshold") == 0) {
        config->temp_2_update_threshold = safe_atof(value, 0.0f);
    }
    return 1;
}

/**
 * @brief Handle layout section configuration.
 * @details Processes layout-related configuration keys (box dimensions, bar settings, etc.).
 * @example
 *     handle_layout_section(config, "box_width", "240");
 */
static int handle_layout_section(Config *config, const char *name, const char *value) {
    if (strcmp(name, "box_width") == 0) {
        config->layout_box_width = safe_atoi(value, 0);
    } else if (strcmp(name, "box_height") == 0) {
        config->layout_box_height = safe_atoi(value, 0);
    } else if (strcmp(name, "box_gap") == 0) {
        config->layout_box_gap = safe_atoi(value, 0);
    } else if (strcmp(name, "bar_width") == 0) {
        config->layout_bar_width = safe_atoi(value, 0);
    } else if (strcmp(name, "bar_height") == 0) {
        config->layout_bar_height = safe_atoi(value, 0);
    } else if (strcmp(name, "bar_gap") == 0) {
        config->layout_bar_gap = safe_atoi(value, 0);
    } else if (strcmp(name, "bar_border_width") == 0) {
        config->layout_bar_border_width = safe_atof(value, 0.0f);
    }
    return 1;
}

/**
 * @brief Handle font section configuration.
 * @details Processes font-related configuration keys (face, sizes).
 * @example
 *     handle_font_section(config, "font_face", "Roboto Black");
 */
static int handle_font_section(Config *config, const char *name, const char *value) {
    if (strcmp(name, "font_face") == 0) {
        if (value && value[0] != '\0') {
            SAFE_STRCPY(config->font_face, value);
        }
    } else if (strcmp(name, "font_size_temp") == 0) {
        config->font_size_temp = safe_atof(value, 12.0f);
    } else if (strcmp(name, "font_size_labels") == 0) {
        config->font_size_labels = safe_atof(value, 10.0f);
    }
    return 1;
}

/**
 * @brief Handle temperature section configuration.
 * @details Processes temperature threshold configuration keys.
 * @example
 *     handle_temperature_section(config, "temp_threshold_1", "55.0");
 */
static int handle_temperature_section(Config *config, const char *name, const char *value) {
    if (strcmp(name, "temp_threshold_1") == 0) {
        config->temp_threshold_1 = safe_atof(value, 50.0f);
    } else if (strcmp(name, "temp_threshold_2") == 0) {
        config->temp_threshold_2 = safe_atof(value, 65.0f);
    } else if (strcmp(name, "temp_threshold_3") == 0) {
        config->temp_threshold_3 = safe_atof(value, 80.0f);
    }
    return 1;
}

/**
 * @brief Handle color section configuration.
 * @details Processes color-related configuration keys for various UI elements.
 * @example
 *     handle_color_section(config, "bar_color_background", "r", "64");
 */
static int handle_color_section(Config *config, const char *section, const char *name, const char *value) {
    if (strcmp(section, "bar_color_background") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->layout_bar_color_background.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->layout_bar_color_background.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->layout_bar_color_background.b);
    } else if (strcmp(section, "bar_color_border") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->layout_bar_color_border.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->layout_bar_color_border.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->layout_bar_color_border.b);
    } else if (strcmp(section, "font_color_temp") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->font_color_temp.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->font_color_temp.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->font_color_temp.b);
    } else if (strcmp(section, "font_color_label") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->font_color_label.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->font_color_label.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->font_color_label.b);
    } else if (strcmp(section, "temp_threshold_1_bar") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->temp_threshold_1_bar.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->temp_threshold_1_bar.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->temp_threshold_1_bar.b);
    } else if (strcmp(section, "temp_threshold_2_bar") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->temp_threshold_2_bar.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->temp_threshold_2_bar.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->temp_threshold_2_bar.b);
    } else if (strcmp(section, "temp_threshold_3_bar") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->temp_threshold_3_bar.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->temp_threshold_3_bar.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->temp_threshold_3_bar.b);
    } else if (strcmp(section, "temp_threshold_4_bar") == 0) {
        if (strcmp(name, "r") == 0) parse_color_component(value, &config->temp_threshold_4_bar.r);
        else if (strcmp(name, "g") == 0) parse_color_component(value, &config->temp_threshold_4_bar.g);
        else if (strcmp(name, "b") == 0) parse_color_component(value, &config->temp_threshold_4_bar.b);
    }
    return 1;
}

/**
 * @brief Main INI parser handler, delegates to section-specific handlers.
 * @details Called for each key-value pair in the INI file. Routes to appropriate section handler for cleaner code organization.
 * @example
 *     ini_parse("/etc/coolerdash/config.ini", inih_config_handler, &cfg);
 */
static int inih_config_handler(void *user, const char *section, const char *name, const char *value) {
    Config *config = (Config *)user;

    // Route to appropriate section handler
    if (strcmp(section, "daemon") == 0) {
        return handle_daemon_section(config, name, value);
    } else if (strcmp(section, "paths") == 0) {
        return handle_paths_section(config, name, value);
    } else if (strcmp(section, "display") == 0) {
        return handle_display_section(config, name, value);
    } else if (strcmp(section, "layout") == 0) {
        return handle_layout_section(config, name, value);
    } else if (strcmp(section, "font") == 0) {
        return handle_font_section(config, name, value);
    } else if (strcmp(section, "temperature") == 0) {
        return handle_temperature_section(config, name, value);
    } else if (strstr(section, "color") != NULL || strstr(section, "_bar") != NULL) {
        // Handle all color sections (bar_color_*, font_color_*, temp_threshold_*_bar)
        return handle_color_section(config, section, name, value);
    }
    
    return 1; // Unknown section, but continue parsing
}

/**
 * @brief Sets fallback default values for missing or empty configuration fields.
 * @details This function should be called after parsing the INI file to ensure all important fields are set to sensible defaults if not provided. Tries to get LCD display dimensions from Liquidctl device as fallback.
 * @example
 *     Config cfg;
 *     if (load_config_ini(&cfg, "/etc/coolerdash/config.ini") != 0) { // handle error }
 *     config_apply_fallbacks(&cfg);
 */
void config_apply_fallbacks(Config *config) {
    if (!config) return;
    
    // Daemon
    if (config->daemon_address[0] == '\0') SAFE_STRCPY(config->daemon_address, "http://localhost:11987");
    if (config->daemon_password[0] == '\0') SAFE_STRCPY(config->daemon_password, "coolAdmin");
    
    // Paths
    if (config->paths_images[0] == '\0') SAFE_STRCPY(config->paths_images, "/opt/coolerdash/images");
    if (config->paths_image_coolerdash[0] == '\0') SAFE_STRCPY(config->paths_image_coolerdash, "/tmp/coolerdash.png");
    if (config->paths_image_shutdown[0] == '\0') SAFE_STRCPY(config->paths_image_shutdown, "/opt/coolerdash/images/shutdown.png");
    if (config->paths_pid[0] == '\0') SAFE_STRCPY(config->paths_pid, "/run/coolerdash/coolerdash.pid");
    
    // Display - try to get dimensions from Liquidctl device first
    if (config->display_width == 0 || config->display_height == 0) {
        int lcd_width = 0, lcd_height = 0;
        // Try to get LCD display info from Liquidctl device via API
        if (get_liquidctl_display_info(config, &lcd_width, &lcd_height) && lcd_width > 0 && lcd_height > 0) {
            if (config->display_width == 0) config->display_width = lcd_width;
            if (config->display_height == 0) config->display_height = lcd_height;
        }
    }
    if (config->display_refresh_interval_sec == 0) config->display_refresh_interval_sec = 2;
    if (config->display_refresh_interval_nsec == 0) config->display_refresh_interval_nsec = 500000000;
    if (config->lcd_brightness == 0) config->lcd_brightness = 80;
    if (config->temp_1_update_threshold == 0.0f) config->temp_1_update_threshold = 1.0f;
    if (config->temp_2_update_threshold == 0.0f) config->temp_2_update_threshold = 1.0f;
    
    // Layout
    if (config->layout_box_width == 0) config->layout_box_width = config->display_width;
    if (config->layout_box_height == 0) config->layout_box_height = config->display_height / 2;
    if (config->layout_bar_width == 0) config->layout_bar_width = config->layout_box_width - 10;
    if (config->layout_bar_height == 0) config->layout_bar_height = 22;
    if (config->layout_bar_gap == 0) config->layout_bar_gap = 10;
    if (config->layout_bar_border_width == 0.0f) config->layout_bar_border_width = 1.5f;
    
    // Font
    if (config->font_face[0] == '\0') SAFE_STRCPY(config->font_face, "Roboto Black");
    if (config->font_size_temp == 0.0f) config->font_size_temp = 100.0f;
    if (config->font_size_labels == 0.0f) config->font_size_labels = 30.0f;
    
    // Temperature thresholds
    if (config->temp_threshold_1 == 0.0f) config->temp_threshold_1 = 55.0f;
    if (config->temp_threshold_2 == 0.0f) config->temp_threshold_2 = 65.0f;
    if (config->temp_threshold_3 == 0.0f) config->temp_threshold_3 = 75.0f;
    
    // Colors
    if (config->layout_bar_color_background.r == 0 && config->layout_bar_color_background.g == 0 && config->layout_bar_color_background.b == 0) {
        config->layout_bar_color_background.r = 64;
        config->layout_bar_color_background.g = 64;
        config->layout_bar_color_background.b = 64;
    }
    if (config->layout_bar_color_border.r == 0 && config->layout_bar_color_border.g == 0 && config->layout_bar_color_border.b == 0) {
        config->layout_bar_color_border.r = 192;
        config->layout_bar_color_border.g = 192;
        config->layout_bar_color_border.b = 192;
    }
    if (config->font_color_temp.r == 0 && config->font_color_temp.g == 0 && config->font_color_temp.b == 0) {
        config->font_color_temp.r = 255;
        config->font_color_temp.g = 255;
        config->font_color_temp.b = 255;
    }
    if (config->font_color_label.r == 0 && config->font_color_label.g == 0 && config->font_color_label.b == 0) {
        config->font_color_label.r = 200;
        config->font_color_label.g = 200;
        config->font_color_label.b = 200;
    }
    
    // Temperature bar colors
    if (config->temp_threshold_1_bar.r == 0 && config->temp_threshold_1_bar.g == 0 && config->temp_threshold_1_bar.b == 0) {
        config->temp_threshold_1_bar.r = 0;
        config->temp_threshold_1_bar.g = 255;
        config->temp_threshold_1_bar.b = 0;
    }
    if (config->temp_threshold_2_bar.r == 0 && config->temp_threshold_2_bar.g == 0 && config->temp_threshold_2_bar.b == 0) {
        config->temp_threshold_2_bar.r = 255;
        config->temp_threshold_2_bar.g = 140;
        config->temp_threshold_2_bar.b = 0;
    }
    if (config->temp_threshold_3_bar.r == 0 && config->temp_threshold_3_bar.g == 0 && config->temp_threshold_3_bar.b == 0) {
        config->temp_threshold_3_bar.r = 255;
        config->temp_threshold_3_bar.g = 70;
        config->temp_threshold_3_bar.b = 0;
    }
    if (config->temp_threshold_4_bar.r == 0 && config->temp_threshold_4_bar.g == 0 && config->temp_threshold_4_bar.b == 0) {
        config->temp_threshold_4_bar.r = 255;
        config->temp_threshold_4_bar.g = 0;
        config->temp_threshold_4_bar.b = 0;
    }
}

/**
 * @brief Validate complete configuration structure.
 * @details Performs comprehensive validation of all configuration fields.
 * @example
 *     if (config_validate(&cfg)) { ... }
 */
int config_validate(const Config *config) {
    if (!config) return 0;
    
    // Validate daemon settings
    if (!config->daemon_address[0] || !config->daemon_password[0]) return 0;
    
    // Validate paths exist and are not empty
    if (!config->paths_images[0] || !config->paths_image_coolerdash[0] || 
        !config->paths_image_shutdown[0] || !config->paths_pid[0]) return 0;
    
    // Validate display dimensions
    if (!config_validate_display_size(config->display_width, config->display_height)) return 0;
    
    // Validate font sizes
    if (config->font_size_temp < CONFIG_MIN_FONT_SIZE || config->font_size_temp > CONFIG_MAX_FONT_SIZE) return 0;
    if (config->font_size_labels < CONFIG_MIN_FONT_SIZE || config->font_size_labels > CONFIG_MAX_FONT_SIZE) return 0;
    
    // Validate temperature thresholds
    if (!config_validate_temperature(config->temp_threshold_1) ||
        !config_validate_temperature(config->temp_threshold_2) ||
        !config_validate_temperature(config->temp_threshold_3)) return 0;
    
    return 1;
}

/**
 * @brief Initialize config structure with safe defaults.
 * @details Sets all fields to safe default values with security considerations.
 * @example
 *     Config cfg;
 *     config_init_defaults(&cfg);
 */
void config_init_defaults(Config *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(Config));
    
    // Apply all fallback values
    config_apply_fallbacks(config);
}

/**
 * @brief Main configuration loading function with enhanced security.
 * @details Loads configuration from INI file with comprehensive validation and secure defaults.
 * @example
 *     Config cfg;
 *     if (load_config("/etc/coolerdash/config.ini", &cfg) != 0) {
 *         // handle error
 *     }
 */
int load_config(const char *path, Config *config) {
    // Validate input parameters
    if (!config || !path) {
        return -1;
    }
    
    // Initialize config struct with zeros to ensure fallbacks work
    memset(config, 0, sizeof(Config));
    
    // Check if file exists and is readable
    FILE *file = fopen(path, "r");
    if (!file) {
        // File doesn't exist - use fallbacks only
        log_message(LOG_INFO, "Config file '%s' not found, using fallback values", path);
        config_apply_fallbacks(config);
        return 0; // Return success, fallbacks are valid
    }
    fclose(file);
    
    // Parse INI file and return success/failure
    int result = (ini_parse(path, inih_config_handler, config) < 0) ? -1 : 0;
    
    // Always apply fallbacks after parsing (for missing/commented values)
    config_apply_fallbacks(config);
    
    return result;
}