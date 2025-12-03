# CoolerDash Configuration Guide

Complete guide for configuring CoolerDash through the `config.ini` file.

## 📍 Configuration File Location

The configuration file is located at:
```
/etc/coolerdash/config.ini
```

## 🔄 Applying Changes

After modifying the configuration file, restart CoolerDash:
```bash
sudo systemctl restart coolerdash
```

---

## 🌐 Daemon Settings

Configure connection to CoolerControl daemon API.

### Example Configuration
```ini
[daemon]
address=http://localhost:11987
password=coolAdmin
```

### Settings
- **`address`**: API endpoint URL (default: `http://localhost:11987`)
- **`password`**: API authentication password (default: `coolAdmin`)

### HTTPS Example
For secure connections:
```ini
[daemon]
address=https://192.168.1.100:11987
password=mySecurePassword123
```

---

## 📁 File Paths

System file and directory locations.

### Example Configuration
```ini
[paths]
images=/opt/coolerdash/images
image_coolerdash=/tmp/coolerdash.png
image_shutdown=/opt/coolerdash/images/shutdown.png
pid=/tmp/coolerdash.pid
```

### Settings
- **`images`**: Directory containing display images
- **`image_coolerdash`**: Generated display image path
- **`image_shutdown`**: Shutdown screen image
- **`pid`**: Process ID file location

---

## 🖥️ Display Settings

LCD display configuration tested with NZXT Kraken 2023.

### Basic Configuration
```ini
[display]
width=240
height=240
refresh_interval=2.50
brightness=80
orientation=0
shape=auto
mode=dual
circle_switch_interval=5
content_scale_factor=0.98

# Optional: Custom inscribe factor for circular displays
# - Range: >=0 && <=1.0
# - 0.0 means "auto" (use geometric inscribe factor 1/√2 ≈ 0.7071)
# - Default: 0.70710678 (geometric inscribe)
# - Example values: 0.70710678 (geometric), 0.85 (more usable area)
# inscribe_factor=0.70710678
```

### Settings
- **`width`**: Screen width in pixels (240 for NZXT Kraken)
- **`height`**: Screen height in pixels (240 for NZXT Kraken)
- **`refresh_interval`**: Update interval in seconds with 2 decimal places (0.01-60.0, default: 2.50)
- **`brightness`**: LCD brightness 0-100% (80% recommended)
- **`orientation`**: Screen rotation: `0`, `90`, `180`, `270` degrees
- **`shape`**: Display shape override: `auto` (default), `rectangular`, or `circular`
- **`mode`**: Display mode: `dual` (default) or `circle`
- **`circle_switch_interval`**: Circle mode sensor switch interval in seconds (1-60, default: 5)
- **`content_scale_factor`**: Content scale factor (0.5-1.0, default: 0.98) - percentage of safe area used for content

### Brightness Examples
```ini
# Dim for night use
brightness=40

# Standard brightness (recommended)
brightness=80

# Maximum brightness
brightness=100
```

### Refresh Rate Examples
```ini
# Fast updates (1 second)
refresh_interval=1.00

# Standard updates (2.5 seconds) - default
refresh_interval=2.50

# Moderate updates (3.5 seconds)
refresh_interval=3.50

# Slow updates (5 seconds) - saves power
refresh_interval=5.00
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
```ini
# Auto-detection (recommended)
shape=auto

# Force rectangular layout (full width, inscribe factor = 1.0)
shape=rectangular

