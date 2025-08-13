# -----------------------------------------------------------------------------
# author: damachine (christkue79@gmail.com)
# website: https://github.com/damachine
# copyright: (c) 2025 damachine
# license: MIT
# version: 1.0
#
# Info:
# 	CoolerDash PKGBUILD
#   Build system for CoolerDash (C99 LCD daemon)
#	Project coding standards and packaging notes (see README for details)
# 	Maintainer: DAMACHINE <christkue79@gmail.com>
# Details:
#   This PKGBUILD handles build, install, dependencies, and packaging for Arch/AUR.
#   Edit dependencies, paths, and user as needed for your system.
#   Do not run as root. Use dedicated user for security.
#   Ensure all required dependencies are installed.
#   It uses color output and Unicode icons for better readability. All paths and dependencies are configurable.
#   See README.md and AUR-README.md for further details.
# Example:
#   makepkg -si
#   makepkg -s
#   makepkg -c
#   makepkg -f
#
# --- Dependency notes ---
# -		'cairo', 'libcurl-gnutls', 'libinih', 'coolercontrol' are required for core functionality
# - 	'nvidia-utils' and 'lm_sensors' are optional for extended hardware monitoring
# - 	'ttf-roboto' is required for proper font rendering on the LCD
# - All dependencies are documented in README.md and AUR-README.md
# -----------------------------------------------------------------------------
pkgname=coolerdash
pkgver=$(cat VERSION)
pkgrel=1
pkgdesc="CoolerDash - LCD dashboard for CoolerControl"
arch=('x86_64')
url="https://github.com/damachine/coolerdash"
license=('MIT')
depends=('cairo' 'libcurl-gnutls' 'libinih' 'coolercontrol' 'ttf-roboto' 'jansson')
makedepends=('gcc' 'make' 'pkg-config')
optdepends=('nvidia-utils: for GPU temperature monitoring'
            'lm_sensors: for additional hardware monitoring')
backup=('etc/coolerdash/config.ini')
install=coolerdash.install
source=()
sha256sums=()

build() {
    echo "================================================================"
    plain ' '
    plain '         .--.  '
    plain '        |o_o | '
    plain '        |:_/ | '
    plain '       //   \ \ '
    plain '      (|     | ) '
    plain '      /'\''\_   _/'\''\ '
    plain '      \___)=(___/ '
    plain ' '

    # For local build: use current directory directly
    cd "$startdir"

    # Remove all previous tarball builds
    rm -rf coolerdash-*.pkg.* || true

    # Clean any previous builds
    make clean || true

    # Build with Arch Linux specific optimizations and C99 compliance
    make

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
    echo "================================================================"
}

check() {
    # For local build: use current directory directly
    cd "$startdir"

    if [[ -f bin/coolerdash ]]; then
        echo "Build successful - binary created"
        #bin/coolerdash --help > /dev/null && echo "Help function works"
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
    install -dm755 "$pkgdir/run/coolerdash"
}
