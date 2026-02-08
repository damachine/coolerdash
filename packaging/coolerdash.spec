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
gzip -9 %{buildroot}/usr/share/man/man1/coolerdash.1

%post
if [ -f /etc/coolercontrol/plugins/coolerdash/config.json ]; then
    chmod 666 /etc/coolercontrol/plugins/coolerdash/config.json
fi
# Migration: remove old helperd from /etc (now shipped in /usr/lib)
rm -f /etc/systemd/system/coolerdash-helperd.service
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    systemctl enable --now coolerdash-helperd.service 2>/dev/null || true
    if systemctl is-active --quiet coolercontrold.service 2>/dev/null; then
        systemctl restart coolercontrold.service || true
    fi
fi

%preun
if command -v systemctl >/dev/null 2>&1; then
    systemctl stop --no-block coolerdash-helperd.service 2>/dev/null || true
    systemctl disable coolerdash-helperd.service 2>/dev/null || true
    systemctl stop cc-plugin-coolerdash.service 2>/dev/null || true
    if systemctl is-active --quiet coolercontrold.service 2>/dev/null; then
        systemctl stop coolercontrold.service || true
    fi
fi

%postun
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    systemctl restart coolercontrold.service 2>/dev/null || true
fi

%files
%doc README.md CHANGELOG.md
%dir /usr/share/licenses/%{name}
%license /usr/share/licenses/%{name}/LICENSE
%dir /usr/libexec/coolerdash
/usr/libexec/coolerdash/coolerdash
%dir /etc/coolercontrol/plugins/coolerdash
%config(noreplace) /etc/coolercontrol/plugins/coolerdash/config.json
/etc/coolercontrol/plugins/coolerdash/manifest.toml
%dir /etc/coolercontrol/plugins/coolerdash/ui
/etc/coolercontrol/plugins/coolerdash/ui/index.html
/etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js
/etc/coolercontrol/plugins/coolerdash/shutdown.png
/etc/coolercontrol/plugins/coolerdash/README.md
/etc/coolercontrol/plugins/coolerdash/CHANGELOG.md
/etc/coolercontrol/plugins/coolerdash/VERSION
/usr/share/man/man1/coolerdash.1.gz
/usr/share/applications/coolerdash.desktop
/usr/share/icons/hicolor/scalable/apps/coolerdash.svg
/usr/lib/udev/rules.d/99-coolerdash.rules
/usr/lib/systemd/system/coolerdash-helperd.service
%dir /etc/systemd/system/cc-plugin-coolerdash.service.d
/etc/systemd/system/cc-plugin-coolerdash.service.d/startup-delay.conf

%changelog
* %(date "+%a %b %d %Y") damachine <damachin3@proton.me> - %{version}-1
- Automated release via GitHub Actions
