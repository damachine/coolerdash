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
refresh_interval_sec=2
refresh_interval_nsec=500000000
brightness=80
orientation=0
```

### Settings
- **`width`**: Screen width in pixels (240 for NZXT Kraken)
- **`height`**: Screen height in pixels (240 for NZXT Kraken)
- **`refresh_interval_sec`**: Update interval in seconds (1-10 recommended)
- **`refresh_interval_nsec`**: Additional nanoseconds (500000000 = 0.5s)
- **`brightness`**: LCD brightness 0-100% (80% recommended)
- **`orientation`**: Screen rotation: `0`, `90`, `180`, `270` degrees

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
refresh_interval_sec=1
refresh_interval_nsec=0

# Standard updates (2.5 seconds)
refresh_interval_sec=2
refresh_interval_nsec=500000000

# Slow updates (5 seconds) - saves power
refresh_interval_sec=5
refresh_interval_nsec=0
```

---

## 🎨 Visual Layout

Display layout and spacing configuration.

### Example Configuration
```ini
[layout]
box_width=240
box_height=120
box_gap=0
bar_width=230
bar_height=22
bar_gap=10
bar_border_width=1.5
```

### Settings
- **`box_width`**: Main display box width (should match display width)
- **`box_height`**: Main display box height (typically half display height)
- **`box_gap`**: Space between display boxes
- **`bar_width`**: Temperature bar width (fits within box_width)
- **`bar_height`**: Temperature bar thickness
- **`bar_gap`**: Space between temperature bars
- **`bar_border_width`**: Border line thickness

### Layout Examples

#### Compact Layout
```ini
[layout]
box_width=240
box_height=115
bar_width=220
bar_height=18
bar_gap=5
bar_border_width=1.0
```

#### Spacious Layout
```ini
[layout]
box_width=240
box_height=125
bar_width=235
bar_height=26
bar_gap=15
bar_border_width=2.0
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

Text appearance configuration.

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

### Font Examples

#### Large Text Setup
```ini
[font]
font_face=Arial Bold
font_size_temp=120.0    # Larger temperature numbers
font_size_labels=35.0   # Larger labels
```

#### Compact Text Setup
```ini
[font]
font_face=Helvetica
font_size_temp=80.0     # Smaller temperature numbers
font_size_labels=24.0   # Smaller labels
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
refresh_interval_sec=2
refresh_interval_nsec=500000000
brightness=80
orientation=0

[layout]
box_width=240
box_height=120
box_gap=0
bar_width=230
bar_height=22
bar_gap=10
bar_border_width=1.5

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

## 🔧 Troubleshooting

### Common Issues

1. **Display not updating**: Check refresh intervals and restart service
2. **Wrong colors**: Verify RGB values are 0-255
3. **Text too small/large**: Adjust font_size_temp and font_size_labels
4. **Bars don't fit**: Ensure bar_width < box_width

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