# CoolerDash Plugin Integration for CoolerControl

## Overview

CoolerDash is now fully integrated as a CoolerControl plugin with enhanced UI support.

## New Features in 2.2.x

### 1. Theme-Adaptive UI

The plugin UI now automatically adapts to the user's CoolerControl theme:

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

### 2. Tailwind CSS Integration

The UI leverages Tailwind CSS classes for rapid development:

```html
<div class="flex flex-col p-4 gap-2">
  <button class="bg-accent/80 hover:!bg-accent">Save</button>
</div>
```

### 3. PrimeIcons Support

Consistent iconography using CoolerControl's icon library:

```html
<i class="pi pi-save"></i>     <!-- Save icon -->
<i class="pi pi-refresh"></i>  <!-- Refresh icon -->
```

### 4. Manifest Enhancements

The `manifest.toml` now includes:

```toml
version = "2.2.x"                              # Displayed in plugin list
url = "https://github.com/damachine/coolerdash" # Link to project homepage
```

These fields are displayed on the CoolerControl plugin page, helping users:
- Check if their plugin is up-to-date
- Report issues or request features
- Access documentation

## Plugin UI Structure

### Before (2.0.4 and earlier)

- ❌ Hardcoded colors (didn't adapt to themes)
- ❌ Custom CSS only (no Tailwind support)
- ❌ Toast notifications (not standard in CC)

### After (2.2.x)

- ✅ Theme-adaptive colors using CSS variables
- ✅ Tailwind CSS for consistent styling
- ✅ Standard console logging (no custom toasts)
- ✅ PrimeIcons for consistent iconography

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

Special thanks to @codifryed (CoolerControl developer) for:
- Providing the custom-device plugin as reference
- Sharing information about theme colors and Tailwind CSS support
- Implementing the version/url manifest fields

## Related Documentation

- [Configuration Guide](./config-guide.md)
- [Developer Guide](./developer-guide.md)
- [Display Modes](./display-modes.md)
