
# CoolerDash - Display real-time sensor on AIO liquid LCD device.

<p align="center">
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-green.svg"></a>
  <a href="https://kernel.org/"><img src="https://img.shields.io/badge/Platform-Linux-green.svg"></a>
  <a href="https://en.wikipedia.org/wiki/C99"><img src="https://img.shields.io/badge/C-99-blue.svg"></a>
  <a href="https://app.codacy.com/gh/damachine/coolerdash/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/056718c967524322acd4f4f76393fd7f"></a>
  <a href="https://github.com/damachine/coolerdash/actions/workflows/github-code-scanning/codeql"><img src="https://github.com/damachine/coolerdash/actions/workflows/github-code-scanning/codeql/badge.svg"></a>
  <a href="https://github.com/damachine/coolerdash/actions/workflows/install.yml"><img src="https://github.com/damachine/coolerdash/actions/workflows/install.yml/badge.svg"></a>
  <a href="https://github.com/damachine/coolerdash/actions/workflows/aur.yml"><img src="https://github.com/damachine/coolerdash/actions/workflows/aur.yml/badge.svg"></a>
  <a href="https://aur.archlinux.org/packages/coolerdash-git"><img src="https://img.shields.io/aur/version/coolerdash-git?color=blue&label=AUR"></a>
  <a href="https://github.com/damachine/coolerdash/issues"><img src="https://img.shields.io/github/issues/damachine/coolerdash"></a>
  <a href="https://github.com/sponsors/damachine"><img src="https://img.shields.io/badge/Sponsor-GitHub-blue?logo=github-sponsors"></a>
</p>

## 📖 Description

