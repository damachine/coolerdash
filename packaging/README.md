# Packaging Structure

This directory contains package configuration files for multiple Linux distributions.

## Structure

```
packaging/
├── debian/           # Debian/Ubuntu DEB package configuration
│   ├── control      # Package metadata and dependencies
│   ├── rules        # Build instructions
│   ├── changelog    # Package changelog template
│   ├── compat       # Debhelper compatibility level
│   ├── install      # File installation mappings
│   ├── postinst     # Post-installation script
│   ├── prerm        # Pre-removal script
│   └── source/
│       └── format   # Source package format
│
└── coolerdash.spec  # RPM package specification (Fedora/RHEL/openSUSE)
```

## Usage

These files are automatically used by the GitHub Actions workflow (`.github/workflows/release.yml`) to build binary packages for each distribution.

- **DEB packages**: Built using `dpkg-buildpackage`
- **RPM packages**: Built using `rpmbuild`

## Notes

- All files must be committed to Git (required by CI/CD workflow)
- Do NOT add `packaging/` to `.gitignore`
- Only build artifacts (*.deb, *.rpm) are ignored via `.gitignore`
