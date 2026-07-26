# Display Geometry Detection

CoolerDash resolves known LCD geometry from immutable device profiles in
`src/device/profile.c`. The profile controls shape, native resolution,
visible diameter, center point, and preferred integration transport metadata.

## Match Priority

1. USB VID:PID, when the device API exposes it
2. Model-name tokens plus exact native resolution
3. Legacy size fallback for unknown devices

CoolerControl's current `/devices` response does not expose USB IDs, so the
second path is normally used today. Keeping VID:PID in the same profile makes
the first path available without redesigning the renderer later.

## Included NZXT Profiles

| USB ID | Model | Resolution | Shape | Preferred integration transport |
|--------|-------|------------|-------|----------------------------|
| `1e71:3008` | Kraken Z series | 320×320 | circular | image upload |
| `1e71:300c` | Kraken 2023 Elite | 640×640 | circular | image upload |
| `1e71:300e` | Kraken 2023 | 240×240 | rectangular | image upload |
| `1e71:3012` | Kraken Elite (2024), RGB and non-RGB | 640×640 | circular | image upload |
| `1e71:3014` | Kraken Plus (2025), RGB and non-RGB | 240×240 | rectangular | image upload |

Transport values are capability metadata only. CoolerDash still sends rendered
PNG files through CoolerControl.

The USB IDs and native resolutions follow liquidctl's device table. Product
generation names and panel specifications follow NZXT's published specs. Panel
refresh rate is not the same as a safe image-upload rate, so
`recommended_fps` remains `0` until continuous updates have been verified on
real hardware.

## Circular Safe Area

Circular layouts use the actual vertical position and height of each element.
For a region spanning `y_top` to `y_bottom`, the limiting distance is:

```text
d = max(abs(y_top - center_y), abs(y_bottom - center_y))
width = 2 × sqrt(radius² - d²)
```

This gives each bar the widest chord that remains inside the visible circle.
Elements near the center can use more width; elements near the top or bottom
are narrowed automatically. `content_scale_factor` remains the global inset.

## Adding a Device

Add one `DisplayProfile` entry in `src/device/profile.c` with:

- canonical VID:PID
- exact native width and height
- shape
- visible diameter and center ratios
- preferred transport metadata
- conservative model-name match tokens

Use `recommended_fps = 0` until a stable rate has been verified on hardware.
Unknown displays retain the old `>240px = circular` fallback to avoid breaking
existing installations.

Ask the contributor to run `coolerdash --hardware-report --test-lcd`. Report
schema 2 includes the CoolerControl model, driver and LCD capabilities,
liquidctl VID:PID/release/driver data, USB device/interface/endpoint
descriptors, firmware, and the contributor's observations of display shape,
visible calibration circle, centering, rotation, and distortion. USB
descriptors can confirm the available HID/bulk paths, but they cannot reveal
the pixel encoding or upload commands; those still require an upstream driver
implementation or a separately captured and reviewed protocol trace.
