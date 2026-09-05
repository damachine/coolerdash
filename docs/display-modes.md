# Display Modes

Three modes: **split** (default), **dual**, and **circle**.

Mode selection: `config.json` → `"display": { "mode": "split" }` or CLI `--split`, `--dual`, or `--circle`.

## Display Elements

The UI shows only the checkboxes supported by the selected mode and saves them
independently in the `display` object:

- **Dual:** `dual_show_bars` (default `true`). Hiding both bars releases the
  middle area for the two sensor rows.
- **Split:** `split_show_load` and `split_show_watts` (default `true`), plus
  `split_show_rpm` (default `false`). Visible secondary rows share the available
  column height; hiding rows enlarges the temperature region. CPU RPM uses the
  cooler's RPM channel, as in Circle; other slots use their own RPM channel.
  Load uses 66% of the effective Power / Extra Info font size.
- **Circle:** individual load, RPM, power, frequency and bar controls, described
  below.

The preview uses static example values and recalculates its layout when elements
are toggled. The device renderer uses its detected geometry and configured font
limits when allocating the freed space; large values and long labels are fitted
within their regions.

## Files

```
src/mods/
├── display.c/h    # Mode dispatcher
├── dual.c/h       # Dual mode
├── circle.c/h     # Circle mode
└── split.c/h      # Split mode
```

Dispatcher (`display.c`):
```c
void draw_display_image(const struct Config *config) {
    if (strcmp(config->display_mode, "circle") == 0)
        draw_circle_image(config);
    else if (strcmp(config->display_mode, "split") == 0)
        draw_split_image(config);
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
    double scale_x, scale_y, scale_uniform;
    double corner_radius;
    double circle_center_x, circle_center_y;
    double circle_radius;
    int safe_bar_width;
    double safe_content_margin;
    int is_circular;
    DisplayShape shape;
    const char *profile_name;
} ScalingParams;
```

Base resolution: 240×240. Scales dynamically.
Circular displays use a position-dependent chord calculated from each rendered
region's Y position and height.

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

### Circle Layout

Selecting Circle in the plugin UI reveals **Circle Layout**:

- **Mode 1 — Classic** (`"circle_layout": "classic"`): the existing layout,
  including CPU/GPU load beside the temperature when available.
- **Mode 2 — Centered** (`"circle_layout": "centered"`): temperature and degree
  symbol centered together above the bar, with centered label and extra info
  below. Load can optionally appear beside the temperature.

Set `circle_layout` inside the `display` object. Missing or unsupported values
use Classic, so existing configurations keep their layout. Both layouts share
sensor slots, switch interval, and threshold colors.

**Display Elements** provides separate checkboxes for Circle in both layouts:

| Element | Key in `display` |
| --- | --- |
| Load (%) | `circle_show_load` |
| Fan / pump speed (RPM) | `circle_show_rpm` |
| Power (W) | `circle_show_watts` |
| Frequency (GHz / MHz) | `circle_show_frequency` |
| Temperature bar | `circle_show_bar` |

These flags only affect Circle. Hidden elements release their space; the
temperature and remaining text are resized and repositioned within the LCD
geometry. Hiding load centers the temperature in either Circle layout.
The old `circle_show_extra_info` setting remains a fallback for RPM, power and
frequency when their individual keys are missing. Explicit individual settings
take precedence. Old configurations keep the bar enabled and load enabled in
Classic, disabled in Centered. The UI saves the individual settings.

Extra lines depend on the available sensor channels. The UI preview uses fixed
example values and switches with the selected layout; it is not a live image.

### State

```c
static int current_slot = 0;      // 0→1→2→0
static time_t last_switch_time = 0;
```

### Centering

In Mode 2, temperature + degree symbol are measured and centered as a unit
within the safe display region. Conceptually:
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
