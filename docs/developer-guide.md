# CoolerDash Developer Documentation

**Language:** C99 | **Platform:** Linux x86-64-v3 | **License:** MIT  
**Author:** damachine (damachin3@proton.me)  
**Repository:** https://github.com/damachine/coolerdash

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Module Structure](#module-structure)
4. [Build System](#build-system)
5. [Configuration System](#configuration-system)
6. [API Integration](#api-integration)
7. [Rendering Pipeline](#rendering-pipeline)
8. [Function Reference](#function-reference)
9. [Development Guidelines](#development-guidelines)
10. [Testing & Debugging](#testing--debugging)

---

## Project Overview

### Purpose

CoolerDash extends the LCD functionality of [CoolerControl](https://gitlab.com/coolercontrol/coolercontrol) for Linux systems, specifically targeting AIO liquid cooler LCD displays (NZXT Kraken, etc.). It provides real-time sensor visualization with customizable UI elements.

### Key Features

- **Real-time Temperature Monitoring:** CPU/GPU sensor data via CoolerControl REST API
- **Adaptive Display Rendering:** Automatic circular/rectangular display detection
- **Customizable UI:** Full color/layout/font configuration via INI file
- **Secure Session Management:** HTTP Basic Auth with cookie-based session persistence
- **Efficient Caching:** One-time device information retrieval at startup
- **Systemd Integration:** Native service management with automatic startup

### System Requirements

- **OS:** Linux (systemd-based)
- **Architecture:** x86-64-v3 (Intel Haswell+ / AMD Excavator+)
- **Dependencies:**
  - `cairo` — PNG generation
  - `jansson` — JSON parsing (config + API)
  - `libcurl-gnutls` — HTTP client
  - `ttf-roboto` — Font rendering
- **Required Service:** CoolerControl >=3.x (must be running)

---

## Architecture

### High-Level Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                         main.c                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ 1. Configuration Loading (device/config.c)                │  │
│  │ 2. Session Initialization (cc_main.c)                     │  │
│  │ 3. Device Cache Setup (cc_conf.c)                         │  │
│  │ 4. Main Loop (configurable interval)                      │  │
│  │    ├─ Temperature Reading (cc_sensor.c)                   │  │
│  │    ├─ Image Rendering (display.c → dual.c|circle.c)       │  │
│  │    └─ LCD Upload (cc_main.c)                              │  │
│  │ 5. Signal Handling (SIGTERM/SIGINT → shutdown image)      │  │
│  │ 6. Cleanup (session + image files)                        │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
              ↓                    ↓                    ↓
    ┌─────────────────┐  ┌──────────────────┐  ┌─────────────────┐
    │  device/        │  │  srv/            │  │  mods/          │
    │  Configuration  │  │  CoolerControl   │  │  Rendering      │
    │  Management     │  │  API Client      │  │  Engine         │
    └─────────────────┘  └──────────────────┘  └─────────────────┘
         config.c/h           cc_main.c/h           display.c/h
                              cc_conf.c/h            dual.c/h
                              cc_sensor.c/h          circle.c/h
```

### Design Principles

1. **Separation of Concerns:** Clear module boundaries (device/srv/mods)
2. **Defense in Depth:** Multi-layer error checking and validation
3. **Resource Safety:** RAII-style cleanup patterns with cleanup guards
4. **Performance Optimization:** Device caching, exponential buffer growth
5. **Security First:** PID file race condition prevention, secure parsing

---

## Module Structure

### Directory Layout

```
coolerdash/
├── src/
│   ├── main.c                    # Main daemon entry point (967 lines, 30 functions)
│   ├── device/                   # Configuration subsystem
│   │   └── config.c/h           # JSON config loader + defaults
│   ├── srv/                     # CoolerControl API client
│   │   ├── cc_main.c/h          # Session management
│   │   ├── cc_conf.c/h          # Device cache, display detection
│   │   └── cc_sensor.c/h        # Temperature monitoring
│   └── mods/                    # Rendering modules
│       ├── display.c/h          # Mode dispatcher, shared helpers
│       ├── dual.c/h             # Dual mode (CPU+GPU simultaneous)
│       └── circle.c/h           # Circle mode (alternating sensor)
├── etc/
│   ├── coolercontrol/plugins/coolerdash/config.json  # User configuration
│   └── systemd/coolerdash.service
├── docs/                        # Documentation
│   ├── config.md                # Configuration guide
│   ├── devices.md               # Supported hardware
│   └── developer-guide.md       # This file
├── Makefile                     # Build system
└── PKGBUILD                     # Arch/AUR packaging
```

### Module Responsibilities

| Module | Purpose | Key Functions | Lines | Public API |
|--------|---------|---------------|-------|------------|
| **main.c** | Daemon lifecycle | Signal handling, PID management, main loop | — | `main()` |
| **device/config** | Config system | JSON loading, hardcoded defaults | — | `load_plugin_config()` |
| **srv/cc_main** | HTTP session | Login, LCD upload, cleanup | — | 4 functions |
| **srv/cc_conf** | Device cache | UID/name/dimensions, display shape | — | 4 functions |
| **srv/cc_sensor** | Temperature | CPU/GPU sensor reading | — | 1 function |
| **mods/display** | Mode dispatch | Route to dual/circle, shared Cairo helpers | — | `draw_display_image()` |
| **mods/dual** | Dual rendering | CPU+GPU simultaneous layout | — | `draw_dual_image()` |
| **mods/circle** | Circle rendering | Alternating single-sensor layout | — | `draw_circle_image()` |

---

## Build System

### Makefile Overview

**Location:** `/Makefile`  
**Build Tool:** GNU Make + GCC  
**Standard:** C99 with strict warnings

#### Key Targets

```bash
make                # Standard build (C99, -O2, -march=x86-64-v3)
make clean          # Remove build artifacts (build/, bin/)
make install        # System installation with dependency checks
make uninstall      # Complete removal (service + files)
make debug          # Debug build (-g -DDEBUG -fsanitize=address)
make help           # Show all available targets
```

#### Compiler Flags

```makefile
CFLAGS = -Wall -Wextra -O2 -std=c99 -march=x86-64-v3 -Iinclude \
         $(shell pkg-config --cflags cairo jansson libcurl)
LIBS = $(shell pkg-config --libs cairo jansson libcurl) -lm
```

**Optimization Level:** `-O2` (production), `-O0` (debug)  
**Architecture:** `x86-64-v3` (AVX2/BMI2 support)  
**Warnings:** All enabled (`-Wall -Wextra`)

#### Build Workflow

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Dependency Detection (pkg-config)                        │
│    ├─ cairo (graphics)                                      │
│    ├─ jansson (JSON)                                        │
│    ├─ libcurl (HTTP)                                        │
│    └─ jansson (JSON parsing)                                 │
├─────────────────────────────────────────────────────────────┤
│ 2. Module Compilation (src/ → build/)                       │
│    ├─ device/config.o                                       │
│    ├─ srv/cc_main.o, srv/cc_conf.o, srv/cc_sensor.o        │
│    └─ mods/display.o, mods/dual.o, mods/circle.o           │
├─────────────────────────────────────────────────────────────┤
│ 3. Linking (main.c + modules → bin/coolerdash)             │
├─────────────────────────────────────────────────────────────┤
│ 4. Installation (make install)                              │
│    ├─ Binary: /usr/libexec/coolerdash/coolerdash            │
│    ├─ Config: /etc/coolercontrol/plugins/coolerdash/config.json │
│    ├─ Plugin UI: /etc/coolercontrol/plugins/coolerdash/ui/  │
│    └─ Manual: /usr/share/man/man1/coolerdash.1             │
└─────────────────────────────────────────────────────────────┘
```

### Dependency Management

**Automatic Detection:** Makefile checks for missing dependencies via `pkg-config`  
**Distribution Support:** Arch, Debian, Fedora, RHEL, openSUSE  
**Fallback Behavior:** Build fails with clear error messages if deps missing

---

## Configuration System

### Two-Stage Configuration Loading

```
┌──────────────────────────────────────────────────────────────────┐
│ Stage 1: Hardcoded Defaults (device/config.c)                    │
│ ─────────────────────────────────────────────────────────────    │
│ All Config fields initialized to built-in defaults              │
│ Function: set_*_defaults(Config *config)                         │
│ Example: daemon_address = "http://localhost:11987"               │
├──────────────────────────────────────────────────────────────────┤
│ Stage 2: JSON Config Override (device/config.c)                  │
│ ─────────────────────────────────────────────────────────────    │
│ Parse config.json and override defaults                         │
│ Function: load_plugin_config(Config *config, const char *path)   │
│ Path: /etc/coolercontrol/plugins/coolerdash/config.json         │
├──────────────────────────────────────────────────────────────────┤
│ Stage 3: Device API Detection (cc_conf.c)                        │
│ ─────────────────────────────────────────────────────────────    │
│ Auto-detect display dimensions from CoolerControl API           │
│ Function: update_config_from_device(Config *config)              │
│ Behavior: Only updates if width/height are 0 in config           │
└──────────────────────────────────────────────────────────────────┘
```

### Configuration Structure

**File:** `src/device/config.h`

```c
typedef struct {
    // Daemon connection
    char daemon_address[256];        // CoolerControl API URL
    char daemon_password[128];       // API password

    // File paths
    char paths_images[PATH_MAX];     // Shutdown image directory
    char paths_image_coolerdash[PATH_MAX];  // Rendered image path
    char paths_image_shutdown[PATH_MAX];    // Shutdown image path
    char paths_pid[PATH_MAX];        // PID file location

    // Display settings
    uint16_t display_width;          // LCD width in pixels (auto-detected)
    uint16_t display_height;         // LCD height in pixels (auto-detected)
    float display_refresh_interval;  // Update interval in seconds (e.g., 2.50 = 2.5 seconds)
    int lcd_brightness;              // 0-100
    int lcd_orientation;             // 0=0°, 90=90°, 180=180°, 270=270°
    char display_mode[16];           // Display mode: "dual" or "circle"
    char display_shape[16];          // Shape override: "auto", "rectangular", "circular"
    uint16_t circle_switch_interval; // Circle mode sensor switch interval (1-60s, default: 5)
    float display_content_scale_factor; // Content scale factor (0.5-1.0, default: 0.98)

    // Layout configuration
    int layout_bar_height;           // Temperature bar height
    int layout_label_size;           // Label font size
    int layout_value_size;           // Value font size
    float layout_content_y_offset;   // Vertical positioning (-1.0 to 1.0)

    // Font settings
    char font_face[128];             // Font family name
    int font_weight_label;           // CAIRO_FONT_WEIGHT_*
    int font_weight_value;           // CAIRO_FONT_WEIGHT_*

    // Temperature thresholds
    float temp_cpu_low;              // CPU low threshold (°C)
    float temp_cpu_medium;           // CPU medium threshold (°C)
    float temp_cpu_high;             // CPU high threshold (°C)
    float temp_gpu_low;              // GPU low threshold (°C)
    float temp_gpu_medium;           // GPU medium threshold (°C)
    float temp_gpu_high;             // GPU high threshold (°C)

    // Color definitions (15 colors)
    Color color_background;          // Display background
    Color color_cpu_label;           // "CPU" text
    Color color_cpu_value;           // Temperature value
    Color color_cpu_bar_low;         // Bar color <low
    Color color_cpu_bar_medium;      // Bar color low-medium
    Color color_cpu_bar_high;        // Bar color >medium
    // ... same for GPU
    Color color_bar_background;      // Bar background
    Color color_bar_border;          // Bar border
} Config;
```

### Change Tracking System

**Purpose:** Log active configuration to systemd journal at startup

**Implementation (config.c):**

```c
// Called after JSON loading completes
void log_config(const Config *config);  // Uses LOG_STATUS level (always visible)
```

**Example Output (always shown in systemd journal):**

```
[CoolerDash STATUS] Config loaded: mode=dual, interval=2.5s, brightness=80
[CoolerDash STATUS] Display: 240x240, shape=auto
[CoolerDash STATUS] Daemon: http://localhost:11987
```

**Note:** Changed from LOG_INFO to LOG_STATUS in version 1.96+ to ensure manual configuration changes are always visible in systemd journal, even without --verbose flag.

---

## API Integration

### CoolerControl REST API

**Base URL:** `http://localhost:11987` (configurable)  
**Authentication:** HTTP Basic Auth (username: CCAdmin)  
**Session Management:** Cookie-based (stored in `/tmp/coolerdash_cookie_<PID>.txt`)

### API Endpoints Used

#### 1. Login & Session

```http
POST /login
Authorization: Basic CCAdmin:<password>
Content-Type: application/json

Response: 200 OK / 204 No Content
Sets: session cookies
```

**Implementation:** `src/srv/cc_main.c` → `init_coolercontrol_session()`

---

#### 2. Device Information

```http
GET /devices
Accept: application/json

Response JSON:
{
  "devices": [
    {
      "uid": "liquidctl-nzxt-kraken-z-0",
      "name": "NZXT Kraken Z73",
      "type": "Liquidctl",
      "info": {
        "channels": {
          "lcd": {
            "lcd_info": {
              "screen_width": 320,
              "screen_height": 320
            }
          }
        }
      }
    }
  ]
}
```

**Implementation:** `src/srv/cc_conf.c` → `initialize_device_cache()`  
**Caching:** One-time fetch at startup, stored in static struct  
**Display Detection:** Kraken devices: >240x240 = circular, ≤240 = rectangular

---

#### 3. Temperature Data

```http
POST /status
Content-Type: application/json
Accept: application/json

Request Body:
{
  "all": false,
  "since": "1970-01-01T00:00:00.000Z"
}

Response JSON:
{
  "devices": [
    {
      "type": "CPU",
      "status_history": [
        {
          "temps": [
            {
              "name": "temp1",
              "temp": 45.0
            }
          ]
        }
      ]
    },
    {
      "type": "GPU",
      "status_history": [
        {
          "temps": [
            {
              "name": "GPU Core",
              "temp": 52.0
            }
          ]
        }
      ]
    }
  ]
}
```

**Implementation:** `src/srv/cc_sensor.c` → `get_temperature_data()`  
**Sensor Selection:** CPU = "temp1", GPU = name contains "GPU"/"gpu"  
**Validation:** Temperature range -50°C to +150°C

---

#### 4. LCD Image Upload

```http
PUT /devices/{device_uid}/settings/lcd/lcd/images?log=false
Content-Type: multipart/form-data

Form Fields:
  - mode: "image"
  - brightness: "80"
  - orientation: "0"
  - images[]: <PNG file data>

Response: 200 OK
```

**Implementation:** `src/srv/cc_main.c` → `send_image_to_lcd()`  
**Image Format:** PNG, dimensions match device LCD  
**Multipart Construction:** CURL mime API with proper MIME types

---

### HTTP Session Management

**Session Lifecycle:**

```
┌─────────────────────────────────────────────────────────────┐
│ 1. init_coolercontrol_session()                             │
│    ├─ curl_global_init()                                    │
│    ├─ POST /login (HTTP Basic Auth)                         │
│    ├─ Store cookies in /tmp/coolerdash_cookie_<PID>.txt    │
│    └─ Set session_initialized = 1                           │
├─────────────────────────────────────────────────────────────┤
│ 2. Repeated API Calls (use existing cookies)                │
│    ├─ send_image_to_lcd() - every 60s                      │
│    └─ get_temperature_data() - every 60s                   │
├─────────────────────────────────────────────────────────────┤
│ 3. cleanup_coolercontrol_session()                          │
│    ├─ curl_easy_cleanup()                                   │
│    ├─ curl_global_cleanup()                                 │
│    ├─ unlink(cookie_jar)                                    │
│    └─ Set session_initialized = 0                           │
└─────────────────────────────────────────────────────────────┘
```

**Security Features:**
- Cookie jar path includes PID (prevents conflicts)
- SSL verification enabled for HTTPS endpoints
- Cleanup protection with static flag (prevents double-free)

---

## Rendering Pipeline

### Cairo Graphics Workflow

**Library:** cairo 1.x (vector graphics)  
**Output Format:** PNG image matching LCD dimensions  
**Color Depth:** 24-bit RGB

### Rendering Steps

```
┌──────────────────────────────────────────────────────────────────┐
│ 1. draw_display_image(config)                                    │
│    ├─ Get device info from cache                                 │
│    ├─ Fetch temperature data (cc_sensor.c)                       │
│    └─ Dispatch to draw_dual_image() or draw_circle_image()       │
├──────────────────────────────────────────────────────────────────┤
│ 2. render_display(config, data, device_name)                     │
│    ├─ Detect display shape (circular vs rectangular)             │
│    ├─ Calculate scaling parameters                               │
│    │   ├─ Circular: inscribe_factor = 1/√2 (≈0.7071, default)
│    │   │   * Can be overridden in `config.json` with `inscribe_factor` (>=0; 0 = auto)
│    │   └─ Rectangular: inscribe_factor = 1.0                     │
│    ├─ Create Cairo surface (width × height × ARGB32)             │
│    ├─ Draw background                                            │
│    ├─ Draw CPU section (label + value + bar)                     │
│    ├─ Draw GPU section (label + value + bar)                     │
│    ├─ Write PNG to /etc/coolercontrol/plugins/coolerdash/coolerdash.png  │
│    └─ Upload to LCD via send_image_to_lcd()                      │
└──────────────────────────────────────────────────────────────────┘
```

### Display Shape Detection

**File:** `src/srv/cc_conf.c` → `is_circular_display_device()`

**Algorithm:**

```c
int is_circular_display_device(const char *device_name, int width, int height) {
    const int is_kraken = (strstr(device_name, "Kraken") != NULL);
    
    if (is_kraken) {
        // NZXT Kraken: >240x240 = circular, ≤240 = rectangular
        const int is_large_display = (width > 240 || height > 240);
        return is_large_display ? 1 : 0;
    }
    
    // Add other circular display brands here
    return 0;
}
```
### Scaling unit tests

There is a small test harness at `tests/test_scaling.c` that validates `safe_area` and `safe_bar` calculations.
Use it to verify behavior across cases:

- `inscribe_factor = 0.0` → auto fallback (1/√2)
- `inscribe_factor = 0.70710678` → explicit geometry
- `inscribe_factor = 0.85` → custom factor
- For rectangular overrides the `inscribe_factor = 1.0`.
- Invalid values (e.g. -1 or 1.5) fallback to the geometric factor.

Build & run:
```bash
gcc -std=c99 -Iinclude -I./src -o build/test_scaling tests/test_scaling.c -lm
./build/test_scaling
```

This script prints a table summarizing `inscribe_used`, `safe_area`, and `safe_bar` for the test cases and whether they meet expectations.


**Known Devices:**
- **NZXT Kraken Z53** (280x280) → Circular
- **NZXT Kraken Z73** (320x320) → Circular
- **NZXT Kraken Elite** (640x640) → Circular
- **NZXT Kraken (240x240 or smaller)** → Rectangular

### Circular Display Rendering

**Challenge:** Circular LCD requires content within inscribed square  
**Solution:** Apply inscribe factor 1/√2 ≈ 0.7071 to all coordinates

**Mathematical Basis:**

```
Circle radius = R
Inscribed square side = R * √2 / 2 = R * 1/√2
```

**Example (320x320 circular display):**

```
Original coordinates: (0, 0) to (320, 320)
Safe drawing area: (48, 48) to (272, 272)  [≈70.71% of original]
```

**Implementation (dual.c):**

```c
ScalingParams params;

// Priority: shape config > force flag > auto-detection
if (strcmp(config->display_shape, "rectangular") == 0) {
    params.inscribe_factor = 1.0;
} else if (strcmp(config->display_shape, "circular") == 0) {
    params.inscribe_factor = M_SQRT1_2;
} else {
    // Auto-detection fallback
    int is_circular = config->force_display_circular || 
                      is_circular_display_device(device_name, width, height);
    params.inscribe_factor = is_circular ? M_SQRT1_2 : 1.0;
}

params.safe_content_margin = (1.0 - params.inscribe_factor) / 2.0;  // ≈0.1464

// Apply to all drawing coordinates
double safe_x = params.safe_content_margin * config->display_width;
double safe_y = params.safe_content_margin * config->display_height;
```

**Configuration Override (v1.96+):**

```ini
[display]
# Manual override (recommended for testing/troubleshooting)
shape = auto  # or "rectangular" or "circular"
```

### Temperature Bar Rendering

**Color Selection Logic:**

```c
const Color *get_bar_color(float temp, float low, float medium) {
    if (temp < low)
        return &config->color_cpu_bar_low;       // Green
    else if (temp < medium)
        return &config->color_cpu_bar_medium;    // Yellow
    else
        return &config->color_cpu_bar_high;      // Red
}
```

**Fill Width Calculation:**

```c
int calculate_temp_fill_width(float temp, int max_width, float max_temp) {
    if (temp <= 0.0f)
        return 0;
    
    const float ratio = fminf(temp / max_temp, 1.0f);  // Clamp to 1.0
    return (int)(ratio * max_width);
}
```

---

## Function Reference

### Module: main.c (Daemon Lifecycle)

#### Core Functions

| Function | Purpose | Returns |
|----------|---------|---------|
| `main(argc, argv)` | Entry point, orchestrates daemon lifecycle | `int` exit code |
| `parse_arguments()` | Parse CLI args (`--verbose`, `-v`, `--help`, etc.) | `const char*` config path |
| `initialize_config_and_instance()` | Load config, check existing instance | `int` success |
| `initialize_coolercontrol_services()` | Start API session, cache devices | `int` success |
| `run_daemon(config)` | Main loop: read temps → render → sleep 60s | `int` exit code |

#### Signal Handling

| Function | Purpose |
|----------|---------|
| `setup_enhanced_signal_handlers()` | Register SIGTERM/SIGINT/SIGHUP handlers |
| `handle_shutdown_signal(signum)` | Display shutdown image, stop loop |

#### PID Management

| Function | Purpose |
|----------|---------|
| `write_pid_file(path)` | Atomic PID file creation with O_EXCL |
| `remove_pid_file(path)` | Secure deletion with validation |
| `check_existing_instance_and_handle(path)` | Detect running instance via PID |

#### Cleanup

| Function | Purpose |
|----------|---------|
| `perform_cleanup(config)` | Cleanup session + PID + temp image |
| `send_shutdown_image_if_needed()` | Upload shutdown.png on exit |

---

### Module: device/config.c (Configuration System)

| Function | Purpose |
|----------|---------|
| `load_plugin_config(config, path)` | Load JSON config, apply defaults, log result |
| `set_*_defaults(config)` | Initialize subsystem fields with hardcoded defaults |
| `log_message(level, format, ...)` | Global logging function (respects verbose flag) |

**Default Values:**

```c
daemon_address = "http://localhost:11987"
daemon_password = "coolAdmin"
display_width = 0  // auto-detected from API
display_refresh_interval = 2.5
lcd_brightness = 80
```

**JSON Sections:**

```json
{
  "daemon": { "address": "...", "password": "..." },
  "display": { "width": 0, "height": 0, "brightness": 80, "mode": "dual" },
  "layout": { "bar_height": 30, "label_size": 18, "value_size": 24 },
  "font": { "face": "Roboto" },
  "temperature": { "cpu_low": 50, "cpu_medium": 70, "cpu_high": 85 },
  "colors": { "background": [0,0,0], "cpu_label": [255,255,255] }
}

---

### Module: srv/cc_main.c (Session Management)

#### Public API (4 functions)

| Function | Purpose | Returns |
|----------|---------|---------|
| `init_coolercontrol_session(config)` | Login, setup cookie jar | `int` success |
| `is_session_initialized()` | Check session state | `int` boolean |
| `cleanup_coolercontrol_session()` | Cleanup CURL, delete cookies | `void` |
| `send_image_to_lcd(config, image_path, device_uid)` | Upload PNG to LCD | `int` success |

#### Internal Helpers (15 functions)

```c
cc_init_response_buffer()        // Allocate HTTP response buffer
write_callback()                 // libcurl write callback
build_login_credentials()        // Construct login URL + credentials
configure_login_curl()           // Setup CURL for login
build_lcd_upload_form()          // Create multipart form (mode/brightness/image)
add_mime_field()                 // Add string field to form
add_image_file_field()           // Add PNG file to form
configure_lcd_upload_curl()      // Setup CURL for upload
validate_upload_params()         // Check parameters before upload
check_upload_response()          // Validate HTTP 200 response
```

**Session State:**

```c
static CoolerControlSession cc_session = {
    .curl_handle = NULL,
    .cookie_jar = {0},
    .session_initialized = 0
};
```

---

### Module: srv/cc_conf.c (Device Cache)

#### Public API (4 functions)

| Function | Purpose | Returns |
|----------|---------|---------|
| `init_device_cache(config)` | Fetch device info once (GET /devices) | `int` success |
| `get_liquidctl_data(config, uid, name, width, height)` | Read cached data | `int` success |
| `update_config_from_device(config)` | Auto-set width/height if 0 | `int` updated |
| `is_circular_display_device(name, width, height)` | Detect display shape | `int` boolean |

#### Internal Helpers (16 functions)

```c
initialize_device_cache()        // HTTP GET + JSON parse + cache population
parse_liquidctl_data()           // Extract device info from JSON
extract_device_type_from_json()  // Get "type" field
extract_device_uid()             // Get "uid" field
extract_device_name()            // Get "name" field
extract_lcd_dimensions()         // Get screen_width/screen_height
search_liquidctl_device()        // Find first Liquidctl device in array
configure_device_cache_curl()    // Setup CURL for /devices request
process_device_cache_response()  // Parse response + populate cache
```

**Cache Structure:**

```c
static struct {
    int initialized;
    char device_uid[128];
    char device_name[CC_NAME_SIZE];
    int screen_width;
    int screen_height;
} device_cache = {0};
```

---

### Module: srv/cc_sensor.c (Temperature Monitoring)

#### Public API (1 function)

| Function | Purpose | Returns |
|----------|---------|---------|
| `get_temperature_monitor_data(config, data)` | Read CPU/GPU temps | `int` success |

**Data Structure:**

```c
typedef struct {
    float temp_cpu;
    float temp_gpu;
} monitor_sensor_data_t;
```

#### Internal Helpers (5 functions)

```c
get_temperature_data()           // HTTP POST /status + parse response
parse_temperature_data()         // Extract temps from JSON
extract_device_temperature()     // Get temp from status_history[last].temps[]
configure_status_request()       // Setup CURL for /status POST
```

**Temperature Validation:**

```c
if (temperature < -50.0f || temperature > 150.0f)
    continue;  // Invalid, skip sensor
```

---

### Module: mods/display.c + dual.c + circle.c (Rendering)

#### Public API

| Function | Purpose | Returns |
|----------|---------|---------|
| `draw_display_image(config)` | Dispatch to dual or circle mode | `void` |
| `draw_dual_image(config)` | Render CPU+GPU simultaneously, upload | `void` |
| `draw_circle_image(config)` | Render alternating sensor slot, upload | `void` |

#### Internal Helpers

```c
calculate_scaling_params()       // Compute inscribe factor, margins
draw_background()                // Fill background color
draw_temperature_section()       // Render CPU/GPU label+value+bar
draw_temperature_label()         // Render "CPU"/"GPU" text
draw_temperature_value()         // Render "45.0°C" text
draw_temperature_bar()           // Render colored bar with border
get_bar_color()                  // Select color based on thresholds
calculate_temp_fill_width()      // Compute bar fill width from temp
set_cairo_color()                // Convert Color struct to cairo RGB
cairo_color_convert()            // uint8_t (0-255) → double (0.0-1.0)
```

**Rendering Workflow:**

```c
cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
cairo_t *cr = cairo_create(surface);

// Draw background
cairo_set_source_rgb(cr, bg_r, bg_g, bg_b);
cairo_paint(cr);

// Draw CPU section (top half)
draw_temperature_section(cr, config, params, data->temp_cpu, "CPU", CPU);

// Draw GPU section (bottom half)
draw_temperature_section(cr, config, params, data->temp_gpu, "GPU", GPU);

// Save to PNG
cairo_surface_write_to_png(surface, "/etc/coolercontrol/plugins/coolerdash/coolerdash.png");
cairo_destroy(cr);
cairo_surface_destroy(surface);
```

---

## Development Guidelines

### Code Style

- **Standard:** C99 with POSIX.1-2001 extensions
- **Indentation:** 4 spaces (no tabs)
- **Line Length:** 120 characters maximum
- **Naming:**
  - Functions: `snake_case`
  - Structs: `PascalCase` (e.g., `Config`, `ScalingParams`)
  - Constants: `UPPER_SNAKE_CASE`
  - Static functions: `static` prefix, file-scope only

### Error Handling

**Pattern:** Check every allocation, API call, file operation

```c
// Example: Safe malloc
char *buffer = malloc(size);
if (!buffer) {
    log_message(LOG_ERROR, "Memory allocation failed");
    return 0;
}

// Example: CURL error checking
CURLcode res = curl_easy_perform(curl);
if (res != CURLE_OK) {
    log_message(LOG_ERROR, "CURL failed: %s", curl_easy_strerror(res));
    return 0;
}

// Example: File operation
FILE *fp = fopen(path, "r");
if (!fp) {
    log_message(LOG_ERROR, "Cannot open file: %s", path);
    return 0;
}
```

### Memory Safety

**Rules:**

1. **Always free what you allocate** (RAII pattern)
2. **Check for NULL before dereferencing**
3. **Use fixed-size buffers with bounds checking**
4. **Prefer stack allocation for small objects**

**Example (cleanup pattern):**

```c
int my_function(void) {
    char *buffer = malloc(1024);
    if (!buffer)
        return 0;
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        free(buffer);
        return 0;
    }
    
    // ... use resources ...
    
    // Cleanup (always reached)
    curl_easy_cleanup(curl);
    free(buffer);
    return 1;
}
```

### Logging Levels

```c
typedef enum {
    LOG_INFO,     // Only shown with --verbose flag
    LOG_STATUS,   // Always shown (important state changes)
    LOG_WARNING,  // Always shown (non-fatal issues)
    LOG_ERROR     // Always shown (fatal errors)
} log_level_t;
```

**Usage:**

```c
log_message(LOG_INFO, "Reading config from %s", path);       // Verbose only
log_message(LOG_STATUS, "Session initialized successfully"); // Always
log_message(LOG_WARNING, "Using fallback value for %s", key); // Always
log_message(LOG_ERROR, "Failed to connect to API");          // Always
```

### Adding New Configuration Options

**Steps:**

1. **Add field to Config struct** (`src/device/config.h`)
2. **Add default value** (`src/device/config.c` → appropriate `set_*_defaults()`)
3. **Add JSON parsing** (`src/device/config.c` → `load_*_from_json()` function)
4. **Update documentation** (`docs/config-guide.md`)
5. **Update example config** (`/etc/coolercontrol/plugins/coolerdash/config.json`)

**Example 1: Adding Circle Mode Switch Interval (uint16_t with validation)**

```c
// 1. config.h - Add field to Config struct
typedef struct {
    // ...
    uint16_t circle_switch_interval;  // Circle mode sensor switch interval (1-60s)
} Config;

// 2. config.c - Set default value
static void set_display_defaults(Config *config) {
    if (config->circle_switch_interval == 0)
        config->circle_switch_interval = 5;
}

// 3. config.c - Add JSON parsing
static void load_display_from_json(Config *config, json_t *display) {
    json_t *val = json_object_get(display, "circle_switch_interval");
    if (json_is_integer(val)) {
        long v = json_integer_value(val);
        if (v >= 1 && v <= 60)
            config->circle_switch_interval = (uint16_t)v;
        else
            log_message(LOG_WARNING, "circle_switch_interval must be 1-60, using default: 5");
    }
}
```

**Example 2: Adding Content Scale Factor (float with validation)**

```c
// 1. config.h - Add field to Config struct
typedef struct {
    // ...
    float display_content_scale_factor;  // Content scale factor (0.5-1.0)
} Config;

// 2. config.c - Set default value
static void set_display_defaults(Config *config) {
    // ...
    config->display_content_scale_factor = 0.98f;  // Default: 98% (2% margin)
}

// 3. config.c - Add JSON parsing
static void load_display_from_json(Config *config, json_t *display) {
    json_t *val = json_object_get(display, "content_scale_factor");
    if (json_is_real(val)) {
        float f = (float)json_real_value(val);
        if (f >= 0.5f && f <= 1.0f)
            config->display_content_scale_factor = f;
    }
}
```

**Example 3: Adding New Color**

```c
// 1. config.h
typedef struct {
    // ...
    Color color_my_new_element;
} Config;

// 2. config.c - Set default value
static void set_color_defaults(Config *config) {
    // ...
    config->color_my_new_element = (Color){0, 255, 0};  // Green
}

// 3. config.c - Add JSON parsing
static void load_colors_from_json(Config *config, json_t *colors) {
    json_t *my_obj = json_object_get(colors, "my_new_element");
    if (json_is_object(my_obj))
        parse_color_from_json(my_obj, &config->color_my_new_element);
}
```

**Configuration Best Practices:**
- Always provide sensible defaults in `set_*_defaults()`
- Validate user input with clear error messages using `log_message(LOG_WARNING, ...)`
- Use appropriate data types (`uint16_t` for small integers, `float` for decimals)
- Document acceptable ranges in both code comments and `config.json`
- Log warnings for invalid values and fallback to defaults

---

## Testing & Debugging

### Manual Testing

```bash
# 1. Stop service
sudo systemctl stop coolerdash.service

# 2. Run manually with verbose logging
coolerdash --verbose

# 3. Watch for errors/warnings in output
# Expected: [CoolerDash STATUS] messages every 60s

# 4. Check generated image
ls -lh /tmp/coolerdash.png
file /tmp/coolerdash.png  # Should show: PNG image data, 320 x 320, ...
```

### Debug Build

```bash
# Compile with debug symbols + AddressSanitizer
make clean
make debug

# Run under debugger
gdb bin/coolerdash
(gdb) run --verbose

# Check for memory leaks
# AddressSanitizer will report any leaks on exit
```

### Service Debugging

```bash
# Check service status
systemctl status coolerdash.service

# View recent logs
journalctl -u coolerdash.service -n 50

# Follow logs in real-time
journalctl -xeu coolerdash.service -f

# Restart service
sudo systemctl restart coolerdash.service
```

### Common Issues

#### Issue: "Session initialization failed"

**Cause:** CoolerControl not running or wrong credentials  
**Debug:**

```bash
# Check CoolerControl status
systemctl status coolercontrold

# Test API manually
curl -u CCAdmin:coolAdmin -X POST http://localhost:11987/login

# Check config password
grep daemon_password /etc/coolercontrol/plugins/coolerdash/config.json
```

---

#### Issue: "No Liquidctl devices found"

**Cause:** Device not detected or wrong type  
**Debug:**

```bash
# List devices via API
curl http://localhost:11987/devices | jq '.devices[] | {uid, name, type}'

# Check for Liquidctl type
# Expected: "type": "Liquidctl"
```

---

#### Issue: "Display wrong shape (circular/rectangular)"

**Cause:** Incorrect display detection logic  
**Debug:**

```bash
# Run with verbose logging
coolerdash --verbose

# Look for log message:
# [CoolerDash STATUS] Display shape: circular (based on 320x320 Kraken)
# or
# [CoolerDash STATUS] Display shape: rectangular (based on 240x240)

# Force circular mode (developer flag)
coolerdash --develop  # All displays treated as circular
```

---

#### Issue: "Temperature values incorrect"

**Cause:** Wrong sensor selection  
**Debug:**

```bash
# Check raw API response
curl -u CCAdmin:coolAdmin -X POST http://localhost:11987/status \
  -H "Content-Type: application/json" \
  -d '{"all": false, "since": "1970-01-01T00:00:00.000Z"}' | jq

# Look for temps[] arrays in CPU/GPU devices
# Verify sensor names match expected patterns:
#   CPU: "temp1"
#   GPU: contains "GPU" or "gpu"
```

---

### Profiling

**CPU Usage Monitoring:**

```bash
# Check daemon resource usage
ps aux | grep coolerdash

# Expected: ~0.1% CPU (idle most of time, 60s sleep)
# Memory: ~10-20 MB RSS
```

**API Call Timing:**

```bash
# Enable verbose logging
coolerdash --verbose 2>&1 | grep -E "(Session|Temperature|Upload)"

# Expected timing:
# Session init: <1s
# Temperature fetch: <500ms
# Image render: <200ms
# LCD upload: <1s
```

---

## Performance Characteristics

### Resource Usage

- **Memory:** ~15 MB RSS (static allocations + Cairo surface)
- **CPU (idle):** <0.1% (60s sleep between updates)
- **CPU (active):** ~5-10% (during rendering + upload, <2s every 60s)
- **Network:** ~50 KB/min (API calls + image uploads)
- **Disk I/O:** Minimal (one PNG write per update)

### Optimization Strategies

1. **Device Caching:** One-time API call instead of per-frame lookup
2. **Exponential Buffer Growth:** Reduce realloc() calls for HTTP responses
3. **Static Session:** Persistent CURL handle + cookie reuse
4. **Stack Allocation:** Small structs on stack (ScalingParams, etc.)
5. **Efficient Scaling Math:** Pre-computed inscribe factors

---

## Future Development

### Potential Enhancements

- [ ] **Multi-display support:** Handle multiple LCD devices simultaneously
- [ ] **Plugin system:** External modules for custom visualizations
- [ ] **GPU utilization:** Show usage % in addition to temperature
- [ ] **Network stats:** Bandwidth monitoring on display
- [ ] **Custom images:** User-provided background/overlay images
- [ ] **Animation support:** GIF/APNG rendering
- [ ] **Themes:** Predefined color/layout presets
- [ ] **Web UI:** Browser-based configuration editor

### Contributing

**Repository:** https://github.com/damachine/coolerdash  
**Issues:** https://github.com/damachine/coolerdash/issues  
**Discussions:** https://github.com/damachine/coolerdash/discussions

**Pull Request Checklist:**

- [ ] Code follows C99 standard + style guidelines
- [ ] No compiler warnings with `-Wall -Wextra`
- [ ] All allocations have matching frees
- [ ] Error paths properly handled
- [ ] Tested with `make debug` (AddressSanitizer)
- [ ] Documentation updated (if adding features)
- [ ] CHANGELOG.md entry added

---

## License

**MIT License** - See LICENSE file for full text.

**Copyright (c) 2025 damachine (damachin3@proton.me)**

---

## References

### External Documentation

- **CoolerControl API:** https://gitlab.com/coolercontrol/coolercontrol/-/blob/main/openapi-spec.json
- **Cairo Graphics:** https://www.cairographics.org/manual/
- **libcurl:** https://curl.se/libcurl/c/
- **Jansson JSON:** https://jansson.readthedocs.io/

### Project Files

- **Configuration Guide:** `docs/config-guide.md`
- **Supported Devices:** `docs/devices.md`
- **Display Detection:** `docs/display-detection.md`
- **Example Config:** `etc/coolercontrol/plugins/coolerdash/config.json`

---

**Document Version:** 2.x  
**Last Updated:** 2026  
**Maintained by:** damachine (damachin3@proton.me)
