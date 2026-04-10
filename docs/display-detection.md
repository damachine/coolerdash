# Display Shape Detection

Determines whether a display is circular or rectangular. Affects inscribe factor and layout calculations.

## Config Override

```json
"display": {
    "shape": "auto"
}
```

Values: `"auto"` (default), `"rectangular"`, `"circular"`

Priority: `shape` config > auto-detection.

## Auto-Detection

### NZXT Kraken (resolution-based)

```c
// ≤240×240 → rectangular (inscribe_factor = 1.0)
// >240×240  → circular   (inscribe_factor = 0.7071)
```

### Other Devices

Checked against `circular_devices[]` database in `cc_conf.c`. Unknown devices default to rectangular.

## Inscribe Factor (1/√2)

Circular displays need content within an inscribed square to prevent clipping.

```
Circle radius = R
Inscribed square side = R × √2
Ratio = 1/√2 ≈ 0.7071
```

Configurable via `inscribe_factor` in `config.json` (default: 0.70710678, `0` = auto).

### Calculation

| Display | Resolution | Shape | inscribe_factor | safe_area | margin |
|---------|-----------|-------|-----------------|-----------|--------|
| Kraken 2023 | 240×240 | rectangular | 1.0 | 240px | ~2px |
| Kraken Z | 320×320 | circular | 0.7071 | ~226px | ~47px |

## Adding Devices

### Circular (non-Kraken)

Add to `circular_devices[]` in `src/srv/cc_conf.c`:

```c
const char *circular_devices[] = {
    "Your Device Name",
};
```

Name matching uses `strstr` — partial matches work.

### NZXT Kraken

No database entry needed. Resolution-based detection is automatic.

### Rectangular

No action needed. Unknown devices default to rectangular.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Clipping on circular display | Set `"shape": "circular"` in config |
| Too much padding on rectangular | Set `"shape": "rectangular"` in config |
| Unknown device | Set `shape` manually or add to database |
