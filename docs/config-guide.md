# CoolerDash Configuration Guide

Config file: `/var/lib/coolercontrol/plugins/coolerdash/config.json`

Restart to apply changes:
```bash
sudo systemctl restart coolercontrold
```

---

## Daemon

```json
"daemon": {
    "address": "http://localhost:11987"
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `address` | `http://localhost:11987` | CoolerControl API endpoint |

Set the access token in the **Connection** tab. CoolerDash checks it against
CoolerControl before saving it to `credentials.json` and removing it from
`config.json`. A token that could not be verified or stored stays pending.
Leave the token field empty to keep the saved token.

The token update requires a Linux write lease on `config.json`. If a
CoolerControl save is in progress, let it finish and try again. Ordinary
configuration loads leave the file untouched.

---

## Paths

```json
"paths": {
    "images": "/var/lib/coolercontrol/plugins/coolerdash",
    "image_coolerdash": "/var/lib/coolercontrol/plugins/coolerdash/coolerdash.png",
    "image_shutdown": "/var/lib/coolercontrol/plugins/coolerdash/shutdown.png"
}
```

| Key | Description |
|-----|-------------|
| `images` | Directory for generated images |
| `image_coolerdash` | Generated display image path |
| `image_background` | Optional PNG, animated GIF, JPEG, BMP, or TIFF background |
| `image_shutdown` | PNG, GIF, JPEG, BMP, or TIFF image shown on daemon shutdown |

With a nonzero orientation, CoolerDash uploads a rotated PNG copy of the shutdown image. Animated shutdown images use their first frame; the source file is not changed.

---

## Display

