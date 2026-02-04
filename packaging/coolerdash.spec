%global debug_package %{nil}
%undefine _debugsource_template

Name:           coolerdash
Version:        %{getenv:COOLERDASH_VERSION}
Release:        1%{?dist}
Summary:        LCD temperature display daemon for AIO liquid coolers
License:        MIT
URL:            https://github.com/damachine/coolerdash
Source0:        %{name}-%{version}.tar.gz

# Build dependencies installed via CI workflow
# to support multiple distributions with different package names

Requires:       cairo
Requires:       libcurl
Requires:       jansson
Requires:       google-roboto-fonts
Requires:       coolercontrol

%description
CoolerDash is a high-performance C99 daemon that displays CPU and GPU
temperatures on LCD displays of AIO liquid coolers (NZXT Kraken, etc.)
via the CoolerControl REST API.

Features:
 * Dual mode: CPU+GPU simultaneously
 * Circle mode: Alternating display
 * Real-time temperature monitoring
 * Automatic scaling and color-coded warnings
 * CoolerControl plugin integration

%prep
%autosetup -n %{name}-%{version}

%build
make SUDO="" REALOS=no %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} SUDO="" REALOS=no
# Compress manpage for RPM standards
gzip -9 %{buildroot}/usr/share/man/man1/coolerdash.1

%post
# Set correct permissions for config file
if [ -f /etc/coolercontrol/plugins/coolerdash/config.json ]; then
    chmod 666 /etc/coolercontrol/plugins/coolerdash/config.json
fi

# Restart CoolerControl to register new plugin
if command -v systemctl >/dev/null 2>&1; then
    if systemctl is-active --quiet coolercontrold.service 2>/dev/null; then
        systemctl restart coolercontrold.service || true
    fi
fi

%preun
# Stop CoolerControl to ensure clean plugin removal
if command -v systemctl >/dev/null 2>&1; then
    if systemctl is-active --quiet coolercontrold.service 2>/dev/null; then
        systemctl stop coolercontrold.service || true
    fi
fi

%files
%license LICENSE
%doc README.md CHANGELOG.md
/etc/coolercontrol/plugins/coolerdash/coolerdash
/etc/coolercontrol/plugins/coolerdash/config.json
/etc/coolercontrol/plugins/coolerdash/manifest.toml
/etc/coolercontrol/plugins/coolerdash/ui/index.html
/etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js
/etc/coolercontrol/plugins/coolerdash/shutdown.png
/etc/coolercontrol/plugins/coolerdash/README.md
/etc/coolercontrol/plugins/coolerdash/LICENSE
/etc/coolercontrol/plugins/coolerdash/CHANGELOG.md
/etc/coolercontrol/plugins/coolerdash/VERSION
/usr/share/man/man1/coolerdash.1.gz
/usr/share/applications/coolerdash.desktop
/usr/share/icons/hicolor/scalable/apps/coolerdash.svg
/usr/lib/udev/rules.d/99-coolerdash.rules

%changelog
* %(date "+%a %b %d %Y") damachine <damachin3@proton.me> - %{version}-1
- Automated release via GitHub Actions
