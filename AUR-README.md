# AUR Package Instructions for CoolerDash

## ⚡ Recommended Installation (Arch Linux)

CoolerDash is best installed via the AUR using `makepkg -si` for full dependency management and automatic conflict cleanup.

### Quick Install (AUR/Local Build)

```bash
# 1. Clone the repository
 git clone https://github.com/damachine/coolerdash.git
 cd coolerdash

# 2. Start CoolerControl daemon (required)
 sudo systemctl start coolercontrold

# 3. Build and install with all dependencies and cleanup
 makepkg -si

# 4. Enable and start CoolerDash service
 sudo systemctl enable --now coolerdash.service

# 5. Check service status
 sudo systemctl status coolerdash.service
 journalctl -xeu coolerdash.service
```

### Alternative: Local .pkg.tar.zst Installation

```bash
# 1. Build the package (creates .pkg.tar.zst)
 makepkg -s

# 2. Install the package manually
 sudo pacman -U coolerdash-*.pkg.tar.zst

# 3. Enable and start service
 sudo systemctl enable --now coolerdash.service
```

---

## Further Information

For all additional details, usage instructions, configuration options, troubleshooting, and development notes, please refer to the main [README.md](README.md) file in the project root.

The main README contains comprehensive documentation and is the authoritative source for all technical and user

---

**Developed and tested on Arch Linux. For other distributions, see the main README.md.**
