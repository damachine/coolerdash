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

%pre
# Stop legacy service
if command -v systemctl >/dev/null 2>&1; then
    if systemctl list-unit-files coolerdash.service | grep -q coolerdash; then
        systemctl stop coolerdash.service
        systemctl disable coolerdash.service
    fi
fi

# Remove legacy files
rm -f /etc/systemd/system/coolerdash.service
rm -f /etc/systemd/system/coolerdash-helperd.service
rm -f /etc/systemd/system/multi-user.target.wants/coolerdash-helperd.service
rm -rf /etc/systemd/system/cc-plugin-coolerdash.service.d
rm -f /usr/lib/systemd/system/coolerdash-helperd.service
rm -f /usr/lib/udev/rules.d/99-coolerdash.rules
rm -rf /opt/coolerdash
rm -rf /etc/coolerdash
rm -f /etc/coolercontrol/plugins/coolerdash/config.ini
rm -f /etc/coolercontrol/plugins/coolerdash/ui.html
rm -f /etc/coolercontrol/plugins/coolerdash/LICENSE
rm -f /etc/coolercontrol/plugins/coolerdash/coolerdash
rm -f /usr/share/applications/coolerdash-settings.desktop
rm -f /bin/coolerdash
rm -f /usr/bin/coolerdash

# Remove legacy user
if id -u coolerdash >/dev/null 2>&1; then
    userdel -rf coolerdash
fi

%post
if [ -f /etc/coolercontrol/plugins/coolerdash/config.json ]; then
    chmod 666 /etc/coolercontrol/plugins/coolerdash/config.json
fi
# Migrate helperd from /etc to /usr/lib
rm -f /etc/systemd/system/multi-user.target.wants/coolerdash-helperd.service
rm -f /etc/systemd/system/coolerdash-helperd.service
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    if systemctl list-unit-files coolerdash-helperd.service | grep -q coolerdash-helperd; then
        systemctl enable --now coolerdash-helperd.service || echo "Note: coolerdash-helperd.service failed. Enable manually."
    fi
    if systemctl is-active --quiet coolercontrold.service; then
        systemctl restart coolercontrold.service || echo "Note: CoolerControl restart failed."
    fi
fi

%preun
if command -v systemctl >/dev/null 2>&1; then
    if systemctl list-unit-files coolerdash-helperd.service | grep -q coolerdash-helperd; then
        systemctl stop --no-block coolerdash-helperd.service
        systemctl disable coolerdash-helperd.service
    fi
    if systemctl list-unit-files cc-plugin-coolerdash.service | grep -q cc-plugin-coolerdash; then
        systemctl stop cc-plugin-coolerdash.service
        systemctl disable cc-plugin-coolerdash.service
    fi
    if systemctl is-active --quiet coolercontrold.service; then
        systemctl stop coolercontrold.service
    fi
fi

# Remove legacy files
if [ "$1" = "0" ]; then
    rm -f /etc/systemd/system/coolerdash-helperd.service
    rm -f /etc/systemd/system/coolerdash.service
    rm -f /etc/systemd/system/multi-user.target.wants/coolerdash-helperd.service
    rm -rf /etc/systemd/system/cc-plugin-coolerdash.service.d
    rm -f /etc/coolercontrol/plugins/coolerdash/config.ini
    rm -f /etc/coolercontrol/plugins/coolerdash/ui.html
    rm -f /etc/coolercontrol/plugins/coolerdash/LICENSE
    rm -f /etc/coolercontrol/plugins/coolerdash/coolerdash
    rm -f /usr/share/applications/coolerdash-settings.desktop
    rm -f /bin/coolerdash
    rm -f /usr/bin/coolerdash
    rm -rf /opt/coolerdash
    rm -rf /etc/coolerdash
    # Remove legacy user
    if id -u coolerdash >/dev/null 2>&1; then
        userdel -rf coolerdash
    fi
fi

%postun
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload
    if systemctl is-active --quiet coolercontrold.service; then
        systemctl restart coolercontrold.service || echo "Note: CoolerControl restart failed."
    fi
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
