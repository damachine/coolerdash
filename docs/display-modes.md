# Display Modes

Two modes: **dual** (default) and **circle**.

Mode selection: `config.json` → `"display": { "mode": "dual" }` or CLI `--dual` / `--circle`.

## Files

```
src/mods/
├── display.c/h    # Mode dispatcher
├── dual.c/h       # Dual mode
└── circle.c/h     # Circle mode
```

Dispatcher (`display.c`):
```c
void draw_display_image(const struct Config *config) {
    if (strcmp(config->display_mode, "circle") == 0)
        draw_circle_image(config);
    else
        draw_dual_image(config);
}
```

---

## Dual Mode

CPU and GPU temperatures side-by-side.

```
┌────────────────────────────┐
│     CPU: 45°    GPU: 52°   │
│     ┌─────┐     ┌─────┐   │
│     │█████│     │██████│   │
│     └─────┘     └─────┘   │
│      CPU         GPU       │
└────────────────────────────┘
```

### ScalingParams

```c
typedef struct {
    double scale_x, scale_y;
    double corner_radius;
    double inscribe_factor;     // 1.0 (rectangular) or M_SQRT1_2 (circular)
    int safe_bar_width;
    double safe_content_margin;
    int is_circular;
} ScalingParams;
```

Base resolution: 240×240. Scales dynamically.
Circular displays: `safe_area = display_width × inscribe_factor × content_scale_factor`

### Rendering Flow

1. `draw_dual_image()` — entry point
2. `get_cached_lcd_device_data()` — device info
3. `get_temperature_monitor_data()` — sensor data
4. Cairo: create surface → draw background → draw bars + labels → write PNG
5. `send_image_to_lcd()` — upload

---

## Circle Mode

Alternates between sensor slots, one at a time. Optimized for circular high-res displays.

```
┌────────────────┐
│      45°       │
│    ┌──────┐    │
│    │██████│    │
│    └──────┘    │
│      CPU       │
└────────────────┘
```

### Sensor Slots

Configured in `config.json`:
```json
"sensor_slot_1": "cpu",
"sensor_slot_2": "liquid",
"sensor_slot_3": "gpu"
```

Cycles through slots at `circle_switch_interval` (default: 8s, range: 1–60s).

### State

```c
static int current_slot = 0;      // 0→1→2→0
static time_t last_switch_time = 0;
```

### Centering

Temperature + degree symbol centered as a unit:
```c
const double total_width = temp_ext.width + 5 + degree_ext.width;
double temp_x = (display_width - total_width) / 2.0;
```

### Rendering Flow

1. `draw_circle_image()` — entry point
2. `get_cached_lcd_device_data()` — device info
3. `get_temperature_monitor_data()` — sensor data
4. `update_sensor_mode()` — check switch interval
5. Cairo: create surface → draw single sensor → write PNG
6. `send_image_to_lcd()` — upload

---

## Adding a New Mode

1. Create `src/mods/newmode.c/h`
2. Implement `draw_newmode_image(const struct Config *config)`
3. Add dispatch in `display.c`
4. Add validation in `config.c` (`load_display_from_json`)
5. Add to Makefile `SRC_MODULES`
6. Add CLI flag in `main.c` (`parse_arguments`)
7. Update docs and `config.json`
