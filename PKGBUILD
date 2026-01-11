# -----------------------------------------------------------------------------
# Created by: damachine (damachine3 at proton dot me)
# Website: https://github.com/damachine/coolerdash
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
backup=('etc/coolercontrol/plugins/coolerdash/config.ini')
install=coolerdash.install
source=()
sha256sums=()

build() {
    # For local build: use current directory directly
    cd "${startdir}"

    # Fetch latest tags if in git repo
    if git rev-parse --git-dir >/dev/null 2>&1; then
        echo "Fetching latest tags..."
        git fetch --tags 2>/dev/null || true
    fi

    # Remove all previous tarball builds
    rm -rf coolerdash-*.pkg.* || true

    # Clean any previous builds
    make clean || true

    # Build with Arch Linux specific optimizations and C99 compliance
    make || return 1

    # Copy files to srcdir for packaging (fakeroot cannot access startdir)
    mkdir -p "${srcdir}/bin" "${srcdir}/images"
    cp -a bin/coolerdash "${srcdir}/bin/coolerdash"
    cp -a README.md CHANGELOG.md VERSION LICENSE "${srcdir}/"
    cp -a etc/coolerdash/config.ini "${srcdir}/"
    cp -a images/shutdown.png "${srcdir}/images/"
    cp -a man/coolerdash.1 "${srcdir}/"
    mkdir -p "$srcdir/plugins/coolercontrol"
    cp -a etc/coolercontrol/plugins/coolerdash/manifest.toml "$srcdir/plugins/coolercontrol/manifest.toml"
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
    # Plugin-mode installation: Everything in /etc/coolercontrol/plugins/coolerdash/
    install -dm755 "${pkgdir}/etc/coolercontrol/plugins/coolerdash"
    install -Dm755 "${srcdir}/bin/coolerdash" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/coolerdash"
    install -Dm644 "${srcdir}/README.md" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/README.md"
    install -Dm644 "${srcdir}/VERSION" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/VERSION"
    install -Dm644 "${srcdir}/LICENSE" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/LICENSE"
    install -Dm644 "${srcdir}/CHANGELOG.md" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/CHANGELOG.md"
    install -Dm644 "${srcdir}/config.ini" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/config.ini"
    install -Dm644 "${srcdir}/images/shutdown.png" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/shutdown.png"
    install -Dm644 "$srcdir/plugins/coolercontrol/manifest.toml" "$pkgdir/etc/coolercontrol/plugins/coolerdash/manifest.toml"
    # Manual page
    install -Dm644 "${srcdir}/coolerdash.1" "${pkgdir}/usr/share/man/man1/coolerdash.1"
}
