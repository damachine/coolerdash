
<p align="left">
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-green.svg"></a>
  <a href="https://en.wikipedia.org/wiki/C99"><img src="https://img.shields.io/badge/C-99-blue.svg"></a>
  <a href="https://kernel.org/"><img src="https://img.shields.io/badge/Platform-Linux-green.svg"></a>
  <a href="https://app.codacy.com/gh/damachine/coolerdash/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/056718c967524322acd4f4f76393fd7f"></a>
  <a href="https://github.com/damachine/coolerdash/actions/workflows/github-code-scanning/codeql"><img src="https://github.com/damachine/coolerdash/actions/workflows/github-code-scanning/codeql/badge.svg"></a>
</p>

---

# CoolerDash 🐧

#### This tool allows you to display real-time telemetry data from sensors on an AIO liquid cooler with an integrated LCD display.   
   
I started developing this tool because the LCD display of my NZXT Kraken 2023 under Linux did not meet my expectations.   
   
> Use it successfully myself – maybe it will help you too! ❤️

---
## Features
- **Extends the LCD functionality of [CoolerControl](https://gitlab.com/coolercontrol/coolercontrol) with additional features.**
- **Support for additional sensor values, and a sophisticated, customizable user interface.**
- **Two display modes:**
  - **Dual Mode (default):** Shows CPU and GPU temperatures simultaneously
  - **Circle Mode (beta):** Alternates between CPU and GPU every 5 sec. - optimized for round high-resolution displays (>240x240px)

> ##### Special thanks to [@codifryed](https://github.com/codifryed), the founder of CoolerControl

<a href="https://discord.com/channels/908873022105079848/1395236612677570560"><img src="https://img.shields.io/badge/Discord-Join%20CoolerDash%20Discussion-blue?logo=discord"></a>

<div align="center">
  <img src="images/round.png" alt="CoolerDash LCD Demo round" width="320" height="320"/> 
  <img src="images/quad.png" alt="CoolerDash LCD Demo quad" width="320" height="320"/>
</div>

---

## System Requirements

- **OS**: Linux
- **CoolerControl**: Version >=2.2.2 REQUIRED - must be installed and running [Installation Guide](https://gitlab.com/coolercontrol/coolercontrol/-/blob/main/README.md)
- **CPU**: x86-64-v3 compatible (Intel Haswell+ / AMD Excavator+)
- **LCD**: AIO liquid cooler LCD displays **(NZXT, etc.)**

> See the [Supported Devices](https://github.com/damachine/coolerdash/blob/main/docs/devices.md) for confirmed working hardware. In principle, all devices supported by CoolerControl/[liquidctl](https://github.com/liquidctl/liquidctl?tab=readme-ov-file#supported-devices) should work with CoolerDash. You can [submit a device confirmation](https://github.com/damachine/coolerdash/issues/new?template=device-confirmation.yml) to help expand the list.

---

## Installation

#### Arch Linux

<a href="https://github.com/damachine/coolerdash/actions/workflows/aur.yml"><img src="https://github.com/damachine/coolerdash/actions/workflows/aur.yml/badge.svg"></a>  
[![AUR](https://img.shields.io/aur/version/coolerdash-git?color=blue&label=AUR)](https://aur.archlinux.org/packages/coolerdash-git)

- Using an AUR helper:

```bash
# STEP 1: Install
yay -S coolerdash-git
#OR any other AUR helper

# STEP 2: Enable/Start CoolerDash sytemd service
systemctl daemon-reload
systemctl enable --now coolerdash.service
```

#### All Distributions

<a href="https://github.com/damachine/coolerdash/actions/workflows/install.yml"><img src="https://github.com/damachine/coolerdash/actions/workflows/install.yml/badge.svg"></a>

- Manual installation:

```bash
# STEP 1: Clone repository
git clone https://github.com/damachine/coolerdash.git
cd coolerdash

# STEP 2: Build and install (auto-detects Linux distribution and installs dependencies)
make install

# STEP 3: Enable/Start CoolerDash sytemd service
systemctl daemon-reload
systemctl enable --now coolerdash.service
```

> For manual installations, make sure all required dependencies are installed correctly. Manual installations need to be updated manually.

---

## Configuration

**CoolerControl Setup:**  
In CoolerControl GUI → **Device and Sensor** → Select CPU and GPU sensors → Set LCD display to **Image/gif**

**Start Service:**
```bash
systemctl enable --now coolerdash.service
```

**Configuration:**  
Edit `/etc/coolerdash/config.ini` then restart: `systemctl restart coolerdash.service`

**Display Modes:**
- **Dual (default):** CPU + GPU simultaneously (all displays)
- **Circle:** Alternates CPU/GPU every 5s (round displays >240x240px)

Enable Circle Mode: Edit config.ini → `[display]` section → `mode=circle`  
CLI override: `coolerdash --circle` or `coolerdash --dual`

> [!NOTE]
> See **[Configuration Guide](https://github.com/damachine/coolerdash/blob/main/docs/config-guide.md)** for all options.

---

## Advanced Usage

<details>
  <summary>Expand</summary>
   
#### Service Management

```bash
# Service control
systemctl enable --now coolerdash.service  # Enable and Start!
systemctl start coolerdash.service         # Start
systemctl stop coolerdash.service          # Stop
systemctl restart coolerdash.service       # Restart
systemctl status coolerdash.service        # Status + recent logs

# Journal log
journalctl -u coolerdash.service

# Live logs
journalctl -xeu coolerdash.service -f
```

#### Build Commands

```bash
make            # Standard C99 build
make clean      # Clean up
make install    # System installation with dependency auto-detection
make uninstall  # Remove installation (service, binary, files)
make debug      # Debug build with AddressSanitizer
make help       # Show all options
```

#### Manual Usage 

```bash
# Run manually (with minimal status logging)
coolerdash

# Run with detailed verbose logging
coolerdash --verbose
# or short form:
coolerdash -v

# Force specific display mode
coolerdash --dual      # Force dual mode (CPU+GPU simultaneously)
coolerdash --circle    # Force circle mode (alternating CPU/GPU)
```

#### Debugging Steps

```bash
# 1. Check CoolerControl status
systemctl status coolercontrold
curl http://localhost:11987/devices

# 2. Test CoolerDash manually (with clean output)
coolerdash

# 3. Test CoolerDash with detailed verbose logging
coolerdash --verbose
# or short form:
coolerdash -v

# 4. Debug build for detailed information (if needed)
make debug && coolerdash --verbose

# 5. Check service logs (STATUS messages always visible)
journalctl -xeu coolerdash.service -f

# 6. View recent logs with context
journalctl -u coolerdash.service -n 50
```

> The systemd service must be stopped before running manually to avoid conflicts:

```bash
systemctl stop coolerdash.service
```
</details>

---
   
## Troubleshooting

<details>
  <summary>Expand</summary>

#### Installation Issues
If you see errors like "conflicting files" or "manual installation detected" during Arch/AUR `makepkg -si`, CoolerDash was previously installed manually via `make install`.

**Solution:**
```bash
sudo systemctl stop coolerdash.service
sudo make uninstall
```

Remove any leftover files:
```bash
sudo rm -rf /opt/coolerdash/ \
            /usr/bin/coolerdash \
            /etc/systemd/system/coolerdash.service
```

#### Check CoolerControl devices

```bash
liquidctl --version
# Expected: liquidctl v1.15.0 (or newer)
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

---

## Documentation

- **[Configuration Guide](https://github.com/damachine/coolerdash/blob/main/docs/config-guide.md)** - All configuration options
- **[Supported Devices](https://github.com/damachine/coolerdash/blob/main/docs/devices.md)** - Confirmed working hardware
- **[Display Modes Guide](https://github.com/damachine/coolerdash/blob/main/docs/display-modes.md)** - Dual and Circle mode reference
- **[Developer Guide](https://github.com/damachine/coolerdash/blob/main/docs/developer-guide.md)** - Architecture and API integration
- **[Display Detection](https://github.com/damachine/coolerdash/blob/main/docs/display-detection.md)** - Display shape detection
- **[CoolerControl API Guide](https://github.com/damachine/coolerdash/blob/main/docs/coolercontrol-api.md)** - API module documentation

---

## Community & Support

**Questions or ideas?** Join our [Discussions](https://github.com/damachine/coolerdash/discussions) or open an [Issue](https://github.com/damachine/coolerdash/issues).

<a href="https://github.com/damachine/coolerdash/discussions"><img src="https://img.shields.io/github/discussions/damachine/coolerdash?style=flat-square&logo=github&label=Discussions"></a> <a href="https://github.com/damachine/coolerdash/issues"><img src="https://img.shields.io/github/issues/damachine/coolerdash?style=flat-square&logo=github&label=Issues"></a>

**Support the project:**  
• ⭐ Star this repo   
• 🐛 Report bugs   
• 🔄 Share with others   
• 📝 Contribute   
• [![Sponsor](https://img.shields.io/badge/Sponsor-GitHub-blue?logo=github-sponsors)](https://github.com/sponsors/damachine)   
   
[![Stargazers over time](https://starchart.cc/damachine/coolerdash.svg?variant=adaptive)](https://starchart.cc/damachine/coolerdash)
