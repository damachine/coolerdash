# Plugin UI Theming

The plugin UI adapts to CoolerControl's theme (dark/light) via CSS variables.

## CSS Variables

```css
rgb(var(--colors-bg-one))       /* Primary background */
rgb(var(--colors-bg-two))       /* Secondary background */
rgb(var(--colors-border-one))   /* Border */
rgb(var(--colors-text))         /* Text */
rgb(var(--colors-accent))       /* Accent / highlights */
rgb(var(--colors-red))          /* Errors */
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
- `--colors-red` — errors, destructive actions
- `--colors-bg-one` — content containers
- `--colors-bg-two` — page background

## Reference

- Implementation: [index.html](../etc/coolercontrol/plugins/coolerdash/ui/index.html)
- [CoolerControl Plugin Docs](https://gitlab.com/coolercontrol/cc-plugins)
- [Tailwind CSS Docs](https://tailwindcss.com/docs)
- [PrimeIcons](https://primevue.org/icons/)
