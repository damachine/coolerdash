# Maintainer: damachin3 (damachine3 at proton dot me)
# Website: https://github.com/damachine/coolerdash

# This PKGBUILD is for building the coolerdash package from local source.
# It assumes the source code is already present in the current directory.

pkgname=coolerdash
pkgver=$(cat VERSION)
pkgrel=1
provides=('coolerdash-git')
replaces=('coolerdash-git')
conflicts=('coolerdash-git')
pkgdesc="Monitor telemetry data on an AIO liquid cooler with an integrated LCD display"
arch=('x86_64')
url="https://github.com/damachine/coolerdash"
license=('MIT')
depends=('cairo' 'coolercontrol' 'jansson' 'libcurl-gnutls' 'ttf-roboto')
makedepends=('gcc' 'make' 'pkg-config' 'git')
optdepends=()
backup=('etc/coolercontrol/plugins/coolerdash/config.json')
install=coolerdash.install
source=()
sha256sums=()

build() {
    # For local build: use current directory directly
    cd "${startdir}"

    # Fetch latest tags if in git repo
    if git rev-parse --git-dir >/dev/null 2>&1; then
        echo "Fetching latest tags..."
        git fetch --tags
    fi

    # Remove all previous tarball builds
    rm -rf coolerdash-*.pkg.*

    # Clean any previous builds if a Makefile exists
    if [[ -f Makefile || -f GNUmakefile ]]; then
        make clean
    fi

    # Build the project
    make

    # Copy files to srcdir for packaging (fakeroot cannot access startdir)
    mkdir -p "${srcdir}/bin" "${srcdir}/images" "${srcdir}/man" "${srcdir}/etc/coolercontrol/plugins/coolerdash/ui" "${srcdir}/etc/applications" "${srcdir}/etc/icons" "${srcdir}/etc/udev/rules.d"
    cp -a bin/coolerdash "${srcdir}/bin/coolerdash"
    cp -a README.md CHANGELOG.md VERSION LICENSE "${srcdir}/"
    cp -a etc/coolercontrol/plugins/coolerdash/config.json "${srcdir}/etc/coolercontrol/plugins/coolerdash/"
    cp -a etc/coolercontrol/plugins/coolerdash/ui/index.html "${srcdir}/etc/coolercontrol/plugins/coolerdash/ui/"
    cp -a etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js "${srcdir}/etc/coolercontrol/plugins/coolerdash/ui/"
    cp -a images/shutdown.png "${srcdir}/images/"
    cp -a man/coolerdash.1 "${srcdir}/man/"
    cp -a etc/coolercontrol/plugins/coolerdash/manifest.toml "${srcdir}/etc/coolercontrol/plugins/coolerdash/"
    cp -a etc/applications/coolerdash.desktop "${srcdir}/etc/applications/"
    cp -a etc/icons/coolerdash.svg "${srcdir}/etc/icons/"
    cp -a etc/udev/rules.d/99-coolerdash.rules "${srcdir}/etc/udev/rules.d/"
    mkdir -p "${srcdir}/etc/systemd/cc-plugin-coolerdash.service.d"
    cp -a etc/systemd/coolerdash-helperd.service "${srcdir}/etc/systemd/"
    cp -a etc/systemd/cc-plugin-coolerdash.service.d/startup-delay.conf "${srcdir}/etc/systemd/cc-plugin-coolerdash.service.d/"
}

check() {
    # For local build: use current directory directly
    cd "${startdir}"

    # Verify that the binary was created successfully
    if [[ -f bin/coolerdash ]]; then
        msg "Build successful - binary created"
    else
        error "Build failed - binary not found"
        return 1
    fi
}

package() {
    # Binary to /usr/libexec, plugin data stays in /etc/coolercontrol/plugins/
    install -dm755 "${pkgdir}/etc/coolercontrol/plugins/coolerdash"
    install -Dm755 "${srcdir}/bin/coolerdash" "${pkgdir}/usr/libexec/coolerdash/coolerdash"
    install -Dm644 "${srcdir}/README.md" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/README.md"
    install -Dm644 "${srcdir}/VERSION" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/VERSION"
    install -Dm644 "${srcdir}/CHANGELOG.md" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/CHANGELOG.md"
    install -Dm666 "${srcdir}/etc/coolercontrol/plugins/coolerdash/config.json" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/config.json"

    install -dm755 "${pkgdir}/etc/coolercontrol/plugins/coolerdash/ui"
    install -m644 "${srcdir}/etc/coolercontrol/plugins/coolerdash/ui/index.html" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/ui/index.html"
    install -m644 "${srcdir}/etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js"
    install -Dm644 "${srcdir}/images/shutdown.png" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/shutdown.png"
    install -Dm644 "${srcdir}/etc/coolercontrol/plugins/coolerdash/manifest.toml" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/manifest.toml"

    sed -i "s/{{VERSION}}/${pkgver}/g" "${pkgdir}/etc/coolercontrol/plugins/coolerdash/manifest.toml"

    install -Dm644 "${srcdir}/man/coolerdash.1" "${pkgdir}/usr/share/man/man1/coolerdash.1"
    install -Dm644 "${srcdir}/etc/applications/coolerdash.desktop" "${pkgdir}/usr/share/applications/coolerdash.desktop"
    install -Dm644 "${srcdir}/etc/udev/rules.d/99-coolerdash.rules" "${pkgdir}/usr/lib/udev/rules.d/99-coolerdash.rules"
    install -Dm644 "${srcdir}/etc/icons/coolerdash.svg" "${pkgdir}/usr/share/icons/hicolor/scalable/apps/coolerdash.svg"
    install -Dm644 "${srcdir}/etc/systemd/coolerdash-helperd.service" "${pkgdir}/usr/lib/systemd/system/coolerdash-helperd.service"
    install -Dm644 "${srcdir}/etc/systemd/cc-plugin-coolerdash.service.d/startup-delay.conf" "${pkgdir}/etc/systemd/system/cc-plugin-coolerdash.service.d/startup-delay.conf"

    install -Dm644 "${srcdir}/LICENSE" "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
