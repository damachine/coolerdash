/**
 * -----------------------------------------------------------------------------
 * Created by: damachine (christkue79 at gmail dot com)
 * Website: https://github.com/damachine/coolerdash
 * -----------------------------------------------------------------------------
 */

/**
 * @brief System default configuration values implementation.
 * @details Provides hardcoded fallback values for all CoolerDash configuration
 * parameters.
 */

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../srv/cc_conf.h"
#include "sys.h"

// ============================================================================
// Global Logging Implementation
// ============================================================================

/**
 * @brief Global logging implementation for all modules except main.c
 * @details Provides unified log output for info, status, warning and error
 * messages.
 */
void log_message(log_level_t level, const char *format, ...) {
  if (level == LOG_INFO && !verbose_logging) {
    return;
  }
  const char *prefix[] = {"INFO", "STATUS", "WARNING", "ERROR"};
  FILE *output = (level == LOG_ERROR) ? stderr : stdout;
  fprintf(output, "[CoolerDash %s] ", prefix[level]);

  enum { LOG_MSG_CAP = 1024 };
  char msg_buf[LOG_MSG_CAP];
  msg_buf[0] = '\0';
  va_list args;
  va_start(args, format);
  vsnprintf(msg_buf, sizeof(msg_buf), (format ? format : "(null)"), args);
  va_end(args);
  fputs(msg_buf, output);
  fputc('\n', output);
  fflush(output);
}

// ============================================================================
// Color Default Values Table
// ============================================================================

/**
 * @brief Set daemon default values.
 */
static void set_daemon_defaults(Config *config) {
  if (config->daemon_address[0] == '\0') {
    SAFE_STRCPY(config->daemon_address, "http://localhost:11987");
  }
  if (config->daemon_password[0] == '\0') {
    SAFE_STRCPY(config->daemon_password, "coolAdmin");
  }
}

/**
 * @brief Set paths default values.
 */
static void set_paths_defaults(Config *config) {
  if (config->paths_images[0] == '\0') {
    SAFE_STRCPY(config->paths_images, "/opt/coolerdash/images");
  }
  if (config->paths_image_coolerdash[0] == '\0') {
    SAFE_STRCPY(config->paths_image_coolerdash, "/tmp/coolerdash.png");
  }
  if (config->paths_image_shutdown[0] == '\0') {
    SAFE_STRCPY(config->paths_image_shutdown,
                "/opt/coolerdash/images/shutdown.png");
  }
  if (config->paths_pid[0] == '\0') {
    SAFE_STRCPY(config->paths_pid, "/tmp/coolerdash.pid");
  }
}

/**
 * @brief Try to set display dimensions from LCD device.
 */
static void try_set_lcd_dimensions(Config *config) {
  if (config->display_width != 0 && config->display_height != 0)
    return;

  int lcd_width = 0, lcd_height = 0;
  if (!get_liquidctl_data(config, NULL, 0, NULL, 0, &lcd_width, &lcd_height))
    return;

  if (lcd_width <= 0 || lcd_height <= 0)
    return;

  if (config->display_width == 0)
    config->display_width = lcd_width;
  if (config->display_height == 0)
    config->display_height = lcd_height;
}

/**
 * @brief Set display default values with LCD device fallback.
 */
static void set_display_defaults(Config *config) {
  try_set_lcd_dimensions(config);

  if (config->display_refresh_interval == 0.0f)
    config->display_refresh_interval = 2.50f; // Default: 2.5 seconds
  if (config->lcd_brightness == 0)
    config->lcd_brightness = 80;
  if (!is_valid_orientation(config->lcd_orientation))
    config->lcd_orientation = 0;
  if (config->display_shape[0] == '\0')
    cc_safe_strcpy(config->display_shape, sizeof(config->display_shape),
                   "auto");
  if (config->display_mode[0] == '\0')
    cc_safe_strcpy(config->display_mode, sizeof(config->display_mode), "dual");
  if (config->circle_switch_interval == 0)
    config->circle_switch_interval = 5; // Default: 5 seconds
  if (config->display_content_scale_factor == 0.0f)
    config->display_content_scale_factor = 0.98f; // Default: 98% (2% margin)
  if (config->display_inscribe_factor < 0.0f)
    config->display_inscribe_factor =
        0.70710678f; // Default: 1/√2 ≈ 0.7071 (geometric inscribe)
}

/**
 * @brief Set layout default values.
 */
static void set_layout_defaults(Config *config) {
  if (config->layout_bar_width == 0)
    config->layout_bar_width = 98; // Default: 98% width (1% margin left+right)
  if (config->layout_label_margin_left == 0)
    config->layout_label_margin_left = 1; // Default: 1% from left edge
  if (config->layout_label_margin_bar == 0)
    config->layout_label_margin_bar = 1; // Default: 1% from bars
  if (config->layout_bar_height == 0)
    config->layout_bar_height = 20;
  if (config->layout_bar_gap == 0)
    config->layout_bar_gap = 10.0f;
  if (config->layout_bar_border == 0.0f)
    config->layout_bar_border = 1.0f;
}

/**
 * @brief Set display positioning default values.
 */
