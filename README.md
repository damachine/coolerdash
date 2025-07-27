# CoolerDash - LCD dashboard for CoolerControl

> **The following settings were tested with an NZXT Kraken 2023.**  
> CoolerDash should work with any LCD device supported by CoolerControl (NZXT, Asus, etc.).  
> **Note:** You may need to adjust `/opt/coolerdash/config.ini` to match your device's display size, orientation, and features.

---

**Why CoolerDash?**

I bought myself an NZXT Kraken 2023, but the manufacturer did not provide suitable software to unlock the full potential of my device. That's why I started this little project. CoolerDash now serves me reliably and to my satisfaction. If you have the same problem, I want to give you the opportunity to use my small program as well.

Special thanks to Guy Boldon, the developer of CoolerControl, for making this possible!

---

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C99](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-Linux-green.svg)](https://kernel.org/)
[![Donate BTC](https://img.shields.io/badge/Donate-Bitcoin-f7931a.svg)](bitcoin:13WjpWQMGG5sg3vTJJnCX3cXzwf2vZddKo)
[![Donate DOGE](https://img.shields.io/badge/Donate-Dogecoin-c2a633.svg)](https://dogechain.info/address/DRSY4cA8eCALn819MjWLbwaePFNti9oS3y)

## 📖 Description

**Take full control of your liquid cooling system with integrated LCD display to monitor real-time sensor data in style.**

This system daemon empowers you to harness the potential of your LCD-equipped liquid coolers. Display comprehensive system monitoring data including CPU and GPU temperatures directly on your LCD screen through seamless CoolerControl API integration.

Transform your cooling system into an intelligent monitoring hub that keeps you informed about your system's vital signs at a glance.

---

### 📸 Screenshot – Example LCD Output

<div align="center">
  <img src="images/animation.gif" alt="CoolerDash LCD Animation" width="240" height="240"/>
  <img src="images/gpt.png" alt="AI-generated LCD Demo" width="240" height="240"/>
</div>

*Left: Live temperature monitoring image on NZXT Kraken 2023 LCD display  
Right: AI-generated image to demonstrate LCD output*

**👨‍💻 Author:** DAMACHINE ([christkue79@gmail.com](mailto:christkue79@gmail.com))  
**🧪 Tested with:** NZXT Kraken 2023 (Z-Series) - Developer's personal system  
**🔗 Compatible:** NZXT Kraken X-Series, Z-Series and other LCD-capable models *(theoretical)*

## ✨ Features

- **🏗️ Modular Architecture**: Separation of CPU, GPU, and display logic into separate modules
- **⚡ Performance-Optimized**: Caching, change detection, minimal I/O operations
- **🔧 Automatic Device Detection**: Runtime UID detection.
- **🎨 Display Modes (legacy)**: The program currently always runs in a fixed two-box layout (CPU/GPU temperature only). Mode selection is not available in this version. Support for selectable display modes (e.g. load bars, circular diagrams) may be reintroduced in a future version if there is sufficient demand.
- **🌐 Native CoolerControl Integration**: REST API communication without Python dependencies
- **📊 Efficient Sensor Polling**: Only necessary sensor data is queried (no mode logic)
- **🔄 Systemd Integration**: Service management with detailed logs
- **🚀 Intelligent Installation**: Automatic dependency detection and installation for all major Linux distributions
- **🔧 Built with strict C99 compliance** for maximum portability and standards conformance.

**Supported Distributions (Auto-Detected):**
- **Arch Linux / Manjaro**: `pacman -S cairo libcurl-gnutls coolercontrol libini gcc make pkg-config`
- **Ubuntu / Debian**: `apt install libcairo2-dev libcurl4-openssl-dev coolercontrol libini-dev gcc make pkg-config`
- **Fedora**: `dnf install cairo-devel libcurl-devel coolercontrol libini-devel gcc make pkg-config`
- **RHEL / CentOS**: `yum install cairo-devel libcurl-devel coolercontrol libini-devel gcc make pkg-config`
- **openSUSE**: `zypper install cairo-devel libcurl-devel coolercontrol libini-devel gcc make pkg-config`

## 📦 Installation

### System Requirements

- **OS**: Linux (hwmon support required)
- **CoolerControl**: Version 1.0+ (REQUIRED - must be installed and running)
- **CPU**: x86-64-v3 compatible (Intel Haswell+ 2013+ / AMD Excavator+ 2015+)
- **LCD**: LCD displays supported by CoolerControl (Asus, NZXT, etc.)
- **Resources**: < 4 MB RAM, < 1-2% CPU load

**For older CPUs**: Use `CFLAGS=-march=x86-64 make` for compatibility

### Prerequisites

1. **Install CoolerControl**: [Installation Guide](https://gitlab.com/coolercontrol/coolercontrol/-/blob/main/README.md)
2. **Start CoolerControl daemon**: `sudo systemctl start coolercontrold`
3. **Set LCD mode**: In CoolerControl GUI, set your LCD display to **"Image/gif"** mode!

### Install CoolerDash

#### Arch Linux (Recommended)

```bash
# STEP 1: Clone repository
git clone https://github.com/damachine/coolerdash.git
cd coolerdash

# STEP 2: Start CoolerControl daemon  if not already running
sudo systemctl start coolercontrold

# STEP 3: Build and install (includes automatic dependency management)
makepkg -si

# STEP 4: Enable autostart and start CoolerDash
sudo systemctl enable --now coolerdash.service

# STEP 5: (optional) Status CoolerDash service
sudo systemctl status coolerdash.service
journalctl -xeu coolerdash.service
```

#### Manual Installation (All Distributions)

```bash
# STEP 1: Clone repository
git clone https://github.com/damachine/coolerdash.git
cd coolerdash

# STEP 2: Start CoolerControl daemon  if not already running
sudo systemctl start coolercontrold

# STEP 3: Build and install (auto-detects Linux distribution and installs dependencies)
sudo make install

# STEP 4: Enable autostart
sudo systemctl enable --now coolerdash.service

# STEP 5: (optional) Status CoolerDash service
sudo systemctl status coolerdash.service
journalctl -xeu coolerdash.service
```

### Manual Usage 

```bash
# Run manually
coolerdash

# Or use full path
/opt/coolerdash/bin/coolerdash

# From directory
./coolerdash
```
> **Note:** The systemd service must be stop before running manually to avoid conflicts:
```bash
sudo systemctl stop coolerdash.service
```

## ⚙️ Configuration
There is no configuration needed.
CoolerDash is pre-configured to use the default mode.

> **Runtime configuration:** All settings are managed exclusively via `/etc/coolerdash/config.ini`. After editing the config file, restart the service with `sudo systemctl restart coolerdash.service` to apply your changes.
>
> **If `/etc/coolerdash/config.ini` does not exist, CoolerDash will use built-in defaults.**

### Important customizable values

All relevant configuration options (display, thresholds, colors, paths, daemon settings) are set in `/etc/coolerdash/config.ini`. Edit this file to customize CoolerDash to your needs. No changes to source files are required.

> **Tip:** Edit `/etc/coolerdash/config.ini` to change the look, update interval, thresholds, or LCD behavior to your needs.

**For troubleshooting**, you can manually check devices:
```bash
# Start CoolerControl (if not running)
sudo systemctl start coolercontrold

# Check available devices
curl http://localhost:11987/devices | jq
```

**Example CoolerControl API output:**
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
```
> **💡 Note**: The daemon automatically finds and uses devices with LCD capability.

### Performance Notes

- **Mode** - Only temperature sensors, minimal I/O (~3.4MB RAM, <1% CPU)
- **Sensor caching**: hwmon paths cached at startup, GPU data cached for 3 seconds
- **Change detection**: PNG only updated when significant changes occur

## 🔍 Troubleshooting

### Common Issues

- **"Connection refused"**: CoolerControl daemon not running → `sudo systemctl start coolercontrold`
- **"Device not found"**: LCD not configured in CoolerControl → Use CoolerControl GUI  
- **"Empty JSON response"**: No devices found → Check CoolerControl configuration and LCD connection
- **"UID not working"**: Wrong device UID → Verify with `curl http://localhost:11987/devices | jq` and copy exact UID
- **"Image send fails on attempt"** and **"Segmentation fault or assertion error in coolercontrol-liqctld"**:  
  If you see errors like  
  ```
  AssertionError: missing messages (attempts=12, missing=1)

  or a segmentation fault in the `coolercontrol-liqctld` service.
  ```

  **Solution:** 
  - In most cases, this error can be ignored: CoolerDash will send the image again on the next update cycle and it will appear on the LCD automatically. 

> If you encounter persistent segmentation faults or assertion errors in the CoolerControl backend, please report them upstream at [CoolerControl GitLab](https://gitlab.com/coolercontrol/coolercontrol/-/issues) with detailed logs.

## Troubleshooting: Manual Installation Conflicts
If you see errors like "conflicting files" or "manual installation detected" during `makepkg -si`, this means CoolerDash was previously installed manually (via `make install`).

**Solution:**
- The PKGBUILD will attempt to clean up automatically.
- If problems persist, run:
  ```
  sudo make uninstall
  ```
- Remove any leftover files in `/opt/coolerdash/`, `/usr/bin/coolerdash`, and `/etc/systemd/system/coolerdash.service`.
- Then retry the installation.

If you need help, open an issue at https://github.com/damachine/coolerdash/issues

> **Note:** The program always runs in a fixed two-box layout (CPU/GPU temperature only). Mode selection is currently not available. Support for selectable display modes may be reintroduced in a future version if there is sufficient demand.

## Troubleshooting: Systemd Service User Issues
If you encounter errors like "User not found" or "Permission denied" when starting the systemd service, it may be due to a missing or misconfigured system user for CoolerDash.

**Solution:**
- CoolerDash is designed to run as a dedicated user for security. If the user was not created automatically, you can add it manually:
  ```bash
  sudo useradd --system --no-create-home --shell /usr/sbin/nologin coolerdash
  ```
- Ensure all relevant directories are owned by the `coolerdash` user:
  ```bash
  sudo chown -R coolerdash:coolerdash /opt/coolerdash
  sudo chown -R coolerdash:coolerdash /run/coolerdash
  sudo chown -R coolerdash:coolerdash /tmp/coolerdash
  ```
- If you use a custom config or image path, check permissions for `/etc/coolerdash/config.ini` and `/tmp/coolerdash/coolerdash.png`.
- After creating the user and setting permissions, restart the service:
  ```bash
  sudo systemctl restart coolerdash.service
  ```

> **Note:** The systemd unit file expects the user `coolerdash` to exist. If you use a different username, edit `/etc/systemd/system/coolerdash.service` accordingly.

## 🔧 Usage & Tips

### Service Management

```bash
# Service control
sudo systemctl start coolerdash.service     # Start
sudo systemctl stop coolerdash.service      # Stop (displays face.png automatically)
sudo systemctl restart coolerdash.service   # Restart
sudo systemctl status coolerdash.service    # Status + recent logs

# Journal log
journalctl -xeu coolerdash.service

# Live logs
sudo journalctl -u coolerdash.service -f

# Makefile shortcuts
make start      # systemctl start coolerdash
make stop       # systemctl stop coolerdash
make status     # systemctl status coolerdash
make logs       # journalctl -u coolerdash -f
```

### Build Commands

```bash
make            # Standard C99 build
make clean      # Clean up
make install    # System installation with dependency auto-detection
make uninstall  # Remove installation (service, binary, files)
make debug      # Debug build with AddressSanitizer
make help       # Show all options
```

### Debugging Steps

```bash
# 1. Check CoolerControl status
sudo systemctl status coolercontrold
curl http://localhost:11987/devices

# 2. Test CoolerDash manually
coolerdash

# 3. Debug build for detailed information
make debug && coolerdash

# 4. Check service logs
sudo journalctl -u coolerdash.service -f
```

## 📄 License

MIT License - See LICENSE file for details.

## 💝 Support the Project

If you find CoolerDash useful and want to support its development:

### 🪙 Cryptocurrency Donations:
- **Bitcoin (BTC)**: `13WjpWQMGG5sg3vTJJnCX3cXzwf2vZddKo`
- **Dogecoin (DOGE)**: `DRSY4cA8eCALn819MjWLbwaePFNti9oS3y`

### 🤝 Other Ways to Support:
- ⭐ **Star this repository** on GitHub
- 🐛 **Report bugs** and suggest improvements  
- 🔄 **Share** the project with others
- 📝 **Contribute** code or documentation

> *All donations help maintain and improve this project. Thank you for your support!*

---

**👨‍💻 Developed by DAMACHINE for maximum efficiency and stability.**  
**📧 Contact:** [christkue79@gmail.com](mailto:christkue79@gmail.com)  
**📖 Manual:** `man coolerdash`  
**📍 Binary:** `/opt/coolerdash/bin/coolerdash` (also available as `coolerdash`)  
**💝 Donate:** BTC: `13WjpWQMGG5sg3vTJJnCX3cXzwf2vZddKo` | DOGE: `DRSY4cA8eCALn819MjWLbwaePFNti9oS3y`
