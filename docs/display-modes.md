# Display Modes

CoolerDash offers **Dual** (default), **Split**, **Circle**, **TTY (Terminal)**,
**Circle Rings**, and **Custom Layout**.

Split, Dual, and Circle can add the optional **Sensor Ring** overlay. Split and Dual
can instead divide the ring between their two visible sensors. In Circle mode,
the normal ring follows the sensor currently shown on the display. In other
modes, `display.sensor_ring_sensor` selects its sensor and defaults to `liquid`.
The ring maps the current value to that sensor's configured maximum and uses
the same threshold colors as its bars and values. The overlay is off by default
for backwards-compatible output.

Choose a mode in the plugin or set `display.mode` in `config.json`. The CLI
supports `--split`, `--dual`, `--circle`, `--terminal`, and `--circle-rings`.

Enable **Show Clock** to display the local time at the bottom of any mode.
The option is off by default.

When you switch modes in the menu, the selected mode starts with its default
display elements and sensor assignments. Opening the plugin loads your saved
settings. You can change the options after selecting a mode and save them
for the LCD.

## TTY (Terminal)

Choose **Display → TTY (Terminal)** for a monospace dashboard that fits
the detected LCD. CPU and GPU rows show temperature and segmented load bars.
The remaining rows show liquid temperature, processor power, and RAM used on
the host running CoolerDash.

The power row sums the available CPU and GPU readings. When only one is
available, the row identifies that processor. Missing readings show a dash,
and unavailable load bars show an empty track. The mode uses the standard
CoolerControl sensors and uses the configured LCD background color, image or GIF.
**TTY Icon** offers a large prompt tile or a small, left-aligned prompt.
**Off** hides both the prompt and the LOAD heading.
## Circle Rings

Choose **Display → Circle Rings** to cycle through liquid, CPU, and GPU screens.
The outer ring shows temperature on a 0–100 °C scale. The inner ring shows
pump duty on the liquid screen and load on the CPU and GPU screens. Both
readings appear in the center. A missing reading shows a dash and an empty
ring. Set **Switch Interval** to control how long each screen stays visible.

The mode uses the configured LCD background. Select **Edit as Custom Layout**
to start with an editable copy of all three screens, the current background
color, and the switch interval. Save to activate your copy. The standard
mode and the editor use the same layout from `presets.json`. The previous
`activity-rings` configuration value and `--activity-rings` CLI flag still work.

## Custom Layout

Open **Display → Custom Layout** in the CoolerControl plugin. Start with an
editable copy of **Dual**, **Split**, **Circle**, **TTY (Terminal)**, or
**Circle Rings**, or create a blank layout with **New**. The five editable
templates are defined in `presets.json`.

In Circle mode, **Edit as Custom Layout** creates one screen per active
sensor slot in slot order. It copies the switch interval, sensor labels, and
temperature bar setting. Enabled load, frequency, power, and fan or pump
speed readings become separate value widgets. An empty slot is skipped.
With no active slots, the copy contains one blank screen.

Use **Add Screen** to create up to five screens in a layout. Select a numbered
preview to edit that screen's widgets. Under **Replace Screen**, pick a
template or copy another screen and click **Apply**. A template replaces the
widgets. A screen copy includes the widgets and duration.

The LCD cycles through the screens in order. Set **Screen Duration** for
each screen from 1 to 60 seconds. New screens start at 6 seconds. The layout
background applies to all its screens.

Under **Arrangement**, choose **Automatic Columns** to let the LCD place
widgets in two columns. Move a widget by changing its column or using
**Move Up** and **Move Down**. Set its preferred height or let it fill the
remaining space. An empty column releases its space to the other one.
Choose **Free** to place widgets directly on the screen.

Click a widget to add it in the center, or drag it from the palette onto the
display. Drag a widget to move it and use its corner handle to resize it.
You can also edit its coordinates and size in the inspector or use the arrow
keys once the widget has focus. Hold Alt to resize with the arrow keys.
Press **Delete** while a widget or its entry in the layer list has focus to
remove it.

