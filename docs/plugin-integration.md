# Plugin Integration

CoolerDash runs as an unprivileged integration plugin. CoolerControl manages its
service lifecycle automatically; no separate service setup is needed.

## Authentication

Create a token with Write Access under CoolerControl's Access Protection, then
save it under Plugins > CoolerDash > Connection. CoolerDash persists the token
with restricted permissions in `credentials.json`; `config.json` only contains
the `***` sentinel after the token has been saved.

## Shutdown Image

Registered once at startup through CoolerControl's shutdown-image API:

```
PUT /devices/{uid}/settings/lcd/{channel}/shutdown-image
```

CoolerControl stores the image and displays it when it stops. Configure a custom
shutdown image via `paths.image_shutdown`, or keep the default file at
`/var/lib/coolercontrol/plugins/coolerdash/shutdown.png`.

CoolerDash detects PNG, GIF, JPEG, BMP, and TIFF files by content and sends the
matching MIME type. Animated GIF support depends on the LCD device and driver.

![Shutdown image preview](../images/shutdown.png)

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
privileged = false
url = "https://github.com/damachine/coolerdash"
```

Displayed on the CoolerControl plugin page.

## Settings UI Sections

- Daemon: API address, access token
- Display: Mode, refresh interval, brightness, orientation
- Advanced: Display dimensions

## Background Preview

The Display tab loads a static background thumbnail through
`pluginFetch('/background-preview?path=' + encodeURIComponent(path))` on the
existing loopback plugin data server. The path may be an unsaved form value.
The endpoint accepts absolute paths to readable regular PNG, GIF, JPEG, BMP or
TIFF files, detects the format by content and returns JSON containing a PNG data
URI. Input is limited to 16 MiB and 32 megapixels; output fits within 480 × 480
pixels. GIFs produce a still frame, independently of the device animation state.

The UI debounces path edits, ignores stale responses and applies background fit,
zoom, color and overlay settings locally. Clearing the path restores the solid
background color. The plugin executable must be running for thumbnail requests.

## Related

- [Configuration Guide](config-guide.md)
- [Plugin UI Theming](plugin-ui-theming.md)
- [CoolerControl Custom Device Plugin](https://gitlab.com/coolercontrol/cc-plugin-custom-device)