static void set_display_positioning_defaults(Config *config) {
  if (config->display_temp_offset_x_cpu == 0)
    config->display_temp_offset_x_cpu = -9999;
  if (config->display_temp_offset_x_gpu == 0)
    config->display_temp_offset_x_gpu = -9999;
  if (config->display_temp_offset_y_cpu == 0)
    config->display_temp_offset_y_cpu = -9999;
  if (config->display_temp_offset_y_gpu == 0)
    config->display_temp_offset_y_gpu = -9999;
  if (config->display_temp_offset_x_liquid == 0)
    config->display_temp_offset_x_liquid = -9999;
  if (config->display_temp_offset_y_liquid == 0)
    config->display_temp_offset_y_liquid = -9999;
  if (config->display_degree_spacing == 0)
    config->display_degree_spacing = 16; // Default: 16px spacing
  if (config->display_label_offset_x == 0)
    config->display_label_offset_x = -9999;
  if (config->display_label_offset_y == 0)
    config->display_label_offset_y = -9999;
}

/**
 * @brief Set font default values with dynamic scaling.
 */
static void set_font_defaults(Config *config) {
  if (config->font_face[0] == '\0')
    SAFE_STRCPY(config->font_face, "Roboto Black");

  if (config->font_size_temp == 0.0f) {
    const double base_resolution = 240.0;
    const double base_font_size_temp = 100.0;
    const double scale_factor =
        ((double)config->display_width + (double)config->display_height) /
        (2.0 * base_resolution);
    config->font_size_temp = (float)(base_font_size_temp * scale_factor);

    log_message(
        LOG_INFO,
        "Font size (temp) auto-scaled: %.1f (display: %dx%d, scale: %.2f)",
        config->font_size_temp, config->display_width, config->display_height,
        scale_factor);
  }

  if (config->font_size_labels == 0.0f) {
    const double base_resolution = 240.0;
    const double base_font_size_labels = 30.0;
    const double scale_factor =
        ((double)config->display_width + (double)config->display_height) /
        (2.0 * base_resolution);
    config->font_size_labels = (float)(base_font_size_labels * scale_factor);

    log_message(
        LOG_INFO,
        "Font size (labels) auto-scaled: %.1f (display: %dx%d, scale: %.2f)",
        config->font_size_labels, config->display_width, config->display_height,
        scale_factor);
  }

  set_display_positioning_defaults(config);
}

/**
 * @brief Set temperature defaults.
 */
static void set_temperature_defaults(Config *config) {
  if (config->temp_threshold_1 == 0.0f)
    config->temp_threshold_1 = 55.0f;
  if (config->temp_threshold_2 == 0.0f)
    config->temp_threshold_2 = 65.0f;
  if (config->temp_threshold_3 == 0.0f)
    config->temp_threshold_3 = 75.0f;
  if (config->temp_max_scale == 0.0f)
    config->temp_max_scale = 115.0f;

  // Liquid temperature defaults (Range: 0-50°C for AIO coolers)
  if (config->temp_liquid_max_scale == 0.0f)
    config->temp_liquid_max_scale = 50.0f;
  if (config->temp_liquid_threshold_1 == 0.0f)
    config->temp_liquid_threshold_1 = 25.0f;
  if (config->temp_liquid_threshold_2 == 0.0f)
    config->temp_liquid_threshold_2 = 28.0f;
  if (config->temp_liquid_threshold_3 == 0.0f)
    config->temp_liquid_threshold_3 = 31.0f;
}

/**
 * @brief Check if color is unset.
 */
static inline int is_color_unset(const Color *color) {
  return (color->r == 0 && color->g == 0 && color->b == 0);
}

/**
 * @brief Color default configuration entry.
 */
typedef struct {
  Color *color_ptr;
  uint8_t r, g, b;
} ColorDefault;

/**
 * @brief Set color default values.
 */
static void set_color_defaults(Config *config) {
  ColorDefault color_defaults[] = {
      {&config->layout_bar_color_background, 52, 52, 52},
      {&config->layout_bar_color_border, 192, 192, 192},
      {&config->font_color_temp, 255, 255, 255},
      {&config->font_color_label, 200, 200, 200},
      {&config->temp_threshold_1_bar, 0, 255, 0},
      {&config->temp_threshold_2_bar, 255, 140, 0},
      {&config->temp_threshold_3_bar, 255, 70, 0},
      {&config->temp_threshold_4_bar, 255, 0, 0},
      {&config->temp_liquid_threshold_1_bar, 0, 255, 0},
      {&config->temp_liquid_threshold_2_bar, 255, 140, 0},
      {&config->temp_liquid_threshold_3_bar, 255, 70, 0},
      {&config->temp_liquid_threshold_4_bar, 255, 0, 0}};

  const size_t color_count = sizeof(color_defaults) / sizeof(color_defaults[0]);
  for (size_t i = 0; i < color_count; i++) {
    if (is_color_unset(color_defaults[i].color_ptr)) {
      color_defaults[i].color_ptr->r = color_defaults[i].r;
      color_defaults[i].color_ptr->g = color_defaults[i].g;
      color_defaults[i].color_ptr->b = color_defaults[i].b;
    }
  }
}

/**
 * @brief Apply system default values for missing fields.
 * @details Public function - sets fallback values for all unset configuration
 * fields.
 */
void apply_system_defaults(Config *config) {
  if (!config)
    return;

  set_daemon_defaults(config);
  set_paths_defaults(config);
  set_display_defaults(config);
  set_layout_defaults(config);
  set_font_defaults(config);
  set_temperature_defaults(config);
  set_color_defaults(config);
}

/**
 * @brief Initialize config structure with system defaults.
 * @details Public function - clears memory and applies all system default
 * values.
 */
void init_system_defaults(Config *config) {
  if (!config)
    return;
  memset(config, 0, sizeof(Config));
  // Set explicit unset sentinel for display_inscribe_factor so 0.0 can be used
  // as auto by user
  config->display_inscribe_factor = -1.0f;
}
