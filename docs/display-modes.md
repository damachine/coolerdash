# CoolerDash Display Modes - Developer Guide

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Dual Mode](#dual-mode)
- [Circle Mode](#circle-mode)
- [Configuration System](#configuration-system)
- [Rendering Pipeline](#rendering-pipeline)
- [Display Detection](#display-detection)
- [Adding New Modes](#adding-new-modes)

---

## Overview

CoolerDash supports two distinct display modes for rendering temperature information on LCD screens:

- **Dual Mode** (`dual`): Simultaneous display of CPU and GPU temperatures with side-by-side layout
- **Circle Mode** (`circle`): Alternating single-sensor display optimized for circular high-resolution screens

### Mode Selection

The mode is selected through a three-tier configuration system:

1. **System Default** (`src/device/sys.c`): `display_mode = "dual"`
2. **User Configuration** (`etc/coolerdash/config.ini`): `[display] mode=dual|circle`
3. **CLI Override** (`src/main.c`): `--dual` or `--circle` flags

Priority: **CLI > INI > Default**

---

## Architecture

### File Structure

```
src/mods/
├── dual.c          # Dual mode implementation
├── dual.h          # Dual mode API
├── circle.c        # Circle mode implementation
└── circle.h        # Circle mode API
```

### Mode Dispatcher

The mode dispatcher is located in `src/mods/dual.c`:

```c
void draw_combined_image(const struct Config *config)
{
    if (strcmp(config->display_mode, "circle") == 0) {
        draw_circle_image(config);
    } else {
        draw_dual_image(config);
    }
}
```

This function is called by the main monitoring loop and routes to the appropriate mode implementation.

---

## Dual Mode

### Purpose
Displays CPU and GPU temperatures simultaneously in a side-by-side layout.

### Implementation File
`src/mods/dual.c` (602 lines)

### Key Components

#### 1. ScalingParams Structure
```c
typedef struct {
    double scale_x;              // Horizontal scaling factor
    double scale_y;              // Vertical scaling factor
    double corner_radius;        // Bar corner radius
    double inscribe_factor;      // 1.0 (rectangular) or M_SQRT1_2 (circular)
    int safe_bar_width;          // Safe bar width for circular displays
    double safe_content_margin;  // Horizontal margin
    int is_circular;             // Display type flag
} ScalingParams;
```

#### 2. Core Functions

##### `calculate_scaling_params()`
Calculates dynamic scaling based on display dimensions:
- Base resolution: 240x240px
- Checks `shape` config override (rectangular/circular/auto)
- Falls back to device detection for circular displays
- Applies inscribe factor (1/√2 ≈ 0.7071) for circular displays
- **Uses configurable `content_scale_factor`** (0.5-1.0, default: 0.98) to determine safe area percentage
- Calculates safe content area to avoid edge clipping

**Priority system:**
1. `shape` config parameter (manual override)
2. `force_display_circular` flag (deprecated)
3. Automatic device detection (default)

**Content Scale Configuration:**
```ini
[display]
content_scale_factor=0.98  # 0.5-1.0, default: 0.98 (2% margin)
```

**Scale Factor Impact:**
- **0.98-1.0:** Minimal margins, maximum screen usage
- **0.90-0.97:** Comfortable padding, safe text rendering
- **0.70-0.89:** Extra margins, conservative layout
- **0.5-0.69:** Large margins, centered content focus

##### `draw_dual_bars()`
Renders CPU and GPU temperature bars:
- Left bar: CPU temperature
- Right bar: GPU temperature
- Color-coded based on temperature thresholds
- Rounded corners with configurable radius
- Border and background colors from config

##### `draw_temperature_labels()`
Renders temperature values and labels:
- Displays numeric temperature with degree symbol
- Draws "CPU" and "GPU" labels
- Uses configurable fonts and colors
- Dynamically positions based on scaling parameters

##### `render_dual_display()`
Main rendering function:
- Creates Cairo surface and context
- Draws black background
- Calls component rendering functions
- Writes PNG to filesystem
- Uploads image to LCD device

### Layout Algorithm

```
┌────────────────────────────┐
│     CPU: 45°    GPU: 52°   │  ← Temperature values
│     ┌─────┐     ┌─────┐    │
│     │█████│     │██████│   │  ← Color-coded bars
│     └─────┘     └─────┘    │
│      CPU         GPU        │  ← Labels
└────────────────────────────┘
```

### Circular Display Handling

For circular displays, dual mode applies an inscribe factor:
- Safe area width = `display_width × M_SQRT1_2 × 0.98`
- Horizontal centering with margin calculation
- Prevents content from clipping at display edges

---

## Circle Mode

### Purpose
Alternates between CPU and GPU display every 5 seconds, optimized for high-resolution circular screens where dual mode scaling is not optimal.

### Implementation File
`src/mods/circle.c` (456 lines)

### Key Components

#### 1. SensorMode Enumeration
```c
typedef enum {
    SENSOR_CPU = 0,
    SENSOR_GPU = 1
} SensorMode;
```

#### 2. Global State Management
```c
static SensorMode current_sensor = SENSOR_CPU;
static time_t last_switch_time = 0;
```

#### 3. Configuration-Based Timing

The sensor switch interval is **configurable** via `config.ini`:

```c
// No longer hardcoded - uses Config parameter
void update_sensor_mode(const struct Config *config)
{
    time_t current_time = time(NULL);
    
    // Use circle_switch_interval from config (default: 5 seconds)
    if (difftime(current_time, last_switch_time) >= config->circle_switch_interval) {
        current_sensor = (current_sensor == SENSOR_CPU) ? SENSOR_GPU : SENSOR_CPU;
        last_switch_time = current_time;
    }
}
```

**Configuration:**
```ini
[display]
mode=circle
circle_switch_interval=5  # 1-60 seconds, default: 5
```

**Use Cases:**
- **Fast (1-3s):** Quick sensor overview
- **Moderate (5-8s):** Balanced viewing (default: 5s)
- **Slow (10-60s):** Focus on individual sensors

#### 4. Core Functions

##### `update_sensor_mode(const struct Config *config)`
Manages sensor alternation:
- Uses `time()` with configurable interval from `config->circle_switch_interval`
- Toggles between CPU and GPU
- Logs sensor switches for debugging

##### `draw_single_sensor()`
Renders single sensor display:
- **Bar-Centered Layout**: Uses temperature bar as central reference point
- **Temperature Positioning**: 55% of display height above bar
- **Label Positioning**: 5% of display height below bar
- **Centering Algorithm**: Calculates total width including degree symbol for proper visual balance

##### `calculate_temp_fill_width()`
Calculates temperature bar fill:
- Bounds-checked ratio calculation
- Uses unified `temp_max_scale` from config
- Returns pixel width for bar fill

##### `get_temperature_bar_color()`
Determines bar color based on thresholds:
- `temp_threshold_1`: Lowest (blue/green)
- `temp_threshold_2`: Medium-low (yellow)
- `temp_threshold_3`: Medium-high (orange)
- `temp_threshold_4`: Critical (red)

##### `render_circle_display()`
Main rendering function:
- Creates Cairo surface and context
- Updates sensor mode (checks 5s interval)
- Renders current sensor
- Writes PNG and uploads to LCD

### Layout Algorithm

```
┌────────────────┐
│                │
│      45°       │  ← Temperature (55% above bar)
│                │
│    ┌──────┐    │  ← Temperature bar (centered)
│    │██████│    │
│    └──────┘    │
│                │
│      CPU       │  ← Label (5% below bar)
│                │
└────────────────┘
```

### Centering Algorithm

Temperature centering includes degree symbol width:
```c
// Calculate degree symbol width
cairo_text_extents_t degree_ext;
cairo_text_extents(cr, "°", &degree_ext);

// Total width = temperature + spacing + degree
const double total_width = temp_ext.width + 5 + degree_ext.width;

// Center as a unit
double temp_x = (config->display_width - total_width) / 2.0;
```

This ensures visual balance by treating "45°" as a single unit rather than centering only the number.

### Timing Implementation

The sensor switching interval is **configurable** via `config.ini` (1-60 seconds, default: 5):

```c
void update_sensor_mode(const struct Config *config)
{
    time_t current_time = time(NULL);
    
    // Use configurable interval from config.ini
    if (difftime(current_time, last_switch_time) >= config->circle_switch_interval) {
        current_sensor = (current_sensor == SENSOR_CPU) ? SENSOR_GPU : SENSOR_CPU;
        last_switch_time = current_time;
    }
}
```

**Configuration:**
```ini
[display]
mode=circle
circle_switch_interval=5  # 1-60 seconds, default: 5
```

**Note**: For sub-second precision, `nanosleep()` or `clock_gettime()` could be used, but the current implementation provides sufficient accuracy for display purposes.

---

## Configuration System

### Config Structure Extension

The `Config` structure in `src/device/sys.h` includes:
```c
char display_mode[16];  // "dual" or "circle"
```

### System Defaults (`src/device/sys.c`)

```c
void set_display_defaults(struct Config *config)
{
    // ... other defaults ...
    cc_safe_strcpy(config->display_mode, "dual", sizeof(config->display_mode));
}
```

### INI Parsing (`src/device/usr.c`)

```c
static int handle_display_mode(void *user, const char *section,
                               const char *name, const char *value)
{
    struct Config *config = (struct Config *)user;
    
    if (strcmp(section, "display") == 0 && strcmp(name, "mode") == 0) {
        if (strcmp(value, "dual") == 0 || strcmp(value, "circle") == 0) {
            cc_safe_strcpy(config->display_mode, value, 
                          sizeof(config->display_mode));
            return 1;
        }
    }
    return 0;
}
```

### CLI Override (`src/main.c`)

```c
static void parse_arguments(int argc, char *argv[], 
                           char *display_mode_override, size_t override_size)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dual") == 0) {
            cc_safe_strcpy(display_mode_override, "dual", override_size);
        } else if (strcmp(argv[i], "--circle") == 0) {
            cc_safe_strcpy(display_mode_override, "circle", override_size);
        }
    }
}

int main(int argc, char *argv[])
{
    char display_mode_override[16] = {0};
    parse_arguments(argc, argv, display_mode_override, sizeof(display_mode_override));
    
    if (display_mode_override[0] != '\0') {
        cc_safe_strcpy(config.display_mode, display_mode_override, 
                      sizeof(config.display_mode));
    }
}
```

---

## Rendering Pipeline

### 1. Main Loop (`src/main.c`)
```c
while (1) {
    draw_combined_image(&config);  // Mode dispatcher
    sleep(update_interval);
}
```

### 2. Mode Dispatcher (`src/mods/dual.c`)
```c
void draw_combined_image(const struct Config *config)
{
    if (strcmp(config->display_mode, "circle") == 0) {
        draw_circle_image(config);
    } else {
        draw_dual_image(config);
    }
}
```

### 3. Mode-Specific Rendering

#### Dual Mode Flow:
1. `draw_dual_image()` → Entry point
2. `get_liquidctl_data()` → Device information
3. `get_temperature_monitor_data()` → Sensor data
4. `render_dual_display()` → Cairo rendering
5. `cairo_surface_write_to_png()` → PNG generation
6. `send_image_to_lcd()` → LCD upload

#### Circle Mode Flow:
1. `draw_circle_image()` → Entry point
2. `get_liquidctl_data()` → Device information
3. `get_temperature_monitor_data()` → Sensor data
4. `update_sensor_mode()` → Check 5s interval
5. `render_circle_display()` → Cairo rendering
6. `cairo_surface_write_to_png()` → PNG generation
7. `send_image_to_lcd()` → LCD upload

### 4. Common Cairo Operations

Both modes use identical Cairo workflow:
```c
// 1. Create surface
cairo_surface_t *surface = cairo_image_surface_create(
    CAIRO_FORMAT_ARGB32, width, height);

// 2. Create context
cairo_t *cr = cairo_create(surface);

// 3. Draw content
cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);  // Black background
cairo_paint(cr);
// ... mode-specific rendering ...

// 4. Write PNG
cairo_surface_write_to_png(surface, path);

// 5. Cleanup
cairo_destroy(cr);
cairo_surface_destroy(surface);
```

---

## Display Detection

### Device Name Detection

Both modes use `is_circular_display_device()` to detect circular displays:

```c
int is_circular = is_circular_display_device(device_name, width, height);
```

**Known Circular Devices**:
- NZXT Kraken Elite (240x240)
- Corsair iCUE LINK (Other models to be added)

### Display Shape Override (New in v1.96)

**Recommended Method:** Manual configuration override in `config.ini`:
```ini
[display]
# Display shape override (auto, rectangular, circular)
# - auto: Auto-detection based on device database (default)
# - rectangular: Force inscribe_factor=1.0 (full width)
# - circular: Force inscribe_factor=0.7071 (inscribed circle)
shape = auto
```

**Legacy Method (Deprecated):** CLI flag for backwards compatibility:
```ini
[display]
force_circular = true
```

**Priority System:**
1. `shape` config parameter (highest - manual override)
2. `force_display_circular` flag (legacy compatibility)
3. Automatic device detection (default)

**Use cases:**
- Testing circular layout on rectangular displays
- Fixing incorrect auto-detection results
- Devices not in the detection database
- Custom/modded hardware

### Inscribe Factor Application

For circular displays:
```c
if (is_circular) {
    params->inscribe_factor = M_SQRT1_2;  // 1/√2 ≈ 0.7071
    safe_area_width = display_width * inscribe_factor;
} else {
    params->inscribe_factor = 1.0;
    safe_area_width = display_width;
}
```

This ensures content fits within the visible circular area without clipping.

---

## Adding New Modes

### Step-by-Step Guide

#### 1. Create Mode Files

Create `src/mods/newmode.c` and `src/mods/newmode.h`:

```c
// newmode.h
#ifndef NEWMODE_H
#define NEWMODE_H

#include "../device/sys.h"

void draw_newmode_image(const struct Config *config);

#endif
```

#### 2. Implement Rendering

```c
// newmode.c
#include "newmode.h"
#include <cairo/cairo.h>

void draw_newmode_image(const struct Config *config)
{
    // 1. Get device info
    char device_uid[128] = {0};
    char device_name[128] = {0};
    int screen_width = 0, screen_height = 0;
    
    if (!get_liquidctl_data(config, device_uid, sizeof(device_uid),
                           device_name, sizeof(device_name),
                           &screen_width, &screen_height)) {
        return;
    }
    
    // 2. Get sensor data
    monitor_sensor_data_t data = {0};
    if (!get_temperature_monitor_data(config, &data)) {
        return;
    }
    
    // 3. Create Cairo context
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, config->display_width, config->display_height);
    cairo_t *cr = cairo_create(surface);
    
    // 4. Draw your custom content
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    // ... your rendering code ...
    
    // 5. Write PNG
    cairo_surface_write_to_png(surface, config->paths_image_coolerdash);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    // 6. Upload to LCD
    send_image_to_lcd(config, config->paths_image_coolerdash, device_uid);
}
```

#### 3. Update Mode Dispatcher

In `src/mods/dual.c`, add your mode:

```c
#include "newmode.h"

void draw_combined_image(const struct Config *config)
{
    if (strcmp(config->display_mode, "newmode") == 0) {
        draw_newmode_image(config);
    } else if (strcmp(config->display_mode, "circle") == 0) {
        draw_circle_image(config);
    } else {
        draw_dual_image(config);
    }
}
```

#### 4. Update Makefile

```makefile
SRC_MODULES = src/mods/dual.c src/mods/circle.c src/mods/newmode.c
HEADERS = ... src/mods/newmode.h
```

#### 5. Update Configuration

Add validation in `src/device/usr.c`:

```c
if (strcmp(value, "dual") == 0 || 
    strcmp(value, "circle") == 0 ||
    strcmp(value, "newmode") == 0) {
    cc_safe_strcpy(config->display_mode, value, sizeof(config->display_mode));
    return 1;
}
```

#### 6. Add CLI Flag

In `src/main.c`:

```c
// In parse_arguments()
else if (strcmp(argv[i], "--newmode") == 0) {
    cc_safe_strcpy(display_mode_override, "newmode", override_size);
}

// In show_help()
printf("  --newmode           Use new display mode\n");
```

#### 7. Update Documentation

- `README.md`: Add mode description
- `etc/coolerdash/config.ini`: Add example
- `docs/display-modes.md`: Add technical details

---

## Best Practices

### 1. Cairo Resource Management

Always pair creation with destruction:
```c
cairo_t *cr = cairo_create(surface);
// ... use cr ...
cairo_destroy(cr);  // Must call

cairo_surface_t *surface = cairo_image_surface_create(...);
// ... use surface ...
cairo_surface_destroy(surface);  // Must call
```

### 2. Configuration Validation

Validate all config values before use:
```c
if (config->display_width <= 0 || config->display_height <= 0) {
    log_message(LOG_ERROR, "Invalid display dimensions");
    return;
}
```

### 3. Error Handling

Check all Cairo operations:
```c
if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    log_message(LOG_ERROR, "Surface creation failed");
    return;
}
```

### 4. Logging

Use appropriate log levels:
- `LOG_ERROR`: Critical failures
- `LOG_WARNING`: Recoverable issues
- `LOG_INFO`: Mode switches, state changes
- `LOG_STATUS`: Successful operations
- `LOG_DEBUG`: Detailed tracing

### 5. Null Checks

Always validate pointers:
```c
if (!cr || !config || !data) {
    log_message(LOG_ERROR, "Null parameter");
    return;
}
```

### 6. Scaling Calculations

Use dynamic scaling for resolution independence:
```c
ScalingParams params = calculate_scaling_params(config);

// Use configurable content_scale_factor (default: 0.98)
double content_scale = (config->display_content_scale_factor > 0.5 && 
                       config->display_content_scale_factor <= 1.0) ?
                       config->display_content_scale_factor : 0.98;

// Apply inscribe factor for circular displays
double inscribe = params.inscribe_factor;  // 1.0 or M_SQRT1_2 (≈0.7071)

// Calculate safe area
double safe_width = config->display_width * content_scale * inscribe;
double safe_height = config->display_height * content_scale * inscribe;
```

**Configuration:**
```ini
[display]
content_scale_factor=0.98  # 0.5-1.0, controls margin/padding
```
```c
const double scale_avg = (scale_x + scale_y) / 2.0;
const double corner_radius = 8.0 * scale_avg;
```

---

## Performance Considerations

### 1. Cairo Surface Reuse
Consider reusing surfaces for reduced memory allocation:
```c
static cairo_surface_t *cached_surface = NULL;

if (!cached_surface) {
    cached_surface = cairo_image_surface_create(...);
}
```

### 2. PNG Compression
Cairo uses default PNG compression. For faster writes:
```c
// Trade file size for speed (not currently implemented)
// Could add custom PNG write with lower compression
```

### 3. LCD Upload Frequency
Respect device limits:
- NZXT Kraken: ~1-2 FPS recommended
- Adjust `update_interval` based on device

### 4. Circle Mode Timing
Current implementation uses `time()` for 5s intervals:
- Granularity: 1 second
- Suitable for display purposes
- For sub-second precision, consider `clock_gettime()`

---

## Debugging Tips

### 1. Enable Debug Logging

Compile with `-DDEBUG`:
```bash
gcc -g -O0 -DDEBUG src/*.c -o coolerdash -lcurl -lcairo -lm -ljansson -linih
```

### 2. Test PNG Output

Check generated PNG manually:
```bash
feh /tmp/coolerdash.png
```

### 3. Mode Verification

Log current mode on startup:
```c
log_message(LOG_INFO, "Display mode: %s", config->display_mode);
```

### 4. Circle Mode State

Monitor sensor switches:
```c
log_message(LOG_INFO, "Circle mode: switched to %s", 
            current_sensor == SENSOR_CPU ? "CPU" : "GPU");
```

### 5. Cairo Status

Always check status after operations:
```c
cairo_status_t status = cairo_status(cr);
if (status != CAIRO_STATUS_SUCCESS) {
    log_message(LOG_ERROR, "Cairo error: %s", cairo_status_to_string(status));
}
```

---

## Future Enhancements

### Potential Features

1. **Graph Mode**: Temperature history graphs
2. **Minimal Mode**: Single temperature value only
3. **Animation Mode**: Smooth transitions between values
4. **Custom Layouts**: User-defined positioning via config
5. **Multi-Sensor**: Support for more than 2 sensors
6. **Themes**: Predefined color schemes

### Implementation Considerations

- Mode plugin architecture for runtime loading
- JSON configuration for complex layouts
- Animation frame buffering
- Custom font support (TTF loading)
- Image overlays (logos, backgrounds)

---

## Conclusion

CoolerDash's display mode architecture provides a flexible framework for LCD rendering. The dual and circle modes demonstrate different approaches to the same problem, each optimized for specific use cases.

Key takeaways:
- **Dual Mode**: Best for rectangular displays or when both sensors need simultaneous visibility
- **Circle Mode**: Optimal for high-resolution circular displays where space is limited
- **Mode System**: Three-tier configuration with CLI override capability
- **Cairo Integration**: Consistent rendering pipeline across all modes
- **Extensibility**: Clear pattern for adding new modes

For questions or contributions, see `CONTRIBUTING.md`.

---

**Version**: 1.0  
**Last Updated**: November 6, 2025  
**Authors**: damachine <christkue79@gmail.com>