# Force circular layout (inscribed square, inscribe factor = 0.7071)
shape=circular
```

**Priority:** `shape` config > `--force-display-circular` CLI flag > auto-detection

**Note:** See [Display Detection Guide](display-detection.md) for technical details about inscribe factors.

#### Circle Mode Sensor Switching

**Parameter:** `circle_switch_interval`

Controls how frequently the circle mode rotates between available sensors:

- **Range:** 1-60 seconds
- **Default:** 5 seconds
- **Applies to:** Circle mode only

**Example:**
```ini
[display]
mode=circle
circle_switch_interval=10  # Switch every 10 seconds
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
```ini
[display]
content_scale_factor=0.95  # 5% margin for more padding
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

## 🎨 Visual Layout

Display layout and spacing configuration. All positioning is now calculated dynamically from display dimensions.

### Example Configuration
```ini
[layout]
bar_height=24
bar_width=98
bar_gap=12
bar_border=2.0
```

### Settings
- **`bar_height`**: Temperature bar thickness in pixels (default: auto-scaled)
- **`bar_width`**: Bar width as percentage of display width (1-100%, default: 98%)
  - 98% = 1% margin left + 1% margin right
  - 50% = centered bar with 25% margin on each side
  - 100% = full width, no margins
- **`bar_gap`**: Space between temperature bars in pixels (default: auto-scaled)
- **`bar_border`**: Border line thickness in pixels (default: 1.0)

### Layout Examples

#### Compact Layout
```ini
[layout]
bar_height=18
bar_width=90
bar_gap=5
bar_border=1.0
```

#### Spacious Layout
```ini
[layout]
bar_height=26
bar_width=98
bar_gap=15
bar_border=2.0
```

#### Custom Width Example (Centered)
```ini
[layout]
bar_height=24
bar_width=60     # 60% width = 20% margin on each side
bar_gap=12
bar_border=1.5
```

---

## 🎨 Colors

RGB color configuration (values 0-255).

### Bar Colors
```ini
[bar_color_background]
r=52    # Dark gray
g=52
b=52

[bar_color_border]
r=192   # Light gray
g=192
b=192
```

### Color Examples

#### Dark Theme
```ini
[bar_color_background]
r=25    # Very dark gray
g=25
b=25

[bar_color_border]
r=100   # Medium gray
g=100
b=100
```

#### Blue Theme
```ini
[bar_color_background]
r=20    # Dark blue
g=30
b=60

[bar_color_border]
r=70    # Light blue
g=130
b=180
```

#### Custom RGB Colors
```ini
# Pure black background
[bar_color_background]
r=0
g=0
b=0

# White border
[bar_color_border]
r=255
g=255
b=255
```

---

## 🔤 Font Settings

Text appearance configuration with automatic display-size-dependent defaults.

### Example Configuration
```ini
[font]
font_face=Roboto Black
font_size_temp=100.0
font_size_labels=30.0

[font_color_temp]
r=255   # White temperature text
g=255
b=255

[font_color_label]
r=200   # Light gray labels
g=200
b=200
```

### Settings
- **`font_face`**: Font family name (must be installed on system, default: Roboto Black)
- **`font_size_temp`**: Temperature number size in points
  - **Dynamic defaults** based on display resolution:
    - 240x240: 100.0pt (base size)
    - 320x320: 133.3pt (automatically scaled)
    - 480x480: 200.0pt (automatically scaled)
  - Formula: `100.0 × ((width + height) / (2 × 240.0))`
- **`font_size_labels`**: CPU/GPU label size in points
  - **Dynamic defaults** based on display resolution:
    - 240x240: 30.0pt (base size)
    - 320x320: 40.0pt (automatically scaled)
    - 480x480: 60.0pt (automatically scaled)
  - Formula: `30.0 × ((width + height) / (2 × 240.0))`

**Note:** If you set custom values in config.ini, they override the automatic scaling.

### Font Examples

#### Large Text Setup (Manual Override)
```ini
[font]
font_face=Arial Bold
font_size_temp=150.0    # Override auto-scaling with fixed large size
font_size_labels=45.0   # Override auto-scaling
```

#### Compact Text Setup (Manual Override)
```ini
[font]
font_face=Helvetica
font_size_temp=80.0     # Override auto-scaling with fixed small size
font_size_labels=24.0   # Override auto-scaling
```

#### Use Auto-Scaling (Recommended)
```ini
[font]
font_face=Roboto Black
# Comment out or remove font_size_* lines to use automatic scaling
# ;font_size_temp=100.0    # Automatically scaled based on display size
# ;font_size_labels=30.0   # Automatically scaled based on display size
```

#### Color Examples
```ini
# Bright white text
[font_color_temp]
r=255
g=255
b=255

