# CoolerControl API Integration - Developer Guide

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Module cc_main](#module-cc_main)
- [Module cc_conf](#module-cc_conf)
- [Module cc_sensor](#module-cc_sensor)
- [API Communication Flow](#api-communication-flow)
- [Data Structures](#data-structures)
- [Error Handling](#error-handling)
- [Best Practices](#best-practices)
- [Troubleshooting](#troubleshooting)

---

## Overview

The CoolerControl API integration consists of three core modules that handle all communication with the CoolerControl daemon's REST API:

- **cc_main.c/h**: Session management, authentication, and LCD image upload
- **cc_conf.c/h**: Device caching, display detection, and configuration utilities
- **cc_sensor.c/h**: Temperature data retrieval from CPU and GPU sensors

### Purpose

CoolerDash acts as a specialized LCD client for CoolerControl, fetching temperature data and uploading rendered images to Liquidctl-compatible devices with LCD displays (e.g., NZXT Kraken Elite).

### API Endpoints Used

```
POST   /login                          - Authentication
GET    /devices                        - Device enumeration
POST   /status                         - Temperature sensor data
PUT    /devices/{uid}/settings/lcd/... - LCD image upload
```

### Dependencies

- **libcurl**: HTTP client for REST API communication
- **jansson**: JSON parsing and generation
- **CoolerControl Daemon**: Must be running and accessible (default: `http://127.0.0.1:11987`)

---

## Architecture

### Module Relationships

```
┌─────────────────────────────────────────────────────────┐
│                      main.c                             │
│                  (Main Event Loop)                      │
└──────────────────┬──────────────────────────────────────┘
                   │
                   │ Initialization & Data Retrieval
                   │
       ┌───────────┴───────────┬──────────────────────────┐
       │                       │                          │
┌──────▼──────┐       ┌───────▼────────┐       ┌────────▼────────┐
│  cc_main.c  │       │   cc_conf.c    │       │  cc_sensor.c    │
│  Session    │◄──────┤   Device       │       │  Temperature    │
│  Auth       │       │   Cache        │       │  Data           │
│  Upload     │       │   Detection    │       │  Retrieval      │
└─────────────┘       └────────────────┘       └─────────────────┘
       │                       │                          │
       │                       │                          │
       └───────────────────────┴──────────────────────────┘
                               │
                               │ libcurl + jansson
                               │
                    ┌──────────▼───────────┐
                    │  CoolerControl       │
                    │  Daemon (REST API)   │
                    └──────────────────────┘
```

### Initialization Sequence

```c
// 1. Configuration loading (config.c)
Config config = {0};
load_plugin_config(&config, CONFIG_PATH);

// 2. CoolerControl session initialization
init_coolercontrol_session(&config);

// 3. Device cache initialization
init_device_cache(&config);

// 4. Update config with device dimensions (if needed)
update_config_from_device(&config);

// 5. Main loop: fetch data → render → upload
while (running) {
    get_temperature_monitor_data(&config, &data);
    draw_display_image(&config);  // Dispatches to dual or circle mode
    // send_image_to_lcd() called internally
    sleep(update_interval);
}

// 6. Cleanup
cleanup_coolercontrol_session();
```

---

## Module cc_main

### Purpose
Handles HTTP session management, authentication, and LCD image upload via multipart PUT requests.

### File: `src/srv/cc_main.c` (600 lines)

### Key Components

#### 1. HTTP Response Buffer

**Purpose**: Dynamic memory management for HTTP responses

```c
typedef struct http_response {
    char *data;
    size_t size;
    size_t capacity;
} http_response;
```

**Functions**:
- `cc_init_response_buffer()`: Allocate with initial capacity (default: 4096-8192 bytes)
- `cc_cleanup_response_buffer()`: Free memory and reset state
- `reallocate_response_buffer()`: Exponential growth (1.5x or exact fit)

**Growth Strategy**:
```c
new_capacity = max(required_size, capacity * 3/2)
```

**Usage Pattern**:
```c
http_response response = {0};
cc_init_response_buffer(&response, 8192);
// ... use response ...
cc_cleanup_response_buffer(&response);
```

#### 2. Session State

**Purpose**: Maintain persistent CURL session with cookie-based authentication

```c
typedef struct {
    CURL *curl_handle;
    char cookie_jar[CC_COOKIE_SIZE];
    int session_initialized;
} CoolerControlSession;

static CoolerControlSession cc_session = {
    .curl_handle = NULL,
    .cookie_jar = {0},
    .session_initialized = 0
};
```

**Cookie Jar**: `/tmp/coolerdash_cookie_{PID}.txt`
- Stores session cookies for authenticated requests
- Cleaned up on program exit
- Per-process isolation via PID

#### 3. Authentication Flow

**Function**: `init_coolercontrol_session(const Config *config)`

**Steps**:
1. Initialize libcurl global state
2. Create CURL easy handle
3. Configure cookie jar for session persistence
4. Build login URL: `{daemon_address}/login`
5. Set HTTP Basic Auth: `CCAdmin:{password}`
6. Send POST request with empty body
7. Validate response (200/204)
8. Store cookies for subsequent requests

**Implementation**:
```c
int init_coolercontrol_session(const Config *config)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cc_session.curl_handle = curl_easy_init();
    
    // Cookie jar: /tmp/coolerdash_cookie_{PID}.txt
    snprintf(cc_session.cookie_jar, sizeof(cc_session.cookie_jar),
             "/tmp/coolerdash_cookie_%d.txt", getpid());
    
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEJAR, cc_session.cookie_jar);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_COOKIEFILE, cc_session.cookie_jar);
    
    // Login request
    char login_url[CC_URL_SIZE];
    snprintf(login_url, sizeof(login_url), "%s/login", config->daemon_address);
    
    char userpwd[CC_USERPWD_SIZE];
    snprintf(userpwd, sizeof(userpwd), "CCAdmin:%s", config->daemon_password);
    
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, login_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_POSTFIELDS, "");
    
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long response_code = 0;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    
    // Secure password cleanup
    memset(userpwd, 0, sizeof(userpwd));
    
    if (res == CURLE_OK && (response_code == 200 || response_code == 204)) {
        cc_session.session_initialized = 1;
        return 1;
    }
    
    return 0;
}
```

**Security Notes**:
- Password immediately zeroed after use (`memset`)
- HTTPS support with SSL verification
- Cookie-based session prevents password re-transmission

#### 4. LCD Image Upload

**Function**: `send_image_to_lcd(const Config *config, const char *image_path, const char *device_uid)`

**Purpose**: Upload rendered PNG to device LCD via multipart PUT request

**URL Format**:
```
PUT {daemon_address}/devices/{device_uid}/settings/lcd/lcd/images?log=false
```

**Multipart Form Fields**:
- `mode`: "image" (static image mode)
- `brightness`: Integer 0-100 (from config)
- `orientation`: Integer 0-270 (from config)
- `images[]`: PNG file (Content-Type: image/png)

**Implementation**:
```c
int send_image_to_lcd(const Config *config, const char *image_path, const char *device_uid)
{
    // 1. Validate parameters
    if (!cc_session.curl_handle || !image_path || !device_uid || !cc_session.session_initialized) {
        return 0;
    }
    
    // 2. Build URL
    char upload_url[CC_URL_SIZE];
    snprintf(upload_url, sizeof(upload_url), 
             "%s/devices/%s/settings/lcd/lcd/images?log=false",
             config->daemon_address, device_uid);
    
    // 3. Build multipart form
    curl_mime *form = curl_mime_init(cc_session.curl_handle);
    
    // Add mode field
    curl_mimepart *field = curl_mime_addpart(form);
    curl_mime_name(field, "mode");
    curl_mime_data(field, "image", CURL_ZERO_TERMINATED);
    
    // Add brightness field
    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d", config->lcd_brightness);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "brightness");
    curl_mime_data(field, brightness_str, CURL_ZERO_TERMINATED);
    
    // Add orientation field
    char orientation_str[8];
    snprintf(orientation_str, sizeof(orientation_str), "%d", config->lcd_orientation);
    field = curl_mime_addpart(form);
    curl_mime_name(field, "orientation");
    curl_mime_data(field, orientation_str, CURL_ZERO_TERMINATED);
    
    // Add image file
    field = curl_mime_addpart(form);
    curl_mime_name(field, "images[]");
    curl_mime_filedata(field, image_path);
    curl_mime_type(field, "image/png");
    
    // 4. Configure CURL
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_URL, upload_url);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(cc_session.curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    
    // 5. Send request
    CURLcode res = curl_easy_perform(cc_session.curl_handle);
    long http_response_code = -1;
    curl_easy_getinfo(cc_session.curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);
    
    // 6. Cleanup
    curl_mime_free(form);
    
    // 7. Check result
    return (res == CURLE_OK && http_response_code == 200);
}
```

**Helper Functions**:
- `build_lcd_upload_form()`: Construct multipart form
- `add_mime_field()`: Add string field with error checking
- `add_image_file_field()`: Add PNG file with MIME type
- `configure_lcd_upload_curl()`: Set CURL options
- `cleanup_lcd_upload_curl()`: Reset CURL state

**Error Handling**:
- Validates session initialization
- Checks file existence (via `curl_mime_filedata()`)
- Logs CURL errors and HTTP codes
- Returns 0 on failure, 1 on success

#### 5. Session Cleanup

**Function**: `cleanup_coolercontrol_session(void)`

**Purpose**: Clean shutdown with resource deallocation

**Steps**:
1. Cleanup CURL easy handle
2. Cleanup libcurl global state
3. Remove cookie jar file
4. Mark session as uninitialized
5. Set cleanup flag to prevent double-cleanup

**Implementation**:
```c
void cleanup_coolercontrol_session(void)
{
    static int cleanup_done = 0;
    if (cleanup_done)
        return;
    
    // Cleanup CURL handle
    if (cc_session.curl_handle) {
        curl_easy_cleanup(cc_session.curl_handle);
        cc_session.curl_handle = NULL;
    }
    
    // Global CURL cleanup
    curl_global_cleanup();
    
    // Remove cookie jar
    unlink(cc_session.cookie_jar);
    
    // Mark as uninitialized
    cc_session.session_initialized = 0;
    cleanup_done = 1;
}
```

**Called By**: `main()` via `atexit()` or signal handler

#### 6. CURL Write Callback

**Function**: `write_callback(const void *contents, size_t size, size_t nmemb, http_response *response)`

**Purpose**: Receive HTTP response data from libcurl

**Algorithm**:
1. Calculate received size: `realsize = size * nmemb`
2. Check if buffer needs reallocation
3. If needed, grow buffer exponentially (1.5x or exact)
4. Copy new data to buffer using `memmove()`
5. Update size and null-terminate
6. Return received size (or 0 on error)

**Memory Safety**:
- Prevents buffer overflow via size checking
- Exponential growth reduces reallocation frequency
- Always null-terminates for string safety

### File: `src/srv/cc_main.h`

**Purpose**: Public API declarations for session management

**Key Definitions**:
```c
// Response buffer
typedef struct http_response {
    char *data;
    size_t size;
    size_t capacity;
} http_response;

// Buffer management
int cc_init_response_buffer(http_response *response, size_t initial_capacity);
void cc_cleanup_response_buffer(http_response *response);

// Session management
int init_coolercontrol_session(const struct Config *config);
int is_session_initialized(void);
void cleanup_coolercontrol_session(void);

// LCD upload
int send_image_to_lcd(const struct Config *config, const char *image_path, const char *device_uid);

// CURL callback
size_t write_callback(const void *contents, size_t size, size_t nmemb, http_response *response);
```

**Constants**:
```c
#define CC_COOKIE_SIZE 512
#define CC_UID_SIZE 128
#define CC_URL_SIZE 512
#define CC_USERPWD_SIZE 128
#define CC_MAX_SAFE_ALLOC_SIZE (SIZE_MAX / 2)
```

---

## Module cc_conf

### Purpose
Device information caching, circular display detection, and configuration utilities.

### File: `src/srv/cc_conf.c` (488 lines)

### Key Components

#### 1. Device Cache

**Purpose**: Store device info once to avoid repeated API calls

```c
static struct {
    int initialized;
    char device_uid[128];
    char device_name[CC_NAME_SIZE];
    int screen_width;
    int screen_height;
} device_cache = {0};
```

**Rationale**:
- Device properties don't change during runtime
- Reduces API overhead (every render cycle)
- Improves performance and reliability

**Access Function**: `get_liquidctl_data()`
- Returns cached data if available
- Initializes cache on first call
- Thread-safe (single-threaded daemon)

#### 2. Device Detection

**Function**: `init_device_cache(const Config *config)`

**Purpose**: Fetch device list and extract Liquidctl device info

**API Call**:
```
GET {daemon_address}/devices
```

**Response Structure**:
```json
{
  "devices": [
    {
      "uid": "1234-5678-abcd",
      "name": "NZXT Kraken Elite",
      "type": "Liquidctl",
      "info": {
        "channels": {
          "lcd": {
            "lcd_info": {
              "width": 640,
              "height": 640
            }
          }
        }
      }
    }
  ]
}
```

**Implementation**:
```c
int init_device_cache(const Config *config)
{
    if (device_cache.initialized)
        return 1;
    
    // Build URL
    char url[256];
    snprintf(url, sizeof(url), "%s/devices", config->daemon_address);
    
    // Initialize response buffer
    http_response response = {0};
    cc_init_response_buffer(&response, 16384);
    
    // Send request
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    
    // Parse JSON response
    if (res == CURLE_OK) {
        parse_liquidctl_data(response.data, 
                            device_cache.device_uid,
                            sizeof(device_cache.device_uid),
                            &device_cache.screen_width,
                            &device_cache.screen_height,
                            device_cache.device_name,
                            sizeof(device_cache.device_name));
        device_cache.initialized = 1;
    }
    
    // Cleanup
    cc_cleanup_response_buffer(&response);
    curl_easy_cleanup(curl);
    
    return device_cache.initialized;
}
```

#### 3. JSON Parsing

**Function**: `parse_liquidctl_data(const char *json, ...)`

**Purpose**: Extract device information from devices JSON response

**Algorithm**:
1. Parse JSON string with jansson
2. Get "devices" array
3. Iterate through devices
4. Check device type ("Liquidctl")
5. Extract UID from "uid" field
6. Extract name from "name" field
7. Navigate to "info.channels.lcd.lcd_info"
8. Extract width and height
9. Return first matching device

**Helper Functions**:
- `extract_device_type_from_json()`: Get device type string
- `extract_device_uid()`: Get UID string
- `extract_device_name()`: Get device name
- `get_lcd_info_from_device()`: Navigate JSON to lcd_info
- `extract_lcd_dimensions()`: Get width/height from lcd_info
- `is_liquidctl_device()`: Check if type matches "Liquidctl"

**Error Handling**:
- Validates JSON structure at each level
- Returns 0 if any field is missing
- Logs errors for debugging

#### 4. Circular Display Detection

**Function**: `is_circular_display_device(const char *device_name, int screen_width, int screen_height)`

**Purpose**: Determine if device has circular LCD display

**Detection Logic**:

```c
int is_circular_display_device(const char *device_name, int screen_width, int screen_height)
{
    if (!device_name)
        return 0;
    
    // NZXT Kraken detection
    const int is_kraken = (strstr(device_name, "Kraken") != NULL);
    
    if (is_kraken) {
        // Elite models with >240x240 are circular
        if (screen_width > 240 || screen_height > 240) {
            return 1;
        }
        return 0;  // 240x240 and smaller are rectangular
    }
    
    // Known circular devices list
    const char *circular_devices[] = {
        "Corsair iCUE LINK"
        // Add more models here
    };
    
    for (size_t i = 0; i < num_circular; i++) {
        if (strstr(device_name, circular_devices[i]) != NULL) {
            return 1;
        }
    }
    
    return 0;
}
```

**NZXT Kraken Rules**:
- **240x240**: Rectangular (e.g., Kraken Z53)
- **>240x240**: Circular (e.g., Kraken Elite 280/360 with 640x640 display)

**Display Shape Override (v1.96+)**:

**Recommended Method:** `display_shape` config parameter (highest priority):
```ini
[display]
shape = auto  # or "rectangular" or "circular"
```

**Legacy Method:** `force_display_circular` flag (backwards compatibility):
```ini
[display]
force_circular = true
```

**Priority System:**
1. `display_shape` config (manual override)
2. `force_display_circular` flag (legacy)
3. Automatic device detection (default)

#### 5. Configuration Update

**Function**: `update_config_from_device(Config *config)`

**Purpose**: Update config with device screen dimensions (only if not set in `config.json`)

**Logic**:
```c
int update_config_from_device(Config *config)
{
    if (!config)
        return 0;
    
    // Only update if config.json values are 0 (auto-detect)
    if (config->display_width == 0) {
        config->display_width = device_cache.screen_width;
        log_message(LOG_INFO, "Updated display width to %d from device", 
                    device_cache.screen_width);
    }
    
    if (config->display_height == 0) {
        config->display_height = device_cache.screen_height;
        log_message(LOG_INFO, "Updated display height to %d from device", 
                    device_cache.screen_height);
    }
    
    return 1;
}
```

**Use Case**: Auto-detect display resolution when user doesn't specify in `config.json`

#### 6. Safe String Copy

**Function**: `cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src)`

**Purpose**: Bounds-checked string copy with null-termination guarantee

**Implementation**:
```c
int cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src)
{
    if (!dest || !src || dest_size == 0)
        return -1;
    
    for (size_t i = 0; i < dest_size - 1; i++) {
        dest[i] = src[i];
        if (src[i] == '\0')
            return 0;
    }
    
    dest[dest_size - 1] = '\0';
    return 0;
}
```

**Advantages**:
- Prevents buffer overflow
- Always null-terminates
- Simple and portable
- No dependency on platform-specific functions

### File: `src/srv/cc_conf.h`

**Purpose**: Public API for device caching and detection

**Key Functions**:
```c
// Device cache
int init_device_cache(const struct Config *config);
int get_liquidctl_data(const struct Config *config, char *device_uid, 
                       size_t uid_size, char *device_name, size_t name_size, 
                       int *screen_width, int *screen_height);

// Configuration
int update_config_from_device(struct Config *config);

// Detection
int is_circular_display_device(const char *device_name, int screen_width, int screen_height);

// Utilities
int cc_safe_strcpy(char *restrict dest, size_t dest_size, const char *restrict src);
const char *extract_device_type_from_json(const json_t *dev);
```

**Constants**:
```c
#define CC_NAME_SIZE 128
```

---

## Module cc_sensor

### Purpose
Retrieve CPU and GPU temperature data from CoolerControl status endpoint.

### File: `src/srv/cc_sensor.c` (290 lines)

### Key Components

#### 1. Temperature Data Structure

```c
typedef struct {
    float temp_cpu;
    float temp_gpu;
} monitor_sensor_data_t;
```

**Usage**:
```c
monitor_sensor_data_t data = {0};
get_temperature_monitor_data(&config, &data);
printf("CPU: %.1f°C, GPU: %.1f°C\n", data.temp_cpu, data.temp_gpu);
```

#### 2. Temperature Retrieval

**Function**: `get_temperature_monitor_data(const Config *config, monitor_sensor_data_t *data)`

**Purpose**: High-level API to fetch temperature data

**Implementation**:
```c
int get_temperature_monitor_data(const Config *config, monitor_sensor_data_t *data)
{
    if (!config || !data)
        return 0;
    
    return get_temperature_data(config, &data->temp_cpu, &data->temp_gpu);
}
```

**Internal Flow**: Delegates to `get_temperature_data()`

#### 3. Status API Request

**Function**: `get_temperature_data(const Config *config, float *temp_cpu, float *temp_gpu)`

**API Call**:
```
POST {daemon_address}/status
Content-Type: application/json

{
  "all": false,
  "since": "1970-01-01T00:00:00.000Z"
}
```

**Response Structure**:
```json
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

**Implementation**:
```c
static int get_temperature_data(const Config *config, float *temp_cpu, float *temp_gpu)
{
    // Initialize outputs
    *temp_cpu = 0.0f;
    *temp_gpu = 0.0f;
    
    // Build URL
    char url[256];
    snprintf(url, sizeof(url), "%s/status", config->daemon_address);
    
    // Initialize response buffer
    http_response response = {0};
    cc_init_response_buffer(&response, 8192);
    
    // Configure request
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    
    // POST data
    const char *post_data = "{\"all\":false,\"since\":\"1970-01-01T00:00:00.000Z\"}";
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    
    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Send request
    CURLcode res = curl_easy_perform(curl);
    
    // Parse response
    if (res == CURLE_OK) {
        parse_temperature_data(response.data, temp_cpu, temp_gpu);
    }
    
    // Cleanup
    cc_cleanup_response_buffer(&response);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return (res == CURLE_OK);
}
```

#### 4. JSON Temperature Parsing

**Function**: `parse_temperature_data(const char *json, float *temp_cpu, float *temp_gpu)`

**Purpose**: Extract CPU and GPU temperatures from status JSON

**Algorithm**:
1. Parse JSON with jansson
2. Get "devices" array
3. For each device:
   - Get device type (CPU/GPU)
   - Get status_history array
   - Get latest status (last element)
   - Get temps array
   - Search for appropriate sensor name
   - Extract temperature value
   - Validate range (-50°C to 150°C)
4. Return when both CPU and GPU found

**Sensor Name Matching**:
- **CPU**: `temp1` (standard Linux sensor name)
- **GPU**: Contains "GPU" or "gpu" (varies by manufacturer)

**Helper Function**: `extract_device_temperature()`

```c
static float extract_device_temperature(const json_t *device, const char *device_type)
{
    // Get status history
    const json_t *status_history = json_object_get(device, "status_history");
    size_t history_count = json_array_size(status_history);
    
    // Get latest status
    const json_t *last_status = json_array_get(status_history, history_count - 1);
    
    // Get temperatures array
    const json_t *temps = json_object_get(last_status, "temps");
    
    // Search for matching sensor
    size_t temp_count = json_array_size(temps);
    for (size_t i = 0; i < temp_count; i++) {
        const json_t *temp_entry = json_array_get(temps, i);
        const char *sensor_name = json_string_value(json_object_get(temp_entry, "name"));
        float temperature = json_number_value(json_object_get(temp_entry, "temp"));
        
        // Validate range
        if (temperature < -50.0f || temperature > 150.0f)
            continue;
        
        // Check sensor name
        if (strcmp(device_type, "CPU") == 0 && strcmp(sensor_name, "temp1") == 0) {
            return temperature;
        }
        else if (strcmp(device_type, "GPU") == 0 && 
                 (strstr(sensor_name, "GPU") || strstr(sensor_name, "gpu"))) {
            return temperature;
        }
    }
    
    return 0.0f;
}
```

**Error Handling**:
- Returns 0.0f on missing data
- Validates JSON structure at each level
- Range checks temperature values
- Logs parsing errors

### File: `src/srv/cc_sensor.h`

**Purpose**: Public API for temperature data retrieval

**Key Definitions**:
```c
// Data structure
typedef struct {
    float temp_cpu;
    float temp_gpu;
} monitor_sensor_data_t;

// High-level API
int get_temperature_monitor_data(const struct Config *config, monitor_sensor_data_t *data);
```

---

## API Communication Flow

### Initialization Phase

```
main() startup
    ↓
1. load_configuration()
    ↓
2. init_coolercontrol_session(&config)
    │
    ├─→ POST /login
    │   (HTTP Basic Auth: CCAdmin:{password})
    │   Response: 200 OK + session cookie
    │
    └─→ Save cookie to /tmp/coolerdash_cookie_{PID}.txt
    ↓
3. init_device_cache(&config)
    │
    ├─→ GET /devices
    │   Response: Device list JSON
    │
    ├─→ Parse JSON, find Liquidctl device
    │
    └─→ Cache: UID, name, width, height
    ↓
4. update_config_from_device(&config)
    │
    └─→ Update display_width/height if 0 in config
    ↓
5. Ready for main loop
```

### Main Loop Phase

```
while (running)
    ↓
1. get_temperature_monitor_data(&config, &data)
    │
    ├─→ POST /status
    │   Content-Type: application/json
    │   Body: {"all":false,"since":"1970-01-01T00:00:00.000Z"}
    │   Cookie: {session_cookie}
    │
    ├─→ Response: Device status JSON with temps
    │
    └─→ Parse: Extract temp_cpu and temp_gpu
    ↓
2. draw_display_image(&config)
    │
    ├─→ render_dual_display() or render_circle_display()
    │   │
    │   ├─→ Cairo rendering
    │   │
    │   └─→ Write PNG: /etc/coolercontrol/plugins/coolerdash/coolerdash.png
    │
    └─→ send_image_to_lcd(&config, png_path, device_uid)
        │
        ├─→ PUT /devices/{uid}/settings/lcd/lcd/images?log=false
        │   Content-Type: multipart/form-data
        │   Cookie: {session_cookie}
        │   
        │   Fields:
        │   - mode: "image"
        │   - brightness: "100"
        │   - orientation: "0"
        │   - images[]: {PNG file data}
        │
        └─→ Response: 200 OK
    ↓
3. sleep(update_interval)
    ↓
4. Loop back to step 1
```

### Shutdown Phase

```
Signal received (SIGTERM/SIGINT) or exit()
    ↓
cleanup_coolercontrol_session()
    │
    ├─→ curl_easy_cleanup()
    │
    ├─→ curl_global_cleanup()
    │
    ├─→ unlink(/tmp/coolerdash_cookie_{PID}.txt)
    │
    └─→ session_initialized = 0
    ↓
Program exit
```

---

## Data Structures

### Config Structure (Relevant Fields)

```c
struct Config {
    // CoolerControl connection
    char daemon_address[256];      // e.g., "http://127.0.0.1:11987"
    char daemon_password[128];     // CCAdmin password
    
    // Display settings
    uint16_t display_width;        // e.g., 640 (auto-detected if 0)
    uint16_t display_height;       // e.g., 640
    char display_shape[16];        // "auto", "rectangular", or "circular" (v1.96+)
    int force_display_circular;    // Legacy override (deprecated)
    
    // LCD settings
    int lcd_brightness;            // 0-100
    int lcd_orientation;           // 0, 90, 180, 270
    
    // Display mode
    char display_mode[16];         // "dual" or "circle"
    
    // Paths
    char paths_image_coolerdash[512]; // PNG output path
};
```

### Device Cache

```c
static struct {
    int initialized;               // 0 = not cached, 1 = cached
    char device_uid[128];          // e.g., "1234-5678-abcd"
    char device_name[CC_NAME_SIZE]; // e.g., "NZXT Kraken Elite"
    int screen_width;              // e.g., 640
    int screen_height;             // e.g., 640
} device_cache;
```

### Session State

```c
typedef struct {
    CURL *curl_handle;             // Persistent CURL handle
    char cookie_jar[CC_COOKIE_SIZE]; // Cookie file path
    int session_initialized;       // 0 = not initialized, 1 = ready
} CoolerControlSession;

static CoolerControlSession cc_session;
```

### HTTP Response Buffer

```c
typedef struct http_response {
    char *data;                    // Dynamic buffer
    size_t size;                   // Current data size
    size_t capacity;               // Allocated capacity
} http_response;
```

### Temperature Data

```c
typedef struct {
    float temp_cpu;                // CPU temperature (°C)
    float temp_gpu;                // GPU temperature (°C)
} monitor_sensor_data_t;
```

---

## Error Handling

### Return Value Convention

- **Success**: 1 (true)
- **Failure**: 0 (false)
- **Error**: Negative values (rare, mostly in legacy code)

### Logging Levels

```c
LOG_ERROR    - Critical failures (API unreachable, auth failed)
LOG_WARNING  - Recoverable issues (sensor missing, invalid data)
LOG_INFO     - Verbose mode info (API responses, cache hits)
LOG_STATUS   - Normal operation status (upload success)
```

### Common Error Scenarios

#### 1. Authentication Failure

**Symptom**: `init_coolercontrol_session()` returns 0

**Causes**:
- Wrong password in config
- CoolerControl daemon not running
- Daemon address unreachable

**Debugging**:
```c
log_message(LOG_ERROR, "Login failed: CURL code %d, HTTP code %ld", res, response_code);
```

**Resolution**:
- Verify daemon is running: `systemctl status coolercontrol`
- Check password in `config.json`
- Test connection: `curl http://127.0.0.1:11987/devices`

#### 2. Device Not Found

**Symptom**: `init_device_cache()` returns 0

**Causes**:
- No Liquidctl device connected
- Device not detected by CoolerControl
- Wrong device type in JSON

**Debugging**:
```c
log_message(LOG_ERROR, "No Liquidctl device found in API response");
```

**Resolution**:
- Check CoolerControl GUI for device list
- Verify Liquidctl support: `liquidctl list`
- Ensure device permissions (udev rules)

#### 3. Temperature Data Missing

**Symptom**: `temp_cpu` or `temp_gpu` is 0.0

**Causes**:
- Sensor not available in CoolerControl
- Sensor name mismatch
- Device disconnected

**Debugging**:
```c
log_message(LOG_WARNING, "CPU temperature not found in status response");
```

**Resolution**:
- Check CoolerControl dashboard for sensor visibility
- Verify sensor names in JSON response
- Ensure drivers loaded (lm-sensors, nvidia-smi)

#### 4. LCD Upload Failure

**Symptom**: `send_image_to_lcd()` returns 0

**Causes**:
- PNG file not found
- Device UID invalid
- Session cookie expired

**Debugging**:
```c
log_message(LOG_ERROR, "LCD upload failed: CURL code %d, HTTP code %ld", res, http_response_code);
if (response.data && response.size > 0) {
    log_message(LOG_ERROR, "Server response: %s", response.data);
}
```

**Resolution**:
- Verify PNG exists: `ls -l /tmp/coolerdash.png`
- Check device UID matches cache
- Re-authenticate if session lost

---

## Best Practices

### 1. Resource Management

**Always pair initialization with cleanup**:
```c
// Buffer
http_response response = {0};
cc_init_response_buffer(&response, 8192);
// ... use response ...
cc_cleanup_response_buffer(&response);

// CURL
CURL *curl = curl_easy_init();
// ... use curl ...
curl_easy_cleanup(curl);

// Headers
struct curl_slist *headers = NULL;
headers = curl_slist_append(headers, "Content-Type: application/json");
// ... use headers ...
curl_slist_free_all(headers);
```

### 2. Session Handling

**Check session before API calls**:
```c
if (!is_session_initialized()) {
    log_message(LOG_ERROR, "Session not initialized");
    return 0;
}
```

**Use persistent session for all requests**:
- Don't create new CURL handles per request
- Reuse `cc_session.curl_handle` for performance
- Cookie persistence handles re-authentication

### 3. JSON Parsing

**Validate structure at each level**:
```c
json_t *root = json_loads(json_string, 0, &error);
if (!root) {
    log_message(LOG_ERROR, "JSON parse error: %s", error.text);
    return 0;
}

const json_t *devices = json_object_get(root, "devices");
if (!devices || !json_is_array(devices)) {
    json_decref(root);
    return 0;
}

// ... continue parsing ...

json_decref(root);  // Always cleanup
```

**Always decref JSON objects**:
```c
json_t *root = json_loads(...);
// ... use root ...
json_decref(root);  // Free memory
```

### 4. Error Logging

**Provide actionable context**:
```c
// Bad
log_message(LOG_ERROR, "Failed");

// Good
log_message(LOG_ERROR, "Failed to upload LCD image: HTTP %ld, device UID: %s", 
            http_code, device_uid);
```

**Include CURL error details**:
```c
CURLcode res = curl_easy_perform(curl);
if (res != CURLE_OK) {
    log_message(LOG_ERROR, "CURL request failed: %s", curl_easy_strerror(res));
}
```

### 5. String Safety

**Use bounds-checked functions**:
```c
// Prefer cc_safe_strcpy over strcpy/strncpy
cc_safe_strcpy(dest, sizeof(dest), src);

// Always validate snprintf
int written = snprintf(buffer, sizeof(buffer), format, args);
if (written < 0 || (size_t)written >= sizeof(buffer)) {
    log_message(LOG_ERROR, "String truncated");
    return 0;
}
```

### 6. Memory Allocation

**Check allocation success**:
```c
char *buffer = malloc(size);
if (!buffer) {
    log_message(LOG_ERROR, "Memory allocation failed: %zu bytes", size);
    return 0;
}
```

**Prevent overflow in size calculations**:
```c
if (initial_capacity > CC_MAX_SAFE_ALLOC_SIZE) {
    log_message(LOG_ERROR, "Requested size too large: %zu", initial_capacity);
    return 0;
}
```

### 7. HTTPS Support

**Enable SSL verification**:
```c
if (strncmp(config->daemon_address, "https://", 8) == 0) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
}
```

**Certificate path** (optional):
```c
curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
```

---

## Troubleshooting

### Debugging API Communication

**Enable CURL verbose output**:
```c
curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
```

**Capture HTTP headers**:
```c
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    printf("Header: %s", buffer);
    return size * nitems;
}

curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
```

**Dump response body**:
```c
if (response.data && response.size > 0) {
    FILE *f = fopen("/tmp/coolerdash_api_response.json", "w");
    fwrite(response.data, 1, response.size, f);
    fclose(f);
    log_message(LOG_INFO, "API response saved to /tmp/coolerdash_api_response.json");
}
```

### Testing API Calls Manually

**Test authentication**:
```bash
curl -X POST http://127.0.0.1:11987/login \
     -u CCAdmin:your_password \
     -c cookies.txt \
     -v
```

**Test device list**:
```bash
curl -X GET http://127.0.0.1:11987/devices \
     -b cookies.txt \
     -H "Accept: application/json" | jq
```

**Test temperature data**:
```bash
curl -X POST http://127.0.0.1:11987/status \
     -b cookies.txt \
     -H "Content-Type: application/json" \
     -d '{"all":false,"since":"1970-01-01T00:00:00.000Z"}' | jq
```

**Test LCD upload**:
```bash
curl -X PUT "http://127.0.0.1:11987/devices/{UID}/settings/lcd/lcd/images?log=false" \
     -b cookies.txt \
     -F "mode=image" \
     -F "brightness=100" \
     -F "orientation=0" \
     -F "images[]=@/tmp/coolerdash.png;type=image/png" \
     -v
```

### Common Issues

#### Issue: "Session not initialized"

**Check**:
```bash
systemctl status coolercontrol
curl -I http://127.0.0.1:11987
```

**Fix**: Start CoolerControl daemon
```bash
systemctl start coolercontrol
```

#### Issue: "No Liquidctl device found"

**Check devices**:
```bash
curl http://127.0.0.1:11987/devices | jq '.devices[] | {type, name}'
```

**Verify Liquidctl**:
```bash
sudo liquidctl list
```

#### Issue: "Temperature always 0.0"

**Check sensor names**:
```bash
curl -X POST http://127.0.0.1:11987/status \
     -H "Content-Type: application/json" \
     -d '{"all":false,"since":"1970-01-01T00:00:00.000Z"}' | \
jq '.devices[] | select(.type=="CPU" or .type=="GPU") | .status_history[0].temps'
```

**Adjust sensor name matching** in `extract_device_temperature()` if needed.

#### Issue: "LCD upload fails with HTTP 401"

**Cause**: Session cookie expired

**Fix**: Session re-initialization handled automatically, but check:
```bash
ls -la /tmp/coolerdash_cookie_*.txt
cat /tmp/coolerdash_cookie_*.txt
```

---

## Performance Considerations

### API Call Frequency

**Current Setup** (default 2-second interval):
- Temperature data: POST /status every 2s
- LCD upload: PUT /devices/.../images every 2s
- Device list: GET /devices once at startup (cached)

**Optimization**:
- Device cache eliminates repeated /devices calls
- Persistent CURL session reuses HTTP connection
- Cookie-based auth avoids re-login

### Memory Usage

**Typical Allocations**:
- Response buffers: 4-16KB (dynamic)
- Device cache: ~400 bytes (static)
- Session state: ~600 bytes (static)
- JSON parsing: Temporary (freed immediately)

**Buffer Sizing**:
- `/devices` response: 8-16KB (many devices)
- `/status` response: 4-8KB (temperature data)
- Upload response: 4KB (minimal)

### Network Overhead

**Per Update Cycle** (2 seconds):
- Request: ~500 bytes (POST /status)
- Response: ~2-4KB (JSON)
- Upload request: ~50-100KB (PNG + multipart)
- Upload response: ~200 bytes

**Total**: ~50-104KB per cycle = ~25-52KB/s average

---

## Future Enhancements

### Potential Improvements

1. **Asynchronous API Calls**: Use libcurl multi interface for parallel requests
2. **Connection Pooling**: Reuse connections more efficiently
3. **Compression**: Enable gzip for JSON responses
4. **WebSocket Support**: Real-time temperature streaming instead of polling
5. **Retry Logic**: Automatic retry with exponential backoff on transient failures
6. **Response Caching**: Skip LCD upload if image hasn't changed
7. **Multiple Devices**: Support multiple LCD devices simultaneously
8. **Device Hot-Plug**: Detect device connection/disconnection and re-initialize

### API Version Compatibility

**Current**: CoolerControl v1.x REST API

**Future**: If CoolerControl API changes:
- Add version detection: GET /version
- Conditional logic for different API versions
- Deprecation warnings for old endpoints

---

## Conclusion

The CoolerControl API integration provides a robust, efficient interface for:
- **Authentication**: Cookie-based session with HTTP Basic Auth
- **Device Detection**: Automatic Liquidctl device discovery and caching
- **Temperature Monitoring**: Real-time CPU/GPU data retrieval
- **LCD Upload**: Multipart image upload with brightness/orientation control
- **Error Handling**: Comprehensive validation and logging

### Key Design Principles

- **Caching**: Device information cached to minimize API calls
- **Persistence**: Reusable CURL session for performance
- **Safety**: Bounds-checked strings, validated allocations
- **Modularity**: Separate concerns (auth, config, sensors)
- **Reliability**: Extensive error checking and logging

### Integration Example

```c
// Complete workflow
Config config = load_configuration();

// Initialize API
if (!init_coolercontrol_session(&config)) {
    log_message(LOG_ERROR, "Failed to connect to CoolerControl");
    return 1;
}

if (!init_device_cache(&config)) {
    log_message(LOG_ERROR, "No compatible device found");
    cleanup_coolercontrol_session();
    return 1;
}

update_config_from_device(&config);

// Main loop
while (running) {
    monitor_sensor_data_t data = {0};
    if (get_temperature_monitor_data(&config, &data)) {
        // Render and upload handled by display modules
        draw_display_image(&config);
    }
    sleep(2);
}

// Cleanup
cleanup_coolercontrol_session();
```

For questions or contributions, see `CONTRIBUTING.md`.

---

**Version**: 1.0  
**Last Updated**: November 6, 2025  
**Authors**: damachine <christkue79@gmail.com>
