/**
 * @author damachine (christkue79@gmail.com)
 * @Maintainer: damachine <christkue79@gmail.com>
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 2.0
 *    This software is provided "as is", without warranty of any kind, express
 * or implied. I do not guarantee that it will work as intended on your system.
 */

/**
 * @brief Display mode dispatcher implementation.
 * @details Routes rendering requests to appropriate mode-specific modules based
 * on configuration.
 */

// Define POSIX constants
#define _POSIX_C_SOURCE 200112L

// Include necessary headers
// cppcheck-suppress-begin missingIncludeSystem
#include <string.h>
// cppcheck-suppress-end missingIncludeSystem

// Include project headers
#include "../device/sys.h"
#include "circle.h"
#include "display.h"
#include "dual.h"

/**
 * @brief Main display dispatcher - routes to appropriate rendering mode.
 * @details Examines display_mode configuration and dispatches to either dual or
 * circle renderer. This provides a clean separation between mode selection
 * logic and mode-specific rendering.
 * @param config Configuration containing display mode and rendering parameters
 */
void draw_display_image(const struct Config *config) {
  if (!config) {
    log_message(LOG_ERROR, "Invalid config parameter for draw_display_image");
    return;
  }

  // Check display mode and dispatch to appropriate renderer
  if (config->display_mode[0] != '\0' &&
      strcmp(config->display_mode, "circle") == 0) {
    // Circle mode: alternating single sensor display
    draw_circle_image(config);
  } else {
    // Dual mode (default): simultaneous CPU+GPU display
    draw_dual_image(config);
  }
}
