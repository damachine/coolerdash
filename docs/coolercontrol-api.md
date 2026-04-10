# CoolerControl API Integration

## Modules

| Module | File | Purpose |
|--------|------|---------|
| cc_main | `src/srv/cc_main.c/h` | Session management, auth, LCD upload |
| cc_conf | `src/srv/cc_conf.c/h` | Device cache, display detection |
| cc_sensor | `src/srv/cc_sensor.c/h` | Temperature retrieval |

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/devices` | GET | Device enumeration (once at startup) |
| `/status` | POST | Temperature sensor data |
| `/devices/{uid}/settings/lcd/lcd/images` | PUT | LCD image upload |
| `/devices/{uid}/settings/lcd/lcd/shutdown-image` | PUT | Shutdown image registration (CC4) |

Base URL: `http://localhost:11987` (configurable)
Auth: `Authorization: Bearer cc_<uuid>`

---

## Session Lifecycle

```
1. init_coolercontrol_session()
   - curl_global_init() + curl_easy_init()
   - Build "Authorization: Bearer cc_<uuid>" header
   - session_initialized = 1

2. Main loop (reuse session)
   - get_temperature_data()  → POST /status
   - send_image_to_lcd()     → PUT /devices/{uid}/.../images

3. cleanup_coolercontrol_session()
   - curl_easy_cleanup() + curl_global_cleanup()
   - session_initialized = 0
```

---

## cc_main — Session & Upload

### Session State

```c
typedef struct {
    CURL *curl_handle;
    char access_token[CC_BEARER_HEADER_SIZE];
    int session_initialized;
} CoolerControlSession;
```

### Public API

| Function | Purpose |
|----------|---------|
| `init_coolercontrol_session(config)` | Init CURL + Bearer header |
| `is_session_initialized()` | Check session state |
| `cleanup_coolercontrol_session()` | Free CURL resources |
| `send_image_to_lcd(config, image_path, device_uid)` | Upload PNG via multipart PUT |

### LCD Upload

```
PUT {address}/devices/{uid}/settings/lcd/lcd/images?log=false
Content-Type: multipart/form-data

Fields:
  mode: "image"
  brightness: "80"
  orientation: "0"
  images[]: <PNG file>
```

### Shutdown Image (CC4)

Called once at startup. CC4 stores it server-side and displays it when CoolerControl stops.

```
PUT {address}/devices/{uid}/settings/lcd/lcd/shutdown-image
```

### HTTP Response Buffer

```c
typedef struct http_response {
    char *data;
    size_t size;
    size_t capacity;
} http_response;
```

Growth strategy: `new_capacity = max(required_size, capacity * 3/2)`

---

## cc_conf — Device Cache

### Cache Structure

```c
static struct {
    int initialized;
    char device_uid[128];
    char device_name[CC_NAME_SIZE];
    int screen_width;
    int screen_height;
} device_cache = {0};
```

Populated once at startup via `GET /devices`. Device properties don't change at runtime.

### Public API

| Function | Purpose |
|----------|---------|
| `init_device_cache(config)` | Fetch + cache device info |
| `get_cached_lcd_device_data(...)` | Read cached UID, name, dimensions |
| `update_config_from_device(config)` | Set width/height if 0 in config |
| `is_circular_display_device(name, w, h)` | Detect display shape |

### Device JSON Structure

```json
{
  "devices": [{
    "uid": "1234-5678-abcd",
    "name": "NZXT Kraken Elite",
    "type": "Liquidctl",
    "info": {
      "channels": {
        "lcd": {
          "lcd_info": { "screen_width": 640, "screen_height": 640 }
        }
      }
    }
  }]
}
```

### Circular Display Detection

- NZXT Kraken ≤240×240 → rectangular
- NZXT Kraken >240×240 → circular
- Other devices: check database, default rectangular

Override via `config.json`: `"shape": "rectangular"` or `"circular"`

### cc_safe_strcpy

Bounds-checked string copy with guaranteed null-termination. Use instead of `strcpy`/`strncpy`.

---

## cc_sensor — Temperature

### Data Structure

```c
typedef struct {
    float temp_cpu;
    float temp_gpu;
} monitor_sensor_data_t;
```

### API Request

```
POST {address}/status
Content-Type: application/json

{"all": false, "since": "1970-01-01T00:00:00.000Z"}
```

### Response

```json
{
  "devices": [{
    "type": "CPU",
    "status_history": [{
      "temps": [{ "name": "temp1", "temp": 45.0 }]
    }]
  }]
}
```

Sensor matching: CPU = `temp1`, GPU = name contains "GPU"/"gpu".
Validation: -50°C to 150°C range.

---

## Testing API Manually

```bash
# Devices
curl http://127.0.0.1:11987/devices \
    -H "Authorization: Bearer cc_<uuid>" | jq

# Temperature
curl -X POST http://127.0.0.1:11987/status \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer cc_<uuid>" \
    -d '{"all":false,"since":"1970-01-01T00:00:00.000Z"}' | jq

# LCD upload
curl -X PUT "http://127.0.0.1:11987/devices/{UID}/settings/lcd/lcd/images?log=false" \
    -H "Authorization: Bearer cc_<uuid>" \
    -F "mode=image" -F "brightness=80" -F "orientation=0" \
    -F "images[]=@coolerdash.png;type=image/png"
```

---

## Troubleshooting

| Problem | Check |
|---------|-------|
| Session init fails | `systemctl status coolercontrold`, verify `access_token` |
| No LCD device found | `curl .../devices \| jq`, check for `"type": "Liquidctl"` |
| Temperature 0.0 | Check sensor names in `/status` response |
| Upload fails (401) | Token invalid or missing |
