# Maintainer: Christian Kühn (damachine3 at proton dot me)
# Website: https://github.com/damachine/coolerdash

# This PKGBUILD is for building the coolerdash package from local source.
# It assumes the source code is already present in the current directory.

pkgname=coolerdash
pkgver=$(cat VERSION)
pkgrel=1
provides=('coolerdash-git')
replaces=('coolerdash-git')
conflicts=('coolerdash-git')
pkgdesc="Plug-in for CoolerControl that extends the LCD functionality with additional features"
arch=('x86_64')
url="https://github.com/damachine/coolerdash"
license=('MIT')
depends=('cairo' 'jansson' 'libcurl-gnutls' 'ttf-roboto')
makedepends=('gcc' 'make' 'pkg-config' 'git')
optdepends=()
backup=('var/lib/coolercontrol/plugins/coolerdash/config.json')
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
    rm -f "${pkgname}"-*.pkg.tar.*

    # Clean any previous builds if a Makefile exists
    if [[ -f Makefile || -f GNUmakefile ]]; then
        make clean
    fi

    # Build the project
    make

    # Copy files to srcdir for packaging (fakeroot cannot access startdir)
    mkdir -p "${srcdir}/bin" "${srcdir}/images" "${srcdir}/var/lib/coolercontrol/plugins/coolerdash/ui"
    cp -a bin/coolerdash "${srcdir}/bin/coolerdash"
    cp -a README.md CHANGELOG.md VERSION LICENSE "${srcdir}/"
    cp -a etc/coolercontrol/plugins/coolerdash/config.json "${srcdir}/var/lib/coolercontrol/plugins/coolerdash/"
    cp -a etc/coolercontrol/plugins/coolerdash/ui/index.html "${srcdir}/var/lib/coolercontrol/plugins/coolerdash/ui/"
    cp -a images/shutdown.png "${srcdir}/images/"
    cp -a etc/coolercontrol/plugins/coolerdash/manifest.toml "${srcdir}/var/lib/coolercontrol/plugins/coolerdash/"
}

package() {
    # Binary to /usr/libexec, plugin data to /var/lib/coolercontrol/plugins/
    install -dm711 "${pkgdir}/var/lib/coolercontrol"
    install -dm755 "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash"
    install -Dm755 "${srcdir}/bin/coolerdash" "${pkgdir}/usr/libexec/coolerdash/coolerdash"
    install -Dm644 "${srcdir}/README.md" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/README.md"
    install -Dm644 "${srcdir}/VERSION" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/VERSION"
    install -Dm644 "${srcdir}/CHANGELOG.md" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/CHANGELOG.md"
    install -Dm600 "${srcdir}/var/lib/coolercontrol/plugins/coolerdash/config.json" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/config.json"

    install -dm755 "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/ui"
    install -m644 "${srcdir}/var/lib/coolercontrol/plugins/coolerdash/ui/index.html" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/ui/index.html"
    install -Dm644 "${srcdir}/images/shutdown.png" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/shutdown.png"
    install -Dm644 "${srcdir}/var/lib/coolercontrol/plugins/coolerdash/manifest.toml" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/manifest.toml"

    sed -i "s/{{VERSION}}/${pkgver}/g" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/manifest.toml"
    sed -i "s/{{VERSION}}/${pkgver}/g" "${pkgdir}/var/lib/coolercontrol/plugins/coolerdash/ui/index.html"

    install -Dm644 "${srcdir}/LICENSE" "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