Choose a local sensor in the inspector for each value, bar, or ring. A missing
reading displays a dash or an empty gauge. A ring turns the sensor's range
into a full circle or an arc. Set the thickness, start angle, arc length,
number of segments, and color in the inspector. The Cairo preview uses sample
readings. Use **1:1 pixels** to check readability and **LCD orientation** to
preview the configured rotation.

Give the layout a name distinct from the built-in templates. Template copies
get their own names when you create them. Select a layout in the bank and click **Save** to
apply it to the LCD. **Export JSON** shows the selected layout with its colors
and widgets. In CoolerControl, copy the JSON into a `.json` file to share it.
Imported layouts appear as new entries. Built-in temperature, processor load,
power, and RAM sources keep their bindings. Assign any device-specific sources
again after import. Background images and GIFs come from the local background
setting. Resetting plugin settings keeps saved layouts and selects Dual.

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

The primary preview sends unsaved settings to CoolerDash and uses the production
Cairo renderer with deterministic example values. If the plugin endpoint is not
available, the embedded SVG preview uses static example values and recalculates
its layout when elements are toggled. The device renderer uses its detected
geometry and configured font limits when allocating the freed space; large
values and long labels are fitted within their regions.

The optional sensor ring follows the detected display geometry. Circular
displays use a 270-degree arc. Rectangular and rounded-square displays use an
inset rounded frame whose stroke stays inside the rendered image.

Split and Dual modes can divide that ring into two equal halves. Each half uses
one visible layout sensor and the same temperature-threshold colors as its bar.
Split temperature numbers stay white so the ring alone carries the threshold
state. Split load, power and RPM values use the configured label color. This
option overrides the single-sensor ring and is enabled by default in Split. Enabling
either ring disables the other, so only one ring type is saved. Ring thickness,
opacity and label color are configurable in the Layout tab.

## Files

```
src/mods/
├── background.c/h    # Image loading, backgrounds and native GIF playback
├── bar.c/h           # Temperature and segmented bars
├── circle.c/h        # Circle screen selection
├── circle_render.c/h # Circle's sensor layout and widget styles
├── circle_text.c/h   # Circle's reading formats and text sizing
├── custom.c/h        # Custom mode
├── display.c/h       # Mode dispatch, preview samples and LCD upload
├── dual.c/h          # Dual mode
├── elements.c/h      # Widget model and layout JSON validation
├── layout.c/h        # Display geometry, widget placement and screen timing
├── readings.c/h      # Sensor and system readings for widgets
├── render.c/h        # Cairo widgets and frame output
├── ring.c/h          # Widget rings and sensor rings at the display edge
├── rings.c/h         # Circle Rings mode
├── split.c/h         # Split mode
├── stream.c/h        # USB streaming and GIF frames
├── text.c/h          # Label and sensor value typography
└── tty.c/h           # TTY mode dashboard
```

The dispatcher uses the same renderer for an LCD frame and its PNG preview.
The TTY renderer takes its rows, sensor bindings, and bars from the same
`presets.json` entry the editor copies.

---

## Dual Mode

Dual stacks two sensor rows vertically. The built-in renderer fits the text
and bars to the LCD and applies the sensor thresholds.

Selecting **Dual** in the mode menu starts with **Temperature Bars** enabled.
**Show Clock**, **Split Sensor Ring**, and **Sensor Ring** start off.

Select **Edit as Custom Layout** to copy the rows from `presets.json` with
their configured sensors, labels, font, and text colors. The copy uses
editable widget positions. Each bar starts with the sensor's configured
scale and its lowest threshold color.

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

## Split Mode

Split shows two sensor columns with a vertical divider. The renderer fits
the columns to the LCD. The preset defines their default labels, font-size
ratios, and secondary readings.

Select **Edit as Custom Layout** to copy the active sensors, labels, font,
and enabled load, power, and speed rows. The copy starts with **Automatic
Columns** and uses the same spacing as Split. Remove a reading to give the
remaining widgets more space. The **Channel** field selects a reading from
the bound device. CPU speed follows the cooler's RPM channel.

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
example values and switches with the selected layout.

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
