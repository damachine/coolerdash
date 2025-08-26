# -----------------------------------------------------------------------------
# Author: damachine (christkue79@gmail.com)
# Maintainer: damachine <christkue79@gmail.com>
# Website: https://github.com/damachine
# Copyright: (c) 2025 damachine
# License: MIT
# Version: 1.0
#   This software is provided "as is", without warranty of any kind, express or implied.
#   I do not guarantee that it will work as intended on your system.
#
# Info:
#   CoolerDash 'PKGBUILD' - For local building only via 'git clone https://github.com/damachine/coolerdash.git'!
#   This 'PKGBUILD' is not hosted on AUR and is intended for local use only!
#   If you want automatic updates and AUR hosting, please use the 'coolerdash-git' package from AUR!
#   This 'PKGBUILD' is designed for Arch Linux and derivatives.
#   Build system for CoolerDash (C99 LCD daemon)
#   Project coding standards and packaging notes (see README for details)
#
# Details:
#   This PKGBUILD handles build, install, dependencies, and packaging local building!
#   Edit dependencies, paths, and user as needed for your system.
#   Do not run as root. Use dedicated user for security.
#   Ensure all required dependencies are installed.
#   It uses color output and Unicode icons for better readability. All paths and dependencies are configurable.
#   See 'README.md' and 'AUR-README.md' for further details.
#
# Build:
#   makepkg -si
#
# Dependency:
#   'cairo' 'coolercontrol' 'jansson' 'libcurl-gnutls' 'libinih' are required for core functionality
#   'ttf-roboto' is required for proper font rendering on the LCD
#   All dependencies are documented in README.md and AUR-README.md
# -----------------------------------------------------------------------------
pkgname=coolerdash
pkgver=$(cat VERSION)
pkgrel=1
provides=('coolerdash-git')
replaces=('coolerdash-git')
conflicts=('coolerdash-git')
pkgdesc="Extends CoolerControl with a polished LCD dashboard"
arch=('x86_64')
url="https://github.com/damachine/coolerdash"
license=('MIT')
depends=('cairo' 'coolercontrol' 'jansson' 'libcurl-gnutls' 'libinih' 'ttf-roboto')
makedepends=('gcc' 'make' 'pkg-config' 'git')
optdepends=()
backup=('etc/coolerdash/config.ini')
install=coolerdash.install
source=()
sha256sums=()

build() {
    # For local build: use current directory directly
    cd "$startdir"

    # Remove all previous tarball builds
    rm -rf coolerdash-*.pkg.* || true
    rm -rf build bin || true
    mkdir -p build bin || true

    # Clean any previous builds
    make clean || true

    # Build with Arch Linux specific optimizations and C99 compliance
    make || return 1

    # Copy binary to $srcdir/bin for packaging
    mkdir -p "$srcdir/bin"
    cp -a bin/coolerdash "$srcdir/bin/coolerdash"

    # Copy all required files for packaging to $srcdir
    cp -a README.md "$srcdir/README.md"
    cp -a AUR-README.md "$srcdir/AUR-README.md"
    cp -a CHANGELOG.md "$srcdir/CHANGELOG.md"
    cp -a VERSION "$srcdir/VERSION"
    cp -a LICENSE "$srcdir/LICENSE"
    cp -a etc/coolerdash/config.ini "$srcdir/config.ini"
    mkdir -p "$srcdir/images"
    cp -a images/shutdown.png "$srcdir/images/shutdown.png"
    mkdir -p "$srcdir/systemd"
    cp -a etc/systemd/coolerdash.service "$srcdir/systemd/coolerdash.service"
    mkdir -p "$srcdir/man"
    cp -a man/coolerdash.1 "$srcdir/man/coolerdash.1"
}

check() {
    # For local build: use current directory directly
    cd "$startdir"

    if [[ -f bin/coolerdash ]]; then
        echo "Build successful - binary created"
    else
        echo "ERROR: Binary not found"
        return 1
    fi
}

package() {
    # For local build: use current directory directly
    install -dm755 "$pkgdir/opt/coolerdash"
    install -Dm644 "$srcdir/AUR-README.md" "$pkgdir/opt/coolerdash/AUR-README.md"
    install -Dm644 "$srcdir/README.md" "$pkgdir/opt/coolerdash/README.md"
    install -Dm644 "$srcdir/VERSION" "$pkgdir/opt/coolerdash/VERSION"
    install -Dm644 "$srcdir/LICENSE" "$pkgdir/opt/coolerdash/LICENSE"
    install -Dm644 "$srcdir/CHANGELOG.md" "$pkgdir/opt/coolerdash/CHANGELOG.md"
    install -Dm644 "$srcdir/config.ini" "$pkgdir/etc/coolerdash/config.ini"
    install -dm755 "$pkgdir/opt/coolerdash/bin"
    install -Dm755 "$srcdir/bin/coolerdash" "$pkgdir/opt/coolerdash/bin/coolerdash"
    install -dm755 "$pkgdir/opt/coolerdash/images"
    install -Dm644 "$srcdir/images/shutdown.png" "$pkgdir/opt/coolerdash/images/shutdown.png"
    install -dm755 "$pkgdir/usr/bin"
    ln -sf /opt/coolerdash/bin/coolerdash "$pkgdir/usr/bin/coolerdash"
    install -Dm644 "$srcdir/systemd/coolerdash.service" "$pkgdir/etc/systemd/system/coolerdash.service"
    install -Dm644 "$srcdir/man/coolerdash.1" "$pkgdir/usr/share/man/man1/coolerdash.1"
}