# Cyan labels
[font_color_label]
r=0
g=255
b=255
```

---

## 🌡️ Temperature Zones

Color-coded temperature thresholds and their colors.

### Example Configuration
```ini
[temperature]
temp_threshold_1=55.0   # Cool zone (< 55°C)
temp_threshold_2=65.0   # Warm zone (55-65°C)
temp_threshold_3=75.0   # Hot zone (65-75°C)
                        # Critical zone (> 75°C)

[temp_threshold_1_bar]  # 💚 Cool (Green)
r=0
g=255
b=0

[temp_threshold_2_bar]  # 🧡 Warm (Orange)
r=255
g=140
b=0

[temp_threshold_3_bar]  # 🔥 Hot (Dark Orange)
r=255
g=70
b=0

[temp_threshold_4_bar]  # 🚨 Critical (Red)
r=255
g=0
b=0
```

### Temperature Threshold Examples

#### Conservative (Lower Thresholds)
```ini
[temperature]
temp_threshold_1=45.0   # Cool < 45°C
temp_threshold_2=55.0   # Warm 45-55°C
temp_threshold_3=65.0   # Hot 55-65°C
                        # Critical > 65°C
```

#### Aggressive (Higher Thresholds)
```ini
[temperature]
temp_threshold_1=60.0   # Cool < 60°C
temp_threshold_2=70.0   # Warm 60-70°C
temp_threshold_3=80.0   # Hot 70-80°C
                        # Critical > 80°C
```

### Color Scheme Examples

#### Rainbow Theme
```ini
[temp_threshold_1_bar]  # Blue (Cool)
r=0
g=100
b=255

[temp_threshold_2_bar]  # Yellow (Warm)
r=255
g=255
b=0

[temp_threshold_3_bar]  # Orange (Hot)
r=255
g=128
b=0

[temp_threshold_4_bar]  # Red (Critical)
r=255
g=0
b=0
```

#### Monochrome Theme
```ini
[temp_threshold_1_bar]  # Light gray
r=200
g=200
b=200

[temp_threshold_2_bar]  # Medium gray
r=150
g=150
b=150

[temp_threshold_3_bar]  # Dark gray
r=100
g=100
b=100

[temp_threshold_4_bar]  # Very dark gray
r=50
g=50
b=50
```

---

## 📋 Complete Example Configuration

Here's a complete working configuration file:

```ini
# ═══════════════════════════════════════════════════════════════════════════════
# CoolerDash Configuration - NZXT Kraken Setup
# ═══════════════════════════════════════════════════════════════════════════════

[daemon]
address=http://localhost:11987
password=coolAdmin

[paths]
images=/opt/coolerdash/images
image_coolerdash=/tmp/coolerdash.png
image_shutdown=/opt/coolerdash/images/shutdown.png
pid=/tmp/coolerdash.pid

[display]
width=240
height=240
refresh_interval=2.50
brightness=80
orientation=0
shape=auto
mode=dual
circle_switch_interval=5
content_scale_factor=0.98

[layout]
bar_height=24
bar_width=98
bar_gap=12
bar_border=2.0

[bar_color_background]
r=52
g=52
b=52

[bar_color_border]
r=192
g=192
b=192

[font]
font_face=Roboto Black
font_size_temp=100.0
font_size_labels=30.0

[font_color_temp]
r=255
g=255
b=255

[font_color_label]
r=200
g=200
b=200

[temperature]
temp_threshold_1=55.0
temp_threshold_2=65.0
temp_threshold_3=75.0

