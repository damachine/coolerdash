# CoolerDash Configuration Guide

Config file: `/etc/coolercontrol/plugins/coolerdash/config.json`

Restart to apply changes:
```bash
sudo systemctl restart coolercontrold
```

---

## Daemon

```json
"daemon": {
    "address": "http://localhost:11987",
    "access_token": ""
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `address` | `http://localhost:11987` | CoolerControl API endpoint |
| `access_token` | `""` | Bearer token (`cc_<uuid>`) from CoolerControl UI > Access Protection |

---

## Paths

```json
"paths": {
    "images": "/etc/coolercontrol/plugins/coolerdash",
    "image_coolerdash": "/etc/coolercontrol/plugins/coolerdash/coolerdash.png",
    "image_shutdown": "/etc/coolercontrol/plugins/coolerdash/shutdown.png"
}
```

| Key | Description |
|-----|-------------|
| `images` | Directory for generated images |
| `image_coolerdash` | Generated display image path |
| `image_shutdown` | Image shown on daemon shutdown |

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
    "shape": "auto",
    "circle_switch_interval": 8,
    "content_scale_factor": 0.98,
    "inscribe_factor": 0.70710678,
    "sensor_slot_1": "cpu",
    "sensor_slot_2": "liquid",
    "sensor_slot_3": "gpu"
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `mode` | `dual` | `dual` or `circle` |
| `width` / `height` | `0` | Pixels. `0` = auto-detect from API |
| `refresh_interval` | `3.5` | Update interval in seconds (0.01–60.0) |
| `brightness` | `80` | LCD brightness 0–100% |
| `orientation` | `0` | Rotation: `0`, `90`, `180`, `270` |
| `shape` | `auto` | `auto`, `rectangular`, `circular` |
| `circle_switch_interval` | `8` | Sensor rotation interval in circle mode (1–60s) |
| `content_scale_factor` | `0.98` | Safe area percentage (0.5–1.0) |
| `inscribe_factor` | `0.70710678` | Inscribe factor for circular displays (1/√2) |
| `sensor_slot_1/2/3` | `cpu`/`liquid`/`gpu` | Sensor assignment per slot |

`shape` overrides auto-detection. Priority: `shape` config > auto-detection.

---

## Layout

```json
"layout": {
    "bar_height": 24,
    "bar_width": 98,
    "bar_gap": 12,
    "bar_border": 1.0,
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
| `bar_height` | `24` | Bar height (px) |
| `bar_width` | `98` | Bar width (% of display width) |
| `bar_gap` | `12` | Gap between bars (px) |
| `bar_border` | `1.0` | Border thickness (px) |
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
    "size_temp": 0,
    "size_labels": 0,
    "font_growth_factor": 1.33
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `face` | `Roboto Black` | Font family (must be installed) |
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
- **Text clipped**: Adjust `inscribe_factor` and `content_scale_factor`
- **Bars too wide**: Lower `bar_width`

Full default config: `/etc/coolercontrol/plugins/coolerdash/config.json`
