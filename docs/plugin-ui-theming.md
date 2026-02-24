# CoolerDash Plugin UI Theming Guide

## Overview

CoolerDash's plugin UI is designed to seamlessly integrate with CoolerControl's theme system. This document explains how the UI adapts to user themes and the available customization options.

## Theme Integration

### Available CSS Variables

CoolerControl provides the following CSS variables that automatically adapt to the user's selected theme:

#### Color Variables

```css
rgb(var(--colors-bg-one))       /* Primary background color */
rgb(var(--colors-bg-two))       /* Secondary background color */
rgb(var(--colors-border-one))   /* Border color */
rgb(var(--colors-text))         /* Primary text color */
rgb(var(--colors-accent))       /* Accent color (highlights, buttons) */
rgb(var(--colors-red))          /* Red color (errors, warnings) */
```

### Usage Examples

#### Basic Container

```css
.section-card {
    background: rgb(var(--colors-bg-one));
    border: 1px solid rgb(var(--colors-border-one));
    border-radius: 0.5rem;
    padding: 1.5rem;
}
```

#### Input Fields

```css
.input-field {
    background: rgb(var(--colors-bg-one));
    border: 1px solid rgb(var(--colors-border-one));
    color: rgb(var(--colors-text));
}

.input-field:focus {
    border-color: rgb(var(--colors-accent));
}
```

#### Buttons

```css
.btn-primary {
    background: rgb(var(--colors-accent));
    color: white;
    opacity: 0.8;
}

.btn-primary:hover {
    opacity: 1;
}
```

## Tailwind CSS Support

CoolerControl provides Tailwind CSS classes for rapid UI development:

### Layout Classes

- `flex`, `flex-col`, `flex-row` - Flexbox layouts
- `grid`, `grid-cols-2` - Grid layouts
- `p-2`, `p-4`, `px-2`, `py-4` - Padding
- `m-2`, `m-4`, `mx-auto` - Margins
- `gap-2`, `gap-4` - Gaps in flex/grid

### Typography

- `text-sm`, `text-lg`, `text-2xl` - Font sizes
- `font-bold`, `font-semibold` - Font weights
- `opacity-70` - Opacity adjustments

### Colors

- `bg-bg-one`, `bg-bg-two` - Background colors
- `text-text-color` - Text color
- `border-border-one` - Border color

### Responsive Design

- `min-h-full` - Minimum height
- `max-w-4xl` - Maximum width
- `rounded-lg` - Border radius

## PrimeIcons Support

CoolerControl includes PrimeIcons for consistent iconography:

```html
<i class="pi pi-save"></i>      <!-- Save icon -->
<i class="pi pi-refresh"></i>   <!-- Refresh icon -->
<i class="pi pi-plus"></i>      <!-- Add icon -->
<i class="pi pi-trash"></i>     <!-- Delete icon -->
```

## Best Practices

### 1. Always Use Theme Variables

❌ **Don't:**
```css
background: #1a1a2e;
color: #eaeaea;
border: 1px solid #2d4263;
```

✅ **Do:**
```css
background: rgb(var(--colors-bg-one));
color: rgb(var(--colors-text));
border: 1px solid rgb(var(--colors-border-one));
```

### 2. Provide Visual Feedback

```css
.input-field:focus {
    border-color: rgb(var(--colors-accent));
}

.btn:hover {
    opacity: 1;
}
```

### 3. Use Semantic Color Meanings

- `--colors-accent` - Primary actions, highlights
- `--colors-red` - Errors, destructive actions
- `--colors-bg-one` - Content containers
- `--colors-bg-two` - Page background

### 4. Maintain Consistency

Use the same spacing, border-radius, and transition patterns throughout:

```css
border-radius: 0.375rem;  /* For inputs/buttons */
border-radius: 0.5rem;    /* For containers */
transition: opacity 0.2s; /* For hover effects */
```

## Dark/Light Theme Support

The UI automatically adapts to the user's theme (dark/light) through the CSS variables. No additional JavaScript or CSS is needed.

## Testing

To test your UI with different themes:

1. Open CoolerControl settings
2. Change the theme under Appearance
3. Navigate to CoolerDash plugin settings
4. Verify all UI elements adapt correctly

## Example Implementation

See [index.html](../etc/coolercontrol/plugins/coolerdash/index.html) for a complete implementation example.

## Resources

- [CoolerControl Custom Device Plugin](https://gitlab.com/coolercontrol/cc-plugin-custom-device) - Reference implementation
- [Tailwind CSS Documentation](https://tailwindcss.com/docs)
- [PrimeIcons](https://primevue.org/icons/) - Icon reference

## Manifest Configuration

Add version and URL to your `manifest.toml`:

```toml
version = "2.2.x"
url = "https://github.com/damachine/coolerdash"
```

These will be displayed on the plugin page in CoolerControl's UI.

## Changelog

- **2.2.x** - Added theme color support and Tailwind CSS integration
- **2.0.4** - Initial plugin UI implementation