```json
"display": {
    "mode": "dual",
    "width": 0,
    "height": 0,
    "refresh_interval": 3.5,
    "brightness": 80,
    "orientation": 0,
    "show_clock": false,
    "circle_switch_interval": 8,
    "content_scale_factor": 0.98,
    "dual_show_bars": true,
    "split_ring_by_sensor": false,
    "sensor_ring_enabled": false,
    "sensor_ring_sensor": "liquid",
    "sensor_slot_1": "cpu",
    "sensor_slot_2": "none",
    "sensor_slot_3": "gpu"
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `mode` | `dual` | `dual`, `split`, `circle`, `terminal`, `circle-rings`, or `custom` |
| `width` / `height` | `0` | Pixels. `0` = auto-detect from API |
| `refresh_interval` | `3.5` | Display update interval in seconds (0.01–60.0) |
| `brightness` | `80` | LCD brightness 0–100% |
| `orientation` | `0` | Clockwise software rotation, whole degrees `0`–`359`. CoolerControl receives `0`. The complete frame, including background images, rotates and fits the LCD; uncovered corners use the background color. Existing quarter-turn values remain valid. |
| `show_clock` | `false` | Show local time (HH:MM) below the layout in any display mode. |
| `tty_icon` | `large` | Prompt icon in TTY mode. Choose `large`, `small`, or `off`. |
| `circle_switch_interval` | `8` | Screen rotation interval in Circle and Circle Rings (1–60s) |
| `content_scale_factor` | `0.98` | Safe area percentage (0.5–1.0) |
| `dual_show_bars` | `true` | Show the temperature bars in Dual mode |
| `split_ring_by_sensor` | `true` in Split, `false` in Dual | Color both ring halves from the two visible layout sensors |
| `sensor_ring_enabled` | `false` | Add the sensor-value ring in Split, Dual, or Circle mode |
| `sensor_ring_sensor` | `liquid` | Sensor that controls the ring outside Circle mode; Circle follows its active sensor |
| `sensor_slot_1/2/3` | `cpu`/`none`/`gpu` in Dual | Sensor assignment per slot. Set all visible slots to `none` for a background-only display. |

---

## Custom Layouts

The [Layout Bank](display-modes.md#custom-layout) stores layouts in
`custom_layouts.items`. Set `custom_layouts.active_id` to one item's ID and
`display.mode` to `custom` to show it. An invalid or missing active layout
falls back to Split.

```json
{
  "active_id": "cpu",
  "items": [{
    "id": "cpu",
    "version": 1,
    "name": "CPU",
    "background": "#141927",
    "elements": [{
      "type": "value", "x": 220, "y": 400, "w": 560, "h": 200,
      "source": "cpu", "text": "CPU", "size": 32,
      "font": "mono", "color": "#e9f1ff"
    }]
  }]
}
```

The coordinates use a 0–1000 scale relative to the LCD's width and height.
Later elements draw over earlier ones. The font size is in pixels at 240 px.
Set `font` on a value, text, or clock widget to a system font family. The
aliases `sans` and `mono` select the system's default sans-serif and monospace
families. If a named family is unavailable, the system selects a fallback.
For a sensor widget, the optional `channel` field selects `duty`, `watts`,
or `rpm` from the same device as its `source`. This works with
`cpu`, `gpu`, `liquid`, and device-specific sensor IDs.
An exported file contains a single item with its `id` omitted.

Set a screen's `arrangement` to `columns` to place its widgets automatically.
Set each widget's `column` to `0` for left or `1` for right. A frame with
`column: -1` separates the two occupied columns. In this arrangement, `h`
sets the preferred height in thousandths of a column. Set `grow: true` to
share the remaining height. Layouts with free positioning use `x`, `y`, `w`,
and `h` for their widget coordinates.

A layout with several screens uses `version: 2` and a `screens` array of one
to five objects. Each screen has its own `elements` array with up to 24
widgets and its own `duration` from 1 to 60 seconds. It defaults to 6 seconds.
An existing `switch_interval` supplies the duration for any screen without
its own value. Single-screen layouts with the default duration can use
version 1 and the top-level `elements` array shown above.

Ring widgets accept these optional fields:

| Key | Default | Range and meaning |
|-----|---------|-------------------|
| `ring_thickness` | `0` | Thickness at 240 px, from 0 to 120. Use 0 for automatic sizing. |
| `ring_start` | `0` | Start angle clockwise from the top, from 0° to 359°. |
| `ring_sweep` | `360` | Total arc length, from 1° to 360°. |
| `segments` | `1` | Segment count, from 1 to 24. Use 1 for a continuous ring. |

---

## Layout

```json
"layout": {
    "bar_height": 24,
    "bar_width": 98,
    "bar_gap": 12,
    "bar_border": 1.0,
    "ring_width": 0,
    "ring_opacity": 1.0,
    "ring_rounded_corners": true,
    "bar_border_enabled": 1,
    "label_margin_left": 1,
    "label_margin_bar": 1,
    "bar_height_1": 0,
    "bar_height_2": 0,
    "bar_height_3": 0
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `bar_height` | `20` | Bar height (px) |
| `bar_width` | `98` | Bar width (% of display width) |
| `bar_gap` | `10` | Gap between bars (px) |
| `bar_border` | `0.6` | Border thickness (px) |
| `ring_width` | `0` | Ring thickness in scaled logical units; `0` selects automatic sizing |
| `ring_opacity` | `1.0` | Opacity of the complete ring (`0.0`–`1.0`) |
| `ring_rounded_corners` | `true` | Round the corners of rectangular sensor rings; `false` uses square corners |
| `bar_border_enabled` | `1` | Border on/off (`1`/`0`) |
| `label_margin_left` | `1` | Left label margin multiplier |
| `label_margin_bar` | `1` | Label-to-bar margin multiplier |
| `bar_height_1/2/3` | `0` | Per-slot height override. `0` = use `bar_height` |

---

## Colors

RGB values (0–255):

```json
"colors": {
    "bar_background":  { "r": 52,  "g": 52,  "b": 52 },
    "bar_border":      { "r": 192, "g": 192, "b": 192 },
    "font_temp":       { "r": 255, "g": 255, "b": 255 },
    "font_label":      { "r": 200, "g": 200, "b": 200 }
}
```

---

## Font

```json
"font": {
    "face": "Roboto Black",
    "tty_face": "Roboto Black",
    "rings_face": "",
    "size_temp": 0,
    "size_labels": 0,
    "font_growth_factor": 1.33
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `face` | `Roboto Black` | Font family for Dual, Split, and Circle |
| `tty_face` | `Roboto Black` | Font family for TTY. The font picker also accepts manual entries. |
| `rings_face` | `""` | Font family for Circle Rings. An empty value uses Roboto Black from the preset. |
| `size_temp` / `size_labels` | `0` | Font size (pt). `0` = auto-scale from resolution |
| `font_growth_factor` | `1.33` | Scaling multiplier for auto-sized fonts |

---

## Temperature Zones

Four color zones per sensor. Bars transition through colors as temperature rises.

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
        "threshold_4_color": { "r": 255, "g": 0,   "b": 0 },
        "offset_x": 0,
        "offset_y": 0
    }
}
```

`max_scale` = temperature at 100% bar fill. Same structure for `gpu` and `liquid`.

---

## Positioning

```json
"positioning": {
    "degree_spacing": 16,
    "label_offset_x": 0,
    "label_offset_y": 0,
    "margin_top": 0,
    "margin_bottom": 0
}
```

---

## Troubleshooting

- **Display not updating**: Check `refresh_interval`, restart CoolerControl
- **Wrong colors**: RGB values must be 0–255
- **Text clipped**: Lower `content_scale_factor`
- **Bars too wide**: Lower `bar_width`

Full default config: `/var/lib/coolercontrol/plugins/coolerdash/config.json`
