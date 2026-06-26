%global debug_package %{nil}
%undefine _debugsource_template

Name:           coolerdash
Version:        %{getenv:COOLERDASH_VERSION}
Release:        1%{?dist}
Summary:        LCD temperature display daemon for AIO liquid coolers
License:        MIT
URL:            https://github.com/damachine/coolerdash
Source0:        %{name}-%{version}.tar.gz

# pkgconfig() resolves to correct -devel packages on Fedora and openSUSE
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(jansson)
BuildRequires:  pkgconfig(libcurl)

# Shared lib deps (libcairo, libjansson, libcurl) are auto-detected by rpmbuild
Requires:       google-roboto-fonts
Recommends:     coolercontrol

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

%post
if [ ! -f /var/lib/coolercontrol/plugins/coolerdash/credentials.json ]; then
    mkdir -p /var/lib/coolercontrol/plugins/coolerdash
    printf '{\n  "access_token": ""\n}\n' > /var/lib/coolercontrol/plugins/coolerdash/credentials.json
fi
mkdir -p /etc/coolercontrol
if [ -f /var/lib/coolercontrol/plugins/coolerdash/coolerdash.png ]; then
    if [ -L /etc/coolercontrol/lcd_image.png ] || [ ! -e /etc/coolercontrol/lcd_image.png ]; then
        ln -sfn /var/lib/coolercontrol/plugins/coolerdash/coolerdash.png /etc/coolercontrol/lcd_image.png
    fi
fi
if [ -f /var/lib/coolercontrol/plugins/coolerdash/config.json ]; then
    chmod 600 /var/lib/coolercontrol/plugins/coolerdash/config.json
fi
if [ -f /var/lib/coolercontrol/plugins/coolerdash/credentials.json ]; then
    chmod 600 /var/lib/coolercontrol/plugins/coolerdash/credentials.json
fi
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    systemctl reset-failed cc-plugin-coolerdash.service >/dev/null 2>&1 || true
    if systemctl list-unit-files cc-plugin-coolerdash.service | grep -q cc-plugin-coolerdash; then
        systemctl restart cc-plugin-coolerdash.service || echo "Note: Plugin restart failed."
    fi
    if systemctl is-active --quiet coolercontrold.service; then
        systemctl restart coolercontrold.service || echo "Note: CoolerControl restart failed."
    fi
elif command -v rc-service >/dev/null 2>&1 && [ -x /etc/init.d/coolercontrold ]; then
    rc-service coolercontrold restart || echo "Note: CoolerControl restart failed."
fi

%preun
if command -v systemctl >/dev/null 2>&1; then
    if systemctl list-unit-files cc-plugin-coolerdash.service | grep -q cc-plugin-coolerdash; then
        systemctl stop cc-plugin-coolerdash.service
    fi
elif command -v rc-service >/dev/null 2>&1 && [ -x /etc/init.d/cc-plugin-coolerdash ]; then
    rc-service cc-plugin-coolerdash stop || true
fi

%postun
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    if systemctl is-active --quiet coolercontrold.service; then
        systemctl restart coolercontrold.service || echo "Note: CoolerControl restart failed."
    fi
elif command -v rc-service >/dev/null 2>&1 && [ -x /etc/init.d/coolercontrold ]; then
    rc-service coolercontrold restart || echo "Note: CoolerControl restart failed."
fi

%files
%doc README.md CHANGELOG.md
%dir /usr/share/licenses/%{name}
%license /usr/share/licenses/%{name}/LICENSE
%dir /usr/libexec/coolerdash
/usr/libexec/coolerdash/coolerdash
%dir /var/lib/coolercontrol/plugins/coolerdash
%config(noreplace) /var/lib/coolercontrol/plugins/coolerdash/config.json
/var/lib/coolercontrol/plugins/coolerdash/manifest.toml
%dir /var/lib/coolercontrol/plugins/coolerdash/ui
/var/lib/coolercontrol/plugins/coolerdash/ui/index.html
/var/lib/coolercontrol/plugins/coolerdash/shutdown.png
/var/lib/coolercontrol/plugins/coolerdash/README.md
/var/lib/coolercontrol/plugins/coolerdash/CHANGELOG.md
/var/lib/coolercontrol/plugins/coolerdash/VERSION

%changelog
* %(date "+%a %b %d %Y") damachine <damachin3@proton.me> - %{version}-1
- Automated release via GitHub Actions
