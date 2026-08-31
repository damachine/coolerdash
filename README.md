
<p align="left">
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-green.svg"></a>
  <a href="https://en.wikipedia.org/wiki/C99"><img src="https://img.shields.io/badge/C-99-blue.svg"></a>
  <a href="https://kernel.org/"><img src="https://img.shields.io/badge/Platform-Linux-green.svg"></a>
  <a href="https://github.com/damachine/coolerdash/actions/workflows/github-code-scanning/codeql"><img src="https://github.com/damachine/coolerdash/actions/workflows/github-code-scanning/codeql/badge.svg"></a>
</p>

# CoolerDash

### Linux LCD telemetry for supported AIO liquid coolers through CoolerControl, tested on multiple NZXT Kraken models.

**Display real-time CPU, GPU, and liquid temperatures with customizable layouts directly on your cooler's LCD.**

## Features
- **Plug-In that extends the LCD functionality of [CoolerControl](https://gitlab.com/coolercontrol/coolercontrol)**
- **Support for additional sensor values (CPU/GPU/Liquid)**
- **Integrated UI for customizing all settings**  
- **Display modes**

### Split Mode

**Default:** Shows CPU and GPU in two compact columns.

<p align="left">
  <img width="120" height="120" alt="split_3" src="https://github.com/user-attachments/assets/2c63e61f-5640-4ba3-a8db-e0e508fabd9a" />&nbsp;&nbsp;&nbsp;
  <img width="120" height="120" alt="split_2" src="https://github.com/user-attachments/assets/b174fe10-7bfb-4eec-adc6-134a14b486bd" />&nbsp;&nbsp;&nbsp;
  <img width="120" height="120" alt="split_1" src="https://github.com/user-attachments/assets/2c0b2faa-ade5-4b54-8a7d-5fd39f9c332d" />
</p>

### Dual Mode

Shows CPU and GPU temperatures with bars.

<p align="left">
  <img width="120" height="120" alt="dual_1" src="https://github.com/user-attachments/assets/5bc82ff9-909f-441f-9832-d1dadf4da7de" />&nbsp;&nbsp;&nbsp;
  <img width="120" height="120" alt="dual_2" src="https://github.com/user-attachments/assets/d33beba5-7db5-41ec-b6bd-9a1208f32d3c" />&nbsp;&nbsp;&nbsp;
  <img width="120" height="120" alt="dual_3" src="https://github.com/user-attachments/assets/d3363d4f-94b7-4c8d-b25a-cb8fdcf5c3a5" />
</p>

### Circle Mode

**Beta:** Alternates between configured sensor slots.

<p align="left">
  <img width="120" height="120" alt="circle_1" src="https://github.com/user-attachments/assets/02c355a5-4cf9-4503-a676-5a0046289e07" />&nbsp;&nbsp;&nbsp;
  <img width="120" height="120" alt="circle_2" src="https://github.com/user-attachments/assets/2547a5be-c83e-4301-99f5-f863ccdfb5ee" />&nbsp;&nbsp;&nbsp;
  <img width="120" height="120" alt="circle_3" src="https://github.com/user-attachments/assets/caaf8f4d-773c-434f-a646-7e1cd84a8add" />
</p>

> **Officially listed by CoolerControl as a [2nd Party (Trusted) Plugin](https://docs.coolercontrol.org/automation/plugins.html#_2nd-party-trusted-plugins).**  
> Special thanks to [@codifryed](https://github.com/codifryed), the founder of CoolerControl.

Join the CoolerDash channel on the official CoolerControl Discord:

<a href="https://discord.com/channels/908873022105079848/1461781766791499981"><img src="https://img.shields.io/badge/Discord-Join%20CoolerDash%20Discussion-blue?logo=discord"></a>

## System Requirements

- **OS**: Linux (systemd or openrc)
- **CoolerControl**: Version >=3.1.0 REQUIRED - must be installed and running [Installation Guide](https://gitlab.com/coolercontrol/coolercontrol/-/blob/main/README.md)
- **CPU**: x86-64
- **LCD**: AIO liquid cooler LCD displays **(NZXT, etc.)**

> See the [Supported Devices](https://github.com/damachine/coolerdash/blob/master/docs/devices.md) for confirmed working hardware. In principle, all devices supported by CoolerControl/[liquidctl](https://github.com/liquidctl/liquidctl?tab=readme-ov-file#supported-devices) should work with CoolerDash. You can [submit a device confirmation](https://github.com/damachine/coolerdash/issues/new?template=device-confirmation.yml) to help expand the list.

## Installation

### Arch-based distributions

[![AUR](https://img.shields.io/aur/version/coolerdash-git?color=blue&label=AUR)](https://aur.archlinux.org/packages/coolerdash-git)

```bash
# Install with an AUR helper
yay -S coolerdash-git
#OR any other AUR helper
```

**Pre-built packages:**

[![Debian/Ubuntu](https://img.shields.io/badge/Debian%2FUbuntu-Download-orange?logo=debian)](https://github.com/damachine/coolerdash/releases/latest)
[![Fedora](https://img.shields.io/badge/Fedora-Download-blue?logo=fedora)](https://github.com/damachine/coolerdash/releases/latest)
[![CentOS/RHEL](https://img.shields.io/badge/CentOS%2FRHEL-Download-green?logo=centos)](https://github.com/damachine/coolerdash/releases/latest)
[![openSUSE](https://img.shields.io/badge/openSUSE-Download-brightgreen?logo=opensuse)](https://github.com/damachine/coolerdash/releases/latest)

### Manual installation

```bash
# STEP 1: Clone repository
git clone https://github.com/damachine/coolerdash.git
cd coolerdash

# STEP 2: Install dependencies, build, and install
sudo make install
```

> For manual installations, make sure all required dependencies are installed correctly. Manual installations need to be updated manually.

## Configuration

**Required: Access Token**

1. In CoolerControl, open **Access Protection → Access Tokens**.
2. Create a token with **Write Access** enabled (for example, named `coolerdash`) and copy it.
3. Open **Plugins → CoolerDash → Connection**, paste it into **Access Token**, and save.

> CoolerDash only reads sensor data and does not change sensor settings. Write Access is required solely to update the LCD through CoolerControl. See the [CoolerControl access token documentation](https://docs.coolercontrol.org/daemon/access-protection.html#access-tokens).

**Optional:** Customize other settings in the CoolerDash UI.

![CoolerDash configuration](images/configuration.png)

## Hardware Reports

<details>
  <summary>Expand</summary>

## Usage

If an AIO LCD is unsupported or only partially working, create a sanitized
hardware report:

~~~bash
coolerdash --hardware-report
~~~

CoolerDash writes a JSON report and a Markdown summary to your home directory.
Nothing is uploaded automatically. Review both files before attaching the JSON
and pasting the Markdown summary into a
[Device Confirmation Issue](https://github.com/damachine/coolerdash/issues/new?template=device-confirmation.yml).

An optional LCD test is available when no other CoolerDash instance is running:

~~~bash
coolerdash --hardware-report --test-lcd
~~~

The test requires an explicit **TEST** confirmation, temporarily displays a test
image with geometry guides, records the visible panel shape and centering, and
then restores the previous LCD settings. For a new or unknown LCD model, use
this form so the report contains enough evidence for a display profile.

Advanced overrides:

```bash
coolerdash --hardware-report --output-dir /path/to/reports
coolerdash --hardware-report --test-lcd --device DEVICE_UID
coolerdash --hardware-report /custom/config.json
```

The report omits access tokens, serial numbers, raw device UIDs, host/user names,
IP addresses, and personal paths. It includes sanitized CoolerControl and
liquidctl metadata plus matching USB device, interface, and endpoint
descriptors. These descriptors identify available HID/bulk paths but do not
capture USB traffic or reveal an unknown pixel protocol.
</details>

## Advanced Usage

<details>
  <summary>Expand</summary>

### Build Commands

```bash
make            # Standard C99 build
make clean      # Clean up
make install-deps       # Install dependencies only
sudo make install       # Install dependencies, build, and install
sudo make uninstall     # Remove the program and preserve user data
make debug      # Debug build with AddressSanitizer
make help       # Show all options
```

### Debugging

```bash
# 1. Check CoolerControl status
systemctl status coolercontrold
# or on OpenRC:
rc-service coolercontrold status
curl http://localhost:11987/devices

# 2. Test CoolerDash manually (with clean output)
/usr/libexec/coolerdash/coolerdash

# 3. Test CoolerDash with detailed verbose logging
/usr/libexec/coolerdash/coolerdash --verbose
# or short form:
/usr/libexec/coolerdash/coolerdash -v

# 4. Debug build and installation (recommended)
# Option A — Build as your user, then install the ASan binary:
make debug
sudo make install

# Option B — Build as your user and install the debug binary manually (recommended):
make clean && make debug
sudo install -Dm755 bin/coolerdash /usr/libexec/coolerdash/coolerdash

# Notes:
#  • Run `make clean && make` to switch from a debug build back to a normal build.
#  • If you previously built as root and own files are root-owned, fix ownership before rebuilding:
#    sudo chown -R $USER:$USER build bin

# 5. Check plugin logs (STATUS messages always visible)
journalctl -xeu coolercontrold.service -f
# On OpenRC, inspect your configured system logger for CoolerControl/coolerdash output.

# 6. View recent logs with context
journalctl -u coolercontrold.service -n 50
```
</details>

## Troubleshooting

<details>
  <summary>Expand</summary>

### Installation Issues
If you see errors like "conflicting files" or "manual installation detected" during Arch/AUR `makepkg -si`, CoolerDash was previously installed manually via `make install`.

**Solution:**
```bash
sudo make uninstall
```

### Check CoolerControl devices is detected

```bash
liquidctl --version
# Expected: liquidctl v1.15.0 (or newer)
```

```bash
liquidctl --list
# Expected: Device #0: NZXT Kraken 2023
```

  ```bash
  curl http://localhost:11987/devices | jq
  ```

  ```json
  {
        "name": "NZXT Kraken 2023",
        "type": "Liquidctl",
        "type_index": 1,
        "uid": "8d4becb03bca2a8e8d4213ac376a1094f39d2786f688549ad3b6a591c3affdf9",
        "lc_info": {
          "driver_type": "KrakenZ3",
          "firmware_version": "2.0.0",
          "unknown_asetek": false
        }
  }
  ```
</details>

## Documentation

<details>
  <summary>Expand</summary>

- **[Configuration Guide](https://github.com/damachine/coolerdash/blob/master/docs/config-guide.md)** - All configuration options
- **[Supported Devices](https://github.com/damachine/coolerdash/blob/master/docs/devices.md)** - Confirmed working hardware
- **[Display Modes Guide](https://github.com/damachine/coolerdash/blob/master/docs/display-modes.md)** - Dual, Split, and Circle mode reference
- **[Developer Guide](https://github.com/damachine/coolerdash/blob/master/docs/developer-guide.md)** - Architecture and API integration
- **[Display Detection](https://github.com/damachine/coolerdash/blob/master/docs/display-detection.md)** - Display shape detection
- **[CoolerControl API Guide](https://github.com/damachine/coolerdash/blob/master/docs/coolercontrol-api.md)** - API module documentation
</details>

## Community & Support

<a href="https://github.com/damachine/coolerdash/discussions"><img src="https://img.shields.io/github/discussions/damachine/coolerdash?style=flat-square&logo=github&label=Discussions"></a> <a href="https://github.com/damachine/coolerdash/issues"><img src="https://img.shields.io/github/issues/damachine/coolerdash?style=flat-square&logo=github&label=Issues"></a> <a href="https://discord.com/channels/908873022105079848/1461781766791499981"><img src="https://img.shields.io/badge/Discord-Join%20CoolerDash%20Discussion-blue?logo=discord"></a>

**Support the project:**

<p align="left">
  <a href="https://github.com/damachine/coolerdash/stargazers"><img src="https://img.shields.io/github/stars/damachine/coolerdash?style=flat&logo=github&label=Stars" alt="GitHub stars"></a>
  <a href="https://github.com/sponsors/damachine"><img src="https://img.shields.io/badge/Sponsor-GitHub-blue?logo=github-sponsors" alt="Sponsor"></a>
</p>
