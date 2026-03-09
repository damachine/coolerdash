# CoolerDash Plugin Integration for CoolerControl

## Overview

CoolerDash runs as a native CoolerControl plugin managed by `cc-plugin-coolerdash.service`. No separate systemd service or startup delay is needed. CoolerControl handles the plugin lifecycle.

## CC4 Integration

### Authentication

CC4 uses Bearer Token authentication. Generate a token in CoolerControl UI under **Access Protection** and set it in `config.json`:

```json
"daemon": {
    "access_token": "cc_<your-uuid-here>"
}
```

CC3 fallback: leave `access_token` empty, set `password` instead.

### Shutdown Image

CoolerDash registers `shutdown.png` with CC4 once at startup:

```
PUT /devices/{uid}/settings/lcd/lcd/shutdown-image
```

CC4 stores the image server-side and displays it when CoolerControl stops. No helper daemon or `ExecStop` workaround needed.

## Plugin UI

### Theme-Adaptive UI

The plugin UI automatically adapts to the user's CoolerControl theme:

- **Dark/Light Theme Support** - Seamlessly integrates with user preferences
- **Theme Color Variables** - Uses CoolerControl's native color system
- **Consistent Design** - Matches CoolerControl's visual language

#### CSS Variables Used

```css
--colors-bg-one       /* Primary background */
--colors-bg-two       /* Secondary background */
--colors-border-one   /* Border color */
--colors-text         /* Text color */
--colors-accent       /* Accent/highlight color */
```

### Tailwind CSS

```html
<div class="flex flex-col p-4 gap-2">
  <button class="bg-accent/80 hover:!bg-accent">Save</button>
</div>
```

### PrimeIcons

```html
<i class="pi pi-save"></i>     <!-- Save icon -->
<i class="pi pi-refresh"></i>  <!-- Refresh icon -->
```

### Manifest

```toml
version = "{{VERSION}}"
url = "https://github.com/damachine/coolerdash"
```

These fields are displayed on the CoolerControl plugin page, helping users:
- Check if their plugin is up-to-date
- Report issues or request features
- Access documentation

## Plugin UI Structure

### Before (2.x and earlier)

- ❌ Hardcoded colors
- ❌ Separate helperd service for shutdown
- ❌ Custom CSS only

### Current (3.x)

- ✅ Theme-adaptive colors via CSS variables
- ✅ Tailwind CSS
- ✅ Shutdown handled natively by CC4
- ✅ PrimeIcons

## Configuration UI

The plugin settings page includes:

### 🌐 Daemon Settings
- CoolerControl API Address
- API Password

### 🖥️ Display Mode
- Mode selection (Dual/Circle)
- Circle switch interval

### 📊 Display Settings
- Refresh interval
- Brightness slider (with live preview)
- Orientation selector

### 🔧 Advanced Settings
- Display dimensions (width/height)

## Technical Implementation

### Theme Colors Example

```css
.section-card {
    background: rgb(var(--colors-bg-one));
    border: 1px solid rgb(var(--colors-border-one));
    border-radius: 0.5rem;
    padding: 1.5rem;
}

.input-field:focus {
    border-color: rgb(var(--colors-accent));
}
```

### Responsive Design

```html
<body class="flex flex-col min-h-full text-text-color bg-bg-two p-4">
  <div class="container mx-auto max-w-4xl">
    <!-- Content adapts to screen size -->
  </div>
</body>
```

## Benefits

### For Users

1. **Visual Consistency** - Plugin UI matches CoolerControl's appearance
2. **Theme Support** - Automatically adapts to dark/light themes
3. **Better UX** - Standard UI patterns and behaviors
4. **Version Info** - Easy to check if plugin is up-to-date

### For Developers

1. **Rapid Development** - Tailwind CSS speeds up UI work
2. **Maintainability** - Theme variables reduce hardcoded values
3. **Documentation** - Clear examples and patterns
4. **Best Practices** - Following CoolerControl's UI guidelines

## Migration Guide

If you're updating from an older version:

1. **No User Action Required** - UI changes are automatic
2. **Config Preserved** - Existing settings remain intact
3. **Theme Applies Immediately** - UI adapts to current theme

## Resources

- [Plugin UI Theming Guide](./plugin-ui-theming.md)
- [CoolerControl Custom Device Plugin](https://gitlab.com/coolercontrol/cc-plugin-custom-device)
- [index.html](../etc/coolercontrol/plugins/coolerdash/index.html) - Full implementation

## Acknowledgments

Special thanks to @codifryed (CoolerControl developer) for the custom-device plugin reference and CC4 API design.

## Related Documentation

- [Configuration Guide](./config-guide.md)
- [Developer Guide](./developer-guide.md)
- [Display Modes](./display-modes.md)
