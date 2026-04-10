# CoolerDash Developer Guide

**C99** | **Linux x86-64-v3** | **MIT License**
Author: damachine (damachin3@proton.me)
Repository: https://github.com/damachine/coolerdash

---

## Architecture

```
main.c
 ├─ Configuration loading    (device/config.c)
 ├─ Session init + auth      (srv/cc_main.c)
 ├─ Device cache setup       (srv/cc_conf.c)
 ├─ Shutdown image register  (srv/cc_main.c, CC4)
 ├─ Main loop
 │   ├─ Temperature reading  (srv/cc_sensor.c)
 │   ├─ Image rendering      (mods/display.c → dual.c | circle.c)
 │   └─ LCD upload           (srv/cc_main.c)
 ├─ Signal handling          (SIGTERM/SIGINT → graceful stop)
 └─ Cleanup                  (session + image files)
```

## Module Structure

```
src/
├── main.c              # Daemon lifecycle, signal handling, PID management
├── device/
│   └── config.c/h      # JSON config loader + defaults
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
| main.c | `main()` |
| device/config | `load_plugin_config()` |
| srv/cc_main | `init_coolercontrol_session()`, `is_session_initialized()`, `cleanup_coolercontrol_session()`, `send_image_to_lcd()` |
| srv/cc_conf | `init_device_cache()`, `get_cached_lcd_device_data()`, `update_config_from_device()`, `is_circular_display_device()` |
| srv/cc_sensor | `get_temperature_monitor_data()` |
| mods/display | `draw_display_image()` |
| mods/dual | `draw_dual_image()` |
| mods/circle | `draw_circle_image()` |

---

## Build System

```bash
make                # C99, -O2, -march=x86-64-v3
make clean          # Remove build artifacts
make debug          # Debug build with AddressSanitizer
make install        # System installation
make uninstall      # Complete removal
```

Compiler flags:
```makefile
CFLAGS = -Wall -Wextra -O2 -std=c99 -march=x86-64-v3 -Iinclude \
         $(shell pkg-config --cflags cairo jansson libcurl)
LIBS = $(shell pkg-config --libs cairo jansson libcurl) -lm
```

Dependencies: `cairo`, `jansson`, `libcurl-gnutls`, `ttf-roboto`

---

## Configuration System

Three-stage loading:

1. **Hardcoded defaults** — `set_*_defaults()` in `config.c`
2. **JSON override** — `load_plugin_config()` from `/etc/coolercontrol/plugins/coolerdash/config.json`
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

- NZXT Kraken ≤240×240 → rectangular (inscribe_factor = 1.0)
- NZXT Kraken >240×240 → circular (inscribe_factor = 1/√2 ≈ 0.7071)
- Override via `config.json`: `"shape": "rectangular"` or `"circular"`

### Scaling

Base resolution: 240×240. Content scales dynamically.
Circular displays: `safe_area = display_width × inscribe_factor`

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
# Unit test
gcc -std=c99 -Iinclude -I./src -o build/test_scaling tests/test_scaling.c -lm
./build/test_scaling

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
