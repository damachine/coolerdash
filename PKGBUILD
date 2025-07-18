# Project coding standards and packaging notes (see README for details)
#
# Example version: 0.2025.07.16.1234 (Year 2025, July 16th, 12:34)
#
# Maintainer: DAMACHINE <christkue79@gmail.com>
#
# --- Dependency notes ---
# - 'cairo', 'libcurl-gnutls', 'coolercontrol' are required for core functionality
# - 'nvidia-utils' and 'lm_sensors' are optional for extended hardware monitoring
# - 'ttf-roboto' is required for proper font rendering on the LCD
# - All dependencies are documented in README and AUR-README
# ------------------------

plain '         .--.  '
plain '        |o_o | '
plain '        |:_/ | '
plain '       //   \ \ '
plain '      (|     | ) '
plain '     /'\''\_   _/'\''\ '
plain '      \___)=(___/ '
plain ' '

pkgname=coolerdash
pkgver=0.25.07.19.2
pkgrel=1
pkgdesc="CoolerDash a Wrapper for LCD Device Image Control in CoolerControl. To take full control of your liquid cooling system with integrated LCD display to monitor real-time sensor data in style."
arch=('x86_64')
url="https://github.com/damachine/coolerdash"
license=('MIT')
depends=('cairo' 'libcurl-gnutls' 'coolercontrol')
makedepends=('gcc' 'make' 'pkg-config' 'ttf-roboto')
optdepends=('nvidia-utils: for GPU temperature monitoring'
            'lm_sensors: for additional hardware monitoring')
backup=('opt/coolerdash/README.md')
install=coolerdash.install
source=()
sha256sums=()

prepare() {
    echo "================================================================"
    echo "Checking for existing manual installation..."
    echo "================================================================"
    
    # Check if manual installation exists
    local manual_installed=0
    local manual_service_exists=0
    local manual_binary_exists=0
    
    # Check for systemd service installed by Makefile
    if [[ -f "/etc/systemd/system/coolerdash.service" ]]; then
        echo "Found manual systemd service: /etc/systemd/system/coolerdash.service"
        manual_service_exists=1
        manual_installed=1
    fi
    
    # Check for binary installed by Makefile
    if [[ -f "/opt/coolerdash/bin/coolerdash" ]] && [[ ! -L "/usr/bin/coolerdash" ]]; then
        echo "Found manual binary installation: /opt/coolerdash/bin/coolerdash"
        manual_binary_exists=1
        manual_installed=1
    fi
    
    if [[ $manual_installed -eq 1 ]]; then
        echo ""
        echo "WARNING: Existing manual installation detected!"
        echo "This will conflict with the PKGBUILD installation."
        echo ""
        echo "Attempting automatic cleanup with 'make uninstall'..."
        echo ""
        
        # Try to run make uninstall if Makefile exists
        if [[ -f "Makefile" ]]; then
            # Stop service if running
            if systemctl is-active --quiet coolerdash.service 2>/dev/null; then
                echo "Stopping coolerdash service..."
                sudo systemctl stop coolerdash.service || true
            fi
            
            # Run make uninstall
            echo "Running 'sudo make uninstall'..."
            if sudo make uninstall; then
                echo "✅ Manual installation successfully removed"
            else
                echo "⚠️  Make uninstall failed, attempting manual cleanup..."
                
                # Complete manual cleanup if make uninstall fails
                sudo systemctl stop coolerdash.service 2>/dev/null || true
                sudo systemctl disable coolerdash.service 2>/dev/null || true
                sudo rm -f /etc/systemd/system/coolerdash.service
                sudo rm -f /usr/share/man/man1/coolerdash.1
                sudo rm -f /opt/coolerdash/bin/coolerdash
                sudo rm -f /opt/coolerdash/README.md
                sudo rm -f /opt/coolerdash/LICENSE
                sudo rm -f /opt/coolerdash/CHANGELOG.md
                sudo rm -rf /opt/coolerdash/images/
                sudo rm -rf /opt/coolerdash/
                # All files and images are removed above; no need for extra image cleanup
                echo "✅ Complete manual cleanup completed"
            fi
        else
            echo "⚠️  No Makefile found, performing manual cleanup..."
            
            # Complete manual cleanup
            sudo systemctl stop coolerdash.service 2>/dev/null || true
            sudo systemctl disable coolerdash.service 2>/dev/null || true
            sudo rm -f /etc/systemd/system/coolerdash.service
            sudo rm -f /usr/share/man/man1/coolerdash.1
            sudo rm -f /opt/coolerdash/bin/coolerdash
            sudo rm -f /opt/coolerdash/README.md
            sudo rm -f /opt/coolerdash/LICENSE
            sudo rm -f /opt/coolerdash/CHANGELOG.md
            sudo rm -rf /opt/coolerdash/images/
            sudo rm -rf /opt/coolerdash/
            sudo rm -f /usr/bin/coolerdash 2>/dev/null || true
            sudo systemctl daemon-reload
            
            echo "✅ Complete manual cleanup completed"
        fi
        
        echo ""
        echo "Proceeding with PKGBUILD installation..."
    else
        echo "✅ No conflicting manual installation found"
    fi
    
    # Additional cleanup for any remaining conflicting files
    # All files and images are already removed above; no further cleanup needed
    echo "✅ Final cleanup completed"
}

build() {
    # For local build: use current directory directly
    cd "$startdir"

    # Remove all previous tarball builds
    rm -f *.pkg.tar.*

    # Clean any previous builds
    make clean || true

    # Build with Arch Linux specific optimizations and C99 compliance
    make CC=gcc CFLAGS="-Wall -Wextra -O2 -std=c99 -march=x86-64-v3 -Iinclude $(pkg-config --cflags cairo)" \
         LIBS="$(pkg-config --libs cairo) -lcurl -lm"
}

check() {
    cd "$startdir"
    
    # Basic functionality test
    if [[ -f "bin/coolerdash" ]]; then
        echo "Build successful - binary created"
        ./bin/coolerdash --help > /dev/null && echo "Help function works"
    else
        echo "ERROR: Binary not found"
        return 1
    fi
}

package() {
  install -dm755 "$pkgdir/opt/coolerdash/bin"
  install -Dm755 "$srcdir/bin/coolerdash" "$pkgdir/opt/coolerdash/bin/coolerdash"
  install -dm755 "$pkgdir/opt/coolerdash/images"
  install -Dm644 "$srcdir/images/shutdown.png" "$pkgdir/opt/coolerdash/images/shutdown.png"
  install -Dm644 "$srcdir/images/coolerdash.png" "$pkgdir/opt/coolerdash/images/coolerdash.png"
  install -Dm644 "$srcdir/README.md" "$pkgdir/opt/coolerdash/README.md"
  install -Dm644 "$srcdir/LICENSE" "$pkgdir/opt/coolerdash/LICENSE"
  install -Dm644 "$srcdir/CHANGELOG.md" "$pkgdir/opt/coolerdash/CHANGELOG.md"
  install -Dm644 "$srcdir/systemd/coolerdash.service" "$pkgdir/etc/systemd/system/coolerdash.service"
  install -Dm644 "$srcdir/man/coolerdash.1" "$pkgdir/usr/share/man/man1/coolerdash.1"
}
