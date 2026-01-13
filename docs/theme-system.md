# CoolerDash UI Theme System

## Automatic Dark/Light Mode Support

### How It Works

CoolerDash's UI **automatically adapts** to CoolerControl's theme system. There is no need for manual theme detection or switching!

### Theme Variables

CoolerControl provides CSS custom properties that automatically change based on the user's selected theme:

```css
--colors-bg-one       /* Primary background (adjusts for dark/light) */
--colors-bg-two       /* Secondary background */
--colors-border-one   /* Border color */
--colors-text         /* Text color (white in dark, black in light) */
--colors-accent       /* Accent color (theme-specific) */
--colors-red          /* Error/warning color */
```

### Usage in CSS

```css
.my-element {
    background: rgb(var(--colors-bg-one));
    color: rgb(var(--colors-text));
    border: 1px solid rgb(var(--colors-border-one));
}
```

### What Happens When Theme Changes

1. User changes theme in CoolerControl settings (Dark → Light or vice versa)
2. CoolerControl updates all CSS variables automatically
3. CoolerDash UI instantly reflects the new theme
4. **No JavaScript required!** Pure CSS solution

### Dark Mode Example

When user selects dark theme:
- `--colors-bg-one` → Dark gray (#1a1a1a)
- `--colors-text` → Light gray/white (#eaeaea)
- `--colors-border-one` → Medium gray

### Light Mode Example

When user selects light theme:
- `--colors-bg-one` → Light gray/white
- `--colors-text` → Dark gray/black
- `--colors-border-one` → Light border color

## Why This Approach?

### ✅ Advantages

1. **Zero Configuration** - Works out of the box
2. **Instant Updates** - No page reload needed
3. **Consistent** - Matches CoolerControl's appearance
4. **Maintainable** - No theme logic to maintain
5. **Accessible** - Follows system preferences via CoolerControl

### ❌ Don't Do This

```css
/* BAD - Hardcoded colors */
body {
    background: #1a1a1a;  /* Won't adapt to light theme! */
    color: #ffffff;
}

/* BAD - Manual @media queries */
@media (prefers-color-scheme: dark) {
    body { background: #1a1a1a; }
}
```

### ✅ Do This Instead

```css
/* GOOD - Uses theme variables */
body {
    background: rgb(var(--colors-bg-two));
    color: rgb(var(--colors-text));
}
```

## Testing

To test theme adaptation:

1. Open CoolerControl
2. Go to Settings → Appearance
3. Switch between themes
4. Open CoolerDash plugin settings
5. UI should instantly match the selected theme

## Implementation Details

### Current Implementation

The `index.html` uses:

```html
<body class="flex flex-col min-h-full text-text-color bg-bg-two p-4">
```

Where:
- `text-text-color` → Tailwind class that uses `--colors-text`
- `bg-bg-two` → Tailwind class that uses `--colors-bg-two`

### CSS Classes

```css
.section-card {
    background: rgb(var(--colors-bg-one));
    border: 1px solid rgb(var(--colors-border-one));
}

.input-field:focus {
    border-color: rgb(var(--colors-accent));
}
```

## System Preference Detection

CoolerControl itself handles system preference detection:

1. User's OS reports dark/light preference
2. CoolerControl reads this via system APIs
3. CoolerControl applies appropriate theme
4. All plugins (including CoolerDash) inherit the theme

### Supported Platforms

- ✅ Linux (GTK/Qt theme detection)
- ✅ Windows (System theme API)
- ✅ User can override with manual theme selection

## Future Enhancements

No theme-related changes needed! The system is complete:

- ✅ Automatic dark/light detection via CoolerControl
- ✅ Instant theme switching without reload
- ✅ Consistent with host application
- ✅ Accessible and maintainable

## Related Documentation

- [Plugin UI Theming](./plugin-ui-theming.md) - Full theming guide
- [Plugin Integration](./plugin-integration.md) - Integration overview

## Summary

**You don't need to implement dark/light mode detection!** CoolerControl handles everything automatically through CSS variables. Just use the theme variables in your CSS and the UI will adapt perfectly to any theme the user selects.
