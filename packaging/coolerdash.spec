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
BuildRequires:  pkgconfig(fontconfig)
BuildRequires:  pkgconfig(gdk-pixbuf-2.0)
BuildRequires:  pkgconfig(jansson)
BuildRequires:  pkgconfig(libcurl)

# Shared lib deps are auto-detected by rpmbuild
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
if [ -f /var/lib/coolercontrol/plugins/coolerdash/config.json ]; then
    chmod 600 /var/lib/coolercontrol/plugins/coolerdash/config.json
fi
if [ -f /var/lib/coolercontrol/plugins/coolerdash/credentials.json ]; then
    chmod 600 /var/lib/coolercontrol/plugins/coolerdash/credentials.json
fi
if command -v systemctl >/dev/null 2>&1; then
    if systemctl is-active --quiet coolercontrold.service; then
        systemctl restart coolercontrold.service || echo "Note: CoolerControl restart failed."
    fi
elif command -v rc-service >/dev/null 2>&1 && [ -x /etc/init.d/coolercontrold ]; then
    if rc-service coolercontrold status >/dev/null 2>&1; then
        rc-service coolercontrold restart || echo "Note: CoolerControl restart failed."
    fi
fi

%postun
if [ "$1" -eq 0 ]; then
    if command -v systemctl >/dev/null 2>&1; then
        if systemctl is-active --quiet coolercontrold.service; then
            systemctl restart coolercontrold.service || echo "Note: CoolerControl restart failed."
        fi
    elif command -v rc-service >/dev/null 2>&1 && [ -x /etc/init.d/coolercontrold ]; then
        if rc-service coolercontrold status >/dev/null 2>&1; then
            rc-service coolercontrold restart || echo "Note: CoolerControl restart failed."
        fi
    fi
fi

%files
%doc README.md CHANGELOG.md
%dir /usr/share/licenses/%{name}
%license /usr/share/licenses/%{name}/LICENSE
%dir /usr/libexec/coolerdash
/usr/libexec/coolerdash/coolerdash
/usr/bin/coolerdash
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
* %(date "+%a %b %d %Y") Christian Kühn <damachin3@proton.me> - %{version}-1
- Automated release via GitHub Actions