**CoolerDash is an add-on wrapper for [CoolerControl](https://gitlab.com/coolercontrol/coolercontrol) that enhances your water-cooling display with extra features and a polished, customizable LCD dashboard.**

> ##### Special thanks to @codifryed, the founder of CoolerControl 

### 📸 Screenshot – Example LCD Output

<div align="center">
  <img src="images/animation.gif" alt="CoolerDash LCD Animation" width="240" height="240"/> 
  <img src="images/round.png" alt="CoolerDash LCD Demo" width="240" height="240"/> 
  <img src="images/quad.png" alt="CoolerDash LCD Demo" width="240" height="240"/> 
</div>

---

## 🖥️ System Requirements

- **OS**: Linux
- **CoolerControl**: Version >=2.2.2 REQUIRED - must be installed and running
- **CPU**: x86-64-v3 compatible (Intel Haswell+ 2013+ / AMD Excavator+ 2015+)
- **LCD**: LCD displays supported by CoolerControl **(Asus, MSI, NZXT, etc.)**  
See the [Supported Devices (Compatibility List)](docs/devices.md) for a list of confirmed working hardware.  
To confirm a device, [submit a Device Confirmation Issue](https://github.com/damachine/coolerdash/issues/new?template=device-confirmation.yml).

**Supported Distributions:**

- **Arch Linux / CachyOS / Manjaro (Recommended)**
- **Debian / Ubuntu**
- **Fedora**
- **RHEL / CentOS**
- **openSUSE**


## 🛠️ Prerequisites

1. **Install CoolerControl**: [Installation Guide](https://gitlab.com/coolercontrol/coolercontrol/-/blob/main/README.md)
2. **Start/Enable CoolerControl daemon**: `systemctl start coolercontrold`
3. **In the CoolerControl settings, under "Device and Sensor", select one sensor for the CPU and one for the GPU.**

> ℹ️ Skip this step if you have already configured your CoolerControl before.
---

## 📦 Installation

#### Arch Linux (Recommended)

[![AUR](https://img.shields.io/aur/version/coolerdash-git?color=blue&label=AUR)](https://aur.archlinux.org/packages/coolerdash-git)

- Using an AUR helper (recommended):
```bash
# STEP 1: Install
yay -S coolerdash-git

# STEP 2: Enable and start CoolerDash (after install or update)
systemctl daemon-reload
systemctl enable --now coolerdash.service
```

- Manual AUR install (no AUR helper):
```bash
# STEP 1: Clone repository
git clone https://aur.archlinux.org/coolerdash-git.git
cd coolerdash-git
makepkg --printsrcinfo > .SRCINFO
makepkg -si

# STEP 2: Start CoolerControl daemon if not already running
systemctl start coolercontrold

# STEP 4: Enable autostart and start CoolerDash
systemctl daemon-reload
systemctl enable --now coolerdash.service

# STEP 5: (optional) Check CoolerDash service status
systemctl status coolerdash.service
journalctl -u coolerdash.service
```

#### All Distributions (Manual Installation)

```bash
# STEP 1: Clone repository
git clone https://github.com/damachine/coolerdash.git
cd coolerdash

# STEP 2: Start CoolerControl daemon if not already running
systemctl start coolercontrold

# STEP 3: Build and install (auto-detects Linux distribution and installs dependencies)
make install

# STEP 4: Enable autostart
systemctl daemon-reload
systemctl enable --now coolerdash.service

# STEP 5: (optional) Check CoolerDash service status
systemctl status coolerdash.service
journalctl -u coolerdash.service
```

> **Note:** For manual installations, please make sure all required dependencies are installed correctly. Only package manager installations receive automatic updates; At this time, manual installations need to be updated manually.

---

## ⚙️ Configuration

**ℹ️ CoolerControl configuration**
- In the CoolerControl settings, under **"Device"** and **"Sensor"**, select one sensor for the CPU and one for the GPU.
- In CoolerControl GUI, set your LCD display to **"Image/gif"**.

**ℹ️ Runtime Configuration:**
- All settings are managed in `/etc/coolerdash/config.ini`.
- After editing the config file, restart the service with `systemctl restart coolerdash.service` to apply your changes.

**ℹ️ NOTE:**
- When CoolerDash stops (for example during system shutdown or reboot), it automatically displays the `shutdown.png` image from the install path. This happens because sensor data is no longer available at that point.
- You can customize this and much more as you wish, by editing the `/etc/coolerdash/config.ini` file.

---

## 🔧 Usage

#### Service Management

```bash
# Service control
systemctl enable --now coolerdash.service     # Enable and Start!
systemctl start coolerdash.service            # Start
systemctl stop coolerdash.service             # Stop
systemctl restart coolerdash.service          # Restart
systemctl status coolerdash.service           # Status + recent logs

# Journal log
journalctl -u coolerdash.service

# Live logs
journalctl -u coolerdash.service -f
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

# Run with detailed debug logging
coolerdash --log
```
> **Note:** The systemd service must be stopped before running manually to avoid conflicts:
```bash
systemctl stop coolerdash.service
```

#### Debugging Steps

```bash
# 1. Check CoolerControl status
systemctl status coolercontrold
curl http://localhost:11987/devices

# 2. Test CoolerDash manually (with clean output)
coolerdash

# 3. Test CoolerDash with detailed debug logging
coolerdash --log

# 4. Debug build for detailed information (if needed)
make debug && coolerdash --log

# 5. Check service logs (STATUS messages always visible)
journalctl -u coolerdash.service -f

# 6. View recent logs with context
journalctl -u coolerdash.service -n 50
```

---

## 🔍 Troubleshooting

#### Common Issues

- **"Device not found"**: LCD not configured in CoolerControl → Use CoolerControl GUI → set LCD mode to `Image/gif` 
- **"Connection refused"**: CoolerControl daemon not running → `systemctl start coolercontrold`
- **"Connection problem"**: No devices found or wrong device UID → Check CoolerControl configuration and LCD connection → Verify with `curl http://localhost:11987/devices | jq`

***Solution:***
- Manually check if devices are detected correctly:
```bash
# Start CoolerControl (if not running)
systemctl start coolercontrold

# Check available devices
curl http://localhost:11987/devices | jq
```
#### Example:
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

- **Manual Installation Conflicts**: If you see errors like "conflicting files" or "manual installation detected" during `makepkg -si`, this means CoolerDash was previously installed manually (via `make install`).

***Solution:***
- The PKGBUILD will attempt to clean up automatically.
- If problems persist, run:
  ```
  sudo make uninstall
  ```
- Remove any leftover files in `/opt/coolerdash/`, `/usr/bin/coolerdash`, and `/etc/systemd/system/coolerdash.service`.
- Then retry the installation.

**Other**
If you need help, open an issue at https://github.com/damachine/coolerdash/issues

---

## ⚠️ Disclaimer

This software is provided "as is", without warranty of any kind, express or implied.  
I do not guarantee that it will work as intended on your system. 

---

## 📄 License

MIT License - See LICENSE file for details.

[![MIT License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

Individual Coolercontrol package have their own licenses:

See respective repositories at [https://gitlab.com/coolercontrol/coolercontrol](https://gitlab.com/coolercontrol/coolercontrol)

---

## 💝 Support the Project

If you find CoolerDash useful and want to support its development:

- ⭐ **Star this repository** on GitHub
- 🐛 **Report bugs** and suggest improvements
- 🔄 **Share** the project with others
- 📝 **Contribute** code or documentation
- [![Sponsor](https://img.shields.io/badge/Sponsor-GitHub-blue?logo=github-sponsors)](https://github.com/sponsors/damachine)

> *🙏 Your support keeps this project alive and improving — thank you!.*

---

**👨‍💻 Developed by DAMACHINE** 
**📧 Contact:** [christkue79@gmail.com](mailto:christkue79@gmail.com) 
**🌐 Repository:** [GitHub](https://github.com/damachine/coolerdash) 