[temp_threshold_1_bar]
r=0
g=255
b=0

[temp_threshold_2_bar]
r=255
g=140
b=0

[temp_threshold_3_bar]
r=255
g=70
b=0

[temp_threshold_4_bar]
r=255
g=0
b=0
```

---

## 🎯 Advanced: Manual Position Offsets

Fine-tune element positions with pixel-level precision. All values are in pixels.

### Example Configuration
```ini
[display_positioning]
# Temperature numbers (CPU and GPU independent)
display_temp_offset_x=0,0      # Horizontal offset (single value=both, "cpu,gpu"=separate)
display_temp_offset_y=0,0      # Vertical offset (single value=both, "cpu,gpu"=separate)

# Degree symbol spacing
display_degree_spacing=16      # Distance from temperature number (default: 16px)

# CPU/GPU labels
display_label_offset_x=0       # Horizontal offset (+right, -left)
display_label_offset_y=0       # Vertical offset (+down, -up)
```

### Temperature Offsets

Control CPU and GPU temperature number positions independently:

```ini
# Both temperatures the same
display_temp_offset_x=15       # Both CPU+GPU 15px right
display_temp_offset_y=-10      # Both CPU+GPU 10px up

# CPU and GPU different (comma-separated)
display_temp_offset_x=20,-5    # CPU: 20px right, GPU: 5px left
display_temp_offset_y=10,-10   # CPU: 10px down, GPU: 10px up
```

**Behavior:**
- `-9999` or commented out = auto positioning (default)
- Single value = applies to both CPU and GPU
- Comma-separated = individual CPU,GPU values
- Positive = right/down, Negative = left/up
- Degree symbol moves with temperature (bound together)

### Degree Symbol Spacing

Adjust distance between temperature number and degree symbol:

```ini
display_degree_spacing=10      # Closer: 10px
display_degree_spacing=20      # Wider: 20px
display_degree_spacing=16      # Default: 16px
```

### Label Offsets

Adjust CPU/GPU label positions:

```ini
display_label_offset_x=5       # 5px right
display_label_offset_y=-3      # 3px up
display_label_offset_x=-10     # 10px left
```

### Use Cases

**Dual Mode Fine-Tuning:**
```ini
# Shift CPU temp slightly up, GPU slightly down
display_temp_offset_y=-5,5
```

**Circle Mode Adjustments:**
```ini
# CPU display uses first value, GPU display uses second
display_temp_offset_x=10,15    # CPU: +10px, GPU: +15px
```

**Tighter Layout:**
```ini
display_degree_spacing=8       # Closer degree symbol
display_label_offset_x=-5      # Labels closer to edge
```

---

## 🔧 Troubleshooting

### Common Issues

1. **Display not updating**: Check refresh intervals and restart service
2. **Wrong colors**: Verify RGB values are 0-255
3. **Text too small/large**: Adjust font_size_temp and font_size_labels (or remove them for auto-scaling)
4. **Bars too wide/narrow**: Adjust bar_width percentage (1-100%, default: 98%)
5. **Content clipped on circular displays**: Check inscribe_factor and content_scale_factor settings

### Testing Changes

Always restart the service after configuration changes:
```bash
sudo systemctl restart coolerdash
sudo systemctl status coolerdash
```

### Backup Configuration

Before making changes, backup your working configuration:
```bash
sudo cp /etc/coolerdash/config.ini /etc/coolerdash/config.ini.backup
```

## 🧪 Run scaling unit test (developer)

There is a small test harness that validates safe_area and safe_bar calculations used by the renderers.
Build and run the test with:
```bash
gcc -std=c99 -Iinclude -I./src -o build/test_scaling tests/test_scaling.c -lm
./build/test_scaling
```
This will print the computed safe_area and safe_bar width for representative cases: auto (0.0), explicit 0.70710678 and custom 0.85.
