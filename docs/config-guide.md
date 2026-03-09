# CoolerDash Configuration Guide

Complete guide for configuring CoolerDash through the `config.json` file.

## Configuration File Location

```
/etc/coolercontrol/plugins/coolerdash/config.json
```

## Applying Changes

Restaring CoolerControl reloads the plugin and picks up config changes:
```bash
sudo systemctl restart coolercontrold
```

---

## Daemon Settings

Connection to the CoolerControl daemon.

```json
"daemon": {
    "address": "http://localhost:11987",
    "access_token": "",
    "password": "coolAdmin"
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `address` | `http://localhost:11987` | API endpoint |
| `access_token` | `""` | **CC4:** Bearer token from CoolerControl UI → Access Protection. Format: `cc_<uuid>`. Takes precedence over `password`. |
| `password` | `coolAdmin` | **CC3 / fallback.** Ignored when `access_token` is set. |

> CC4: generate a token in CoolerControl UI under **Access Protection** and paste it into `access_token`.  
> CC3: leave `access_token` empty, set `password`.

---

## File Paths

System file and directory locations.

### Example
```json
"paths": {
    "images": "/etc/coolercontrol/plugins/coolerdash",
    "image_coolerdash": "/etc/coolercontrol/plugins/coolerdash/coolerdash.png",
    "image_shutdown": "/etc/coolercontrol/plugins/coolerdash/shutdown.png"
}
```

### Settings
- **`images`**: Directory for generated display images
- **`image_coolerdash`**: Generated display image path
- **`image_shutdown`**: Image displayed on daemon shutdown

---

## 🖥️ Display Settings

LCD display configuration tested with NZXT Kraken 2023.

### Basic Configuration
```json
"display": {
    "mode": "dual",
    "width": 0,
    "height": 0,
    "refresh_interval": 2.5,
    "brightness": 80,
    "orientation": 0,
    "shape": "auto",
    "circle_switch_interval": 5,
    "content_scale_factor": 0.98,
    "inscribe_factor": 0.70710678
}
```

> `width`/`height` set to `0` means CoolerDash queries the actual device dimensions from the API at startup.

### Settings
- **`mode`**: Display mode: `dual` (default) or `circle`
- **`width`** / **`height`**: Screen dimensions in pixels; `0` = auto-detect from API
- **`refresh_interval`**: Update interval in seconds (0.01–60.0, default: `2.5`)
- **`brightness`**: LCD brightness 0–100% (default: `80`)
- **`orientation`**: Screen rotation: `0`, `90`, `180`, `270` degrees
- **`shape`**: Display shape: `auto` (default), `rectangular`, or `circular`
- **`circle_switch_interval`**: Slot switch interval for circle mode in seconds (1–60, default: `5`)
- **`content_scale_factor`**: Safe area percentage (0.5–1.0, default: `0.98`)
- **`inscribe_factor`**: Inscribe factor for circular displays (default: `0.70710678` = 1/√2)
- **`sensor_slot_up`**: Sensor shown in top slot: `cpu`, `gpu`, or `liquid` (default: `cpu`)
- **`sensor_slot_mid`**: Sensor shown in middle slot (default: `liquid`)
- **`sensor_slot_down`**: Sensor shown in bottom slot (default: `gpu`)

Sensor slots control which sensor appears in each display position for both dual and circle mode.

### Brightness Examples
```json
"brightness": 40,   // dim for night use
"brightness": 80,   // recommended default
"brightness": 100   // maximum
```

### Refresh Rate Examples
```json
"refresh_interval": 1.0,  // fast
"refresh_interval": 2.5,  // default
"refresh_interval": 5.0   // power-saving
```

### Display Shape Override

The `shape` parameter allows manual control of the **inscribe factor** used for layout calculations:

- **`auto`** (default): Automatic detection based on device database
- **`rectangular`**: Force inscribe factor = 1.0 (use full display width)
- **`circular`**: Force inscribe factor = 0.7071 (inscribed square for round displays)

**When to use:**
- Testing different layouts on your display
- Troubleshooting clipping issues on circular displays
- Overriding auto-detection if it's incorrect for your device

**Examples:**
```json
"shape": "auto"        // recommended
"shape": "rectangular" // full width, inscribe_factor = 1.0
"shape": "circular"    // inscribed square, inscribe_factor = 0.7071
```

**Priority:** `shape` config > auto-detection

**Note:** See [Display Detection Guide](display-detection.md) for technical details about inscribe factors.

#### Circle Mode Sensor Switching

**Parameter:** `circle_switch_interval`

Controls how frequently the circle mode rotates between available sensors:

