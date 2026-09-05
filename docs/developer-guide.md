# CoolerDash Developer Guide

**C99** | **Linux x86-64** | **MIT License**
Author: Christian Kühn (damachin3@proton.me)
Repository: https://github.com/damachine/coolerdash

---

## Architecture

```
main.c
 ├─ Standalone update check  (exits before plugin setup)
 ├─ Configuration loading    (device/config.c)
 ├─ Session init + auth      (srv/cc_main.c)
 ├─ Device cache setup       (srv/cc_conf.c)
 ├─ Shutdown image register  (srv/cc_main.c, CC4)
 ├─ Main loop
 │   ├─ Temperature reading  (srv/cc_sensor.c)
 │   ├─ Image rendering      (mods/display.c → split.c | dual.c | circle.c)
 │   └─ LCD upload           (srv/cc_main.c)
 ├─ Signal handling          (SIGTERM/SIGINT → graceful stop)
 └─ Cleanup                  (session + image files)
```

## Module Structure

```
src/
├── main.c              # Daemon lifecycle and standalone update check
├── main.h              # Update helper declarations
├── device/
│   ├── config.c/h          # JSON config loader + defaults
│   ├── profile.c/h         # LCD geometry and transport metadata
│   └── hwreport.c/h        # Sanitized hardware reporting + LCD test
├── srv/
│   ├── cc_main.c/h     # Session management, auth, LCD upload
│   ├── cc_conf.c/h     # Device cache, display detection
│   └── cc_sensor.c/h   # Temperature monitoring
└── mods/
    ├── display.c/h      # Mode dispatcher
    ├── dual.c/h         # Dual mode (CPU+GPU simultaneous)
    └── circle.c/h       # Circle mode (alternating sensor)
```

| Module | Public API |
|--------|------------|
| main.c | `main()`, `update_compare_versions()`, `update_parse_release()` |
| device/config | `load_plugin_config()` |
| device/profile | `resolve_display_profile()`, `calculate_circle_chord_bounds()` |
| device/hwreport | `run_hardware_report()` |
| srv/cc_main | `init_coolercontrol_session()`, `is_session_initialized()`, `cleanup_coolercontrol_session()`, `send_image_to_lcd()` |
| srv/cc_conf | `init_device_cache()`, `get_cached_lcd_device_data()`, `update_config_from_device()`, `is_circular_display_device()` |
| srv/cc_sensor | `get_temperature_monitor_data()` |
| mods/display | `draw_display_image()` |
| mods/split | `draw_split_image()` |
| mods/dual | `draw_dual_image()` |
| mods/circle | `draw_circle_image()` |

---

## Build System

```bash
make                # C99, -O2
make clean          # Remove build artifacts
make debug          # Debug build with AddressSanitizer
make install-deps   # Install dependencies only
sudo make install   # Install dependencies, build, and install
sudo make uninstall # Remove program files and preserve user data
```

Compiler flags:
```makefile
CFLAGS ?= -Wall -Wextra -O2
CFLAGS += -std=c99
CPPFLAGS += -Iinclude $(shell pkg-config --cflags cairo fontconfig gdk-pixbuf-2.0 jansson libcurl)
LDLIBS = $(shell pkg-config --libs cairo fontconfig gdk-pixbuf-2.0 jansson libcurl) -lm
```

Dependencies: `cairo`, `coolercontrold`, `fontconfig`, `gdk-pixbuf`, `jansson`, `libcurl`, `glibc`, `ttf-roboto`

---

## Configuration System

Three-stage loading:

1. **Hardcoded defaults** — `set_*_defaults()` in `config.c`
2. **JSON override** — `load_plugin_config()` from `/var/lib/coolercontrol/plugins/coolerdash/config.json`
3. **API detection** — `update_config_from_device()` sets width/height if 0

### Adding a Config Option

1. Add field to `Config` struct in `config.h`
2. Set default in `set_*_defaults()` in `config.c`
3. Add JSON parsing in `load_*_from_json()` in `config.c`
4. Add to `config.json`
5. Update `docs/config-guide.md`

---

## Rendering Pipeline

```
draw_display_image(config)
 ├─ get_cached_lcd_device_data()
 ├─ get_temperature_monitor_data()
 └─ dispatch → draw_dual_image() or draw_circle_image()
      ├─ cairo_image_surface_create(ARGB32, w, h)
      ├─ cairo_create(surface)
      ├─ draw background + bars + labels + temperatures
      ├─ cairo_surface_write_to_png(surface, path)
      ├─ cairo_destroy(cr) + cairo_surface_destroy(surface)
      └─ send_image_to_lcd(config, path, uid)
```

### Display Shape

- Known devices resolve through `device/profile.c`.
- VID:PID is canonical; model tokens plus exact resolution are the current API fallback.
- Unknown devices retain the legacy resolution heuristic for compatibility.

### Scaling

Base resolution: 240×240. Content scales dynamically.
Circular bars use `2 × sqrt(radius² - distance²)` at their actual vertical
position. Text lanes and extra information use the same circle geometry.

---

## Code Style

- C99 + POSIX.1-2001
- 4 spaces, no tabs, 120 char max line
- Functions: `snake_case`, Structs: `PascalCase`, Constants: `UPPER_SNAKE_CASE`
- Use `cc_safe_strcpy()` instead of `strcpy`/`strncpy`
- Check every `malloc`, `fopen`, `curl_easy_perform`
- Return 1 on success, 0 on failure

### Logging

```c
log_message(LOG_INFO, "...");     // --verbose only
log_message(LOG_STATUS, "...");   // always shown
log_message(LOG_WARNING, "...");  // always shown
log_message(LOG_ERROR, "...");    // always shown
```

---

## Testing

```bash
# Debug build
make clean && make debug

# Manual run
sudo systemctl stop coolercontrold
coolerdash --verbose

# Service logs
journalctl -xeu coolerdash.service -f
```

---

## References

- CoolerControl API: https://gitlab.com/coolercontrol/coolercontrol
- Cairo: https://www.cairographics.org/manual/
- libcurl: https://curl.se/libcurl/c/
- Jansson: https://jansson.readthedocs.io/
