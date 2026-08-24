# Plugin UI Theming

The plugin UI adapts to CoolerControl's theme (dark/light) via CSS variables.

## CSS Variables

CoolerControl sends the active theme's variables to the plugin iframe, each carrying an
`r g b` triplet:

```css
rgb(var(--colors-bg-one))                /* Primary background */
rgb(var(--colors-bg-two))                /* Secondary background */
rgb(var(--colors-border-one))            /* Border */
rgb(var(--colors-text-color))            /* Text */
rgb(var(--colors-text-color-secondary))  /* Muted text */
rgb(var(--colors-accent))                /* Accent / highlights */
rgb(var(--colors-surface-hover) / 0.05)  /* Hover tint, see below */
rgb(var(--colors-success))               /* Success */
rgb(var(--colors-warning))               /* Warning */
rgb(var(--colors-error))                 /* Error / destructive */
rgb(var(--colors-info))                  /* Informational */
rgb(var(--colors-accent-gradient-to))    /* Accent gradient end */
```

`--colors-surface-hover` is not a color. It is a tint to composite over a surface: white
on dark themes, black on light ones. Use it with a low alpha, as CoolerControl does at 5%.
Filling with it at full strength gives a solid white or black hover.

Give every one a fallback. It keeps the UI rendering standalone, where no parent
stylesheet is injected, and on CoolerControl versions that publish a smaller set:

```css
:root {
    --bg-one: var(--colors-bg-one, 27 30 35);
}
```

## Usage

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

.btn-primary {
    background: rgb(var(--colors-accent));
    opacity: 0.8;
}
.btn-primary:hover {
    opacity: 1;
}
```

Always use theme variables instead of hardcoded colors.

## Tailwind CSS

Available utility classes:

| Class | Role |
|-------|------|
| `bg-bg-one` | Base / deepest background layer |
| `bg-bg-two` | Elevated surface — panels, cards, dialogs |
| `bg-surface-hover` | Subtle overlay for hover states |
| `bg-accent` / `text-accent` | Brand / interactive accent color |
| `text-text-color` | Primary text |
| `text-text-color-secondary` | Muted / secondary text |
| `border-border-one` | Standard border color |
| `bg-success` / `text-success` | Success (green) |
| `bg-error` / `text-error` | Error / danger (red) |
| `bg-warning` / `text-warning` | Warning (yellow) |

Layout: `flex`, `flex-col`, `grid`, `p-2`, `p-4`, `gap-2`, `text-sm`, `font-bold`, `rounded-lg`.

## PrimeIcons

```html
<i class="pi pi-save"></i>
<i class="pi pi-refresh"></i>
<i class="pi pi-plus"></i>
<i class="pi pi-trash"></i>
```

## Rendering Context

Detect how the plugin UI is displayed:

```js
const { mode } = await getContext(); // 'modal' | 'full_page'
```

- `modal` — opened as a dialog (e.g. settings shortcut)
- `full_page` — dedicated plugin page in the sidebar

## Semantic Colors

- `--colors-accent` — primary actions, highlights
- `--colors-bg-one` — content containers
- `--colors-bg-two` — page background
- `--colors-text-color` and `--colors-text-color-secondary` for primary and muted text
- `--colors-success`, `--colors-warning`, `--colors-error`, `--colors-info` for status

## Reference

- Implementation: [index.html](../etc/coolercontrol/plugins/coolerdash/ui/index.html)
- [CoolerControl Plugin Docs](https://gitlab.com/coolercontrol/cc-plugins)
- [Tailwind CSS Docs](https://tailwindcss.com/docs)
- [PrimeIcons](https://primevue.org/icons/)
