# Plugin Integration

CoolerDash runs as a CoolerControl plugin managed by `cc-plugin-coolerdash.service`. No separate systemd service needed.

## Authentication

Bearer token auth via CoolerControl Access Protection:

```json
"daemon": {
    "access_token": "cc_<your-uuid-here>"
}
```

Generate token in CoolerControl UI > Access Protection.

## Shutdown Image

Registered once at startup via CC4 API:

```
PUT /devices/{uid}/settings/lcd/lcd/shutdown-image
```

CC4 stores the image and displays it when CoolerControl stops. No helper daemon needed.

## Plugin UI

Theme-adaptive UI using CoolerControl CSS variables + Tailwind CSS + PrimeIcons.

### CSS Variables

```css
--colors-bg-one       /* Primary background */
--colors-bg-two       /* Secondary background */
--colors-border-one   /* Border color */
--colors-text         /* Text color */
--colors-accent       /* Accent color */
```

### Manifest

```toml
version = "{{VERSION}}"
url = "https://github.com/damachine/coolerdash"
```

Displayed on the CoolerControl plugin page.

## Settings UI Sections

- Daemon: API address, access token
- Display: Mode, refresh interval, brightness, orientation
- Advanced: Display dimensions

## Related

- [Configuration Guide](config-guide.md)
- [Plugin UI Theming](plugin-ui-theming.md)
- [CoolerControl Custom Device Plugin](https://gitlab.com/coolercontrol/cc-plugin-custom-device)