- **Range:** 1-60 seconds
- **Default:** 5 seconds
- **Applies to:** Circle mode only

**Example:**
```json
"mode": "circle",
"circle_switch_interval": 10
```

**Use Cases:**
- **Fast switching (1-3s):** Quick overview of all sensors
- **Moderate (5-8s):** Default balanced viewing
- **Slow switching (10-60s):** Focus on individual sensors longer

#### Content Scale Factor

**Parameter:** `content_scale_factor`

Controls the safe area percentage used for rendering content (determines margin/padding):

- **Range:** 0.5-1.0 (50%-100%)
- **Default:** 0.98 (98% = 2% margin)
- **Applies to:** Both dual and circle modes

**Example:**
```json
"content_scale_factor": 0.95
```

**Use Cases:**
- **0.98-1.0:** Minimal margins, maximum screen usage
- **0.90-0.97:** Comfortable padding, safe for text rendering
- **0.70-0.89:** Extra margins, conservative layout
- **0.5-0.69:** Large margins, centered content focus

**Visual Impact:**
- Higher values (0.95-1.0) = content fills more screen area, less padding
- Lower values (0.5-0.8) = more white space around content, safer margins

---

## Visual Layout

All values are in pixels unless noted. Positions are calculated dynamically from display dimensions.

```json
"layout": {
    "bar_height": 24,
    "bar_width": 98,
    "bar_gap": 12,
    "bar_border": 1.0,
    "bar_border_enabled": 1,
    "label_margin_left": 1,
    "label_margin_bar": 1,
    "bar_height_up": 0,
    "bar_height_mid": 0,
    "bar_height_down": 0
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `bar_height` | `24` | Bar height in pixels |
| `bar_width` | `98` | Bar width in % of display width |
| `bar_gap` | `12` | Gap between bars in pixels |
| `bar_border` | `1.0` | Border thickness in pixels |
| `bar_border_enabled` | `1` | Enable bar border (`1`/`0`) |
| `label_margin_left` | `1` | Left label margin multiplier |
| `label_margin_bar` | `1` | Margin between label and bar |
| `bar_height_up/mid/down` | `0` | Per-slot bar height override. `0` = use `bar_height` |

---

## Colors

RGB color values (0–255).

```json
"colors": {
    "bar_background":  { "r": 52,  "g": 52,  "b": 52 },
    "bar_border":      { "r": 192, "g": 192, "b": 192 },
    "font_temp":       { "r": 255, "g": 255, "b": 255 },
    "font_label":      { "r": 200, "g": 200, "b": 200 }
}
```

---

## Font Settings

```json
"font": {
    "face": "Roboto Black",
    "size_temp": 0,
    "size_labels": 0
}
```

- **`face`**: Font family name (must be installed, default: `Roboto Black`)
- **`size_temp`** / **`size_labels`**: Font size in points. `0` = auto-scale based on display resolution
  - 240×240: ~100pt temp / ~30pt labels
  - Formula: `100.0 × (width + height) / (2 × 240.0)`

---

## Temperature Zones

Four color zones based on temperature thresholds. Configured per-sensor in the `sensors` section.

```json
"sensors": {
    "cpu": {
        "threshold_1": 55.0,
        "threshold_2": 65.0,
        "threshold_3": 75.0,
        "max_scale": 115.0,
        "threshold_1_color": { "r": 0,   "g": 255, "b": 0 },
        "threshold_2_color": { "r": 255, "g": 140, "b": 0 },
        "threshold_3_color": { "r": 255, "g": 70,  "b": 0 },
        "threshold_4_color": { "r": 255, "g": 0,   "b": 0 }
    },
    "gpu": { ... }
}
```

`max_scale` sets the temperature at 100% bar fill. Bars transition through 4 color zones as temperature rises through the thresholds.

---

## Complete Example

See the default config at `/etc/coolercontrol/plugins/coolerdash/config.json` for a full reference. The file is installed with all options and their defaults.

---

## Troubleshooting

1. **Display not updating**: Check `refresh_interval` and restart CoolerControl
2. **Wrong colors**: Verify RGB values are 0–255
3. **Text clipped on circular display**: Adjust `inscribe_factor` and `content_scale_factor`
4. **Bars too wide**: Lower `bar_width` value

### Backup

```bash
sudo cp /etc/coolercontrol/plugins/coolerdash/config.json \
        /etc/coolercontrol/plugins/coolerdash/config.json.backup
```

## Developer: Scaling Unit Test

```bash
gcc -std=c99 -Iinclude -I./src -o build/test_scaling tests/test_scaling.c -lm
./build/test_scaling
```
