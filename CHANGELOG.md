# Changelog

All notable changes to this project are documented directly in the Git commit history.

For a detailed list of all changes, see:

```bash
git log --oneline --decorate --graph --all
```

or visit the repository online:

https://github.com/damachine/coolerdash/commits/main

> This file is kept for compatibility. All release notes and technical details are tracked via Git commits.

## [1.99] - Unreleased

### Added
- New config: `display_inscribe_factor` to control safe area for circular displays (0.0 = auto, >=0 and <=1.0). Default: `0.70710678` (1/√2). (dual/circle renderers)
- INI parser handler for `inscribe_factor` (`[display] inscribe_factor`) with validation and logging.
- Test harness: `tests/test_scaling.c` that validates `safe_area` and `safe_bar` calculations for auto/explicit/custom inscribe factors and rectangular overrides.
- Documentation updates: `etc/coolerdash/config.ini`, `docs/config-guide.md`, `docs/display-modes.md`, `docs/display-detection.md`, `docs/developer-guide.md` reflect the new option and test instructions.

### Changed
- `calculate_scaling_params()` in `dual.c` and `circle.c` now uses `display_inscribe_factor` if set; otherwise auto fallback (1/√2).

