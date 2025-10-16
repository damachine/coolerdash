# 📘 Project Best Practices

## 1. Project Purpose
CoolerDash is a C99 Linux daemon that renders CPU and GPU temperatures to the LCD screen of AIO coolers (e.g., NZXT) by talking to the CoolerControl daemon via HTTP. It generates a PNG with Cairo, caches device info for performance, and uploads images to the device with libcurl.

## 2. Project Structure
- Root
  - Makefile: Build, install, uninstall, debug, and distro-aware dependencies.
  - etc/systemd/coolerdash.service: Systemd unit for running as a service under user coolerdash.
  - docs/: Configuration and device support documentation.
  - man/coolerdash.1: Manual page.
  - README.md, SECURITY.md, CONTRIBUTING.md, CODE_OF_CONDUCT.md, CHANGELOG.md, LICENSE.
  - openapi-spec.json: CoolerControl API reference.
  - VERSION: Program version string.
  - PKGBUILD, .SRCINFO, .github/: AUR packaging and CI.
- src/ (modular separation of concerns)
  - main.c: Entry point, lifecycle, signal handling, PID file, loop timing.
  - config.h/c: INI parsing, defaults, logging API, configuration schema, color model.
  - coolercontrol.h/c: REST session, login, device cache, LCD upload, API helpers.
  - monitor.h/c: Temperature acquisition from CoolerControl status endpoint.
  - display.h/c: Cairo rendering pipeline and PNG generation; triggers LCD upload.

Key roles
- main: Orchestration, lifecycle, single-instance enforcement, graceful shutdown with device update.
- config: Strong defaults, safe parsing, minimal validation, and central logging API.
- monitor: Reads temps from CoolerControl /status with robust JSON parsing (jansson).
- coolercontrol: Handles HTTP login, device discovery (/devices), LCD upload, device info caching.
- display: Renders PNG image and pushes to LCD using brightness/orientation from config.

Entry points and config
- Binary: /usr/bin/coolerdash (symlink to /opt/coolerdash/bin/coolerdash)
- Config: /etc/coolerdash/config.ini
- Runtime files: /tmp/coolerdash.pid (PID), /tmp/coolerdash.png (generated image)
- Systemd: etc/systemd/coolerdash.service (ExecStart=/usr/bin/coolerdash)

## 3. Test Strategy
Current state: No test suite in the repository.

Recommendations
- Frameworks
  - Unit tests: Criterion or CMocka for pure C99 and easy assertions.
  - JSON parsing tests: Feed jansson with fixtures to validate parsing logic.
  - Integration tests: Bash scripts + jq + curl to stand up expected HTTP responses or hit a real CoolerControl instance (guarded by env flags).
- Structure
  - tests/unit/: Isolated modules (config, safe parsers, response buffer management).
  - tests/integration/: End-to-end flows (device cache init, end-to-end render + mock upload).
  - tests/fixtures/: JSON samples (devices/status), INI variants, images.
- Mocking
  - Mock HTTP with a tiny local server (e.g., Python http.server) or pre-recorded fixtures and intercepted write_callback usage.
  - Mock file system paths for PID/image in a temporary directory during tests.
- Philosophy
  - Unit tests for deterministic logic: safe_atoi/safe_atof, color parsing, bar color selection, fill width, logging prefixes.
  - Integration tests for: device cache init, full render cycle, handling missing devices, and LCD upload error paths.
  - Coverage: Aim for high coverage in parsing/formatting and error/edge-case handling (e.g., invalid JSON, empty INI, long URLs).
- CI
  - Add test target to Makefile and run on CI, include AddressSanitizer in debug builds for memory safety checks.

## 4. Code Style
Language and build
- C99 with -std=c99 and -march=x86-64-v3.
- Prefer static, static inline, and internal linkage where possible.
- Use AddressSanitizer (-fsanitize=address) in debug builds.

Naming and structure
- snake_case for functions and variables; PascalCase is not used.
- Header guards for all headers.
- Module prefixes for cohesion: cc_* for CoolerControl helpers; monitor_* for monitor; render/draw naming in display; safe_* parsing helpers.
- Keep prototypes in headers synchronized with implementations.

Logging and verbosity
- Use log_message(level, ...) uniformly across modules.
  - Levels: LOG_INFO (hidden unless verbose), LOG_STATUS (key state), LOG_WARNING, LOG_ERROR.
- Avoid direct printf outside of controlled help/usage output in main.

Error handling
- Favor early returns on invalid inputs or failed preconditions.
- Standardize return conventions:
  - int functions typically return 1 for success, 0 for failure (e.g., network ops, parsing).
  - Use errno, CURLcode, and HTTP status codes to enrich error logs.
- Clean up resources on every error path (buffers, CURL handles, MIME forms, file descriptors).

Safety and parsing
- String copying: cc_safe_strcpy(dest, dest_size, src) with bounds checking.
- Numeric parsing: safe_atoi/safe_atof equivalents used to validate and clamp values.
- Color parsing: clamp to [0, 255].
- Memory: cc_secure_malloc for zero-initialized allocations; cc_init_response_buffer/cc_cleanup_response_buffer for HTTP response lifecycle.
- JSON parsing: jansson with robust type checks and fallback defaults.

Signals and lifecycle
- Use only async-signal-safe functions inside signal handlers.
- Maintain volatile sig_atomic_t flags (running) for loop control.
- On shutdown, attempt to send shutdown image or switch off LCD via brightness=0 strategy; clean PID/image files.
- If using pthread APIs (e.g., pthread_sigmask), ensure correct headers and link flags (-pthread) are added.

Configuration evolution
- When adding config fields:
  - Expand struct Config in config.h.
  - Update parsing in config.c (lookup tables).
  - Set defaults in get_config_defaults or section-specific helpers.
  - Update docs/config.md and examples.
  - Validate and clamp values where appropriate.

## 5. Common Patterns
- Single-instance management via PID file in /tmp with atomic write and stale PID cleanup.
- Device info caching
  - One-time fetch from /devices stored in a static cache for UID, device name, and LCD dimensions; avoids repeated API calls.
- Render pipeline
  - Collect temps (monitor) → render PNG (display, Cairo) → upload to LCD (coolercontrol).
  - Double LCD upload for reliability.
- Timing
  - Absolute sleeps via clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME) to achieve precise intervals.
- Defensive coding
  - Size-limited buffers for URLs, strings, and POST fields.
  - Input validation across all APIs.
  - Fallback defaults when config or API data is missing.
- REST usage
  - Login with Basic Auth, cookie jar in /tmp, SSL verification when using HTTPS.
  - Minimal headers set and cleared between requests.

## 6. Do's and Don'ts
- Do
  - Keep modules focused: config parsing, monitoring, coolercontrol I/O, rendering, and orchestration.
  - Use log_message for any diagnostic output; prefer LOG_STATUS for milestone events and LOG_INFO only when verbose_logging is enabled.
  - Validate all external inputs (INI, JSON, environment-driven strings).
  - Bound every allocation and copy operation; always check for truncation and errors.
  - Apply safe defaults for missing configuration and invalid device info.
  - Clean up CURL resources, MIME forms, and response buffers deterministically.
  - Respect async-signal-safety in signal handlers; only call allowed functions.
  - Keep header declarations in sync with implementations and enforce a single source of truth for constants.
- Don’t
  - Don’t print directly to stdout/stderr for diagnostics; route through log_message.
  - Don’t assume device presence or API success; handle missing devices, timeouts, and invalid JSON gracefully.
  - Don’t block in signal handlers or perform complex logic during shutdown.
  - Don’t introduce global state without encapsulation; prefer static module scope and clear lifecycles.
  - Don’t bypass bounds checks for strings, URLs, or buffers.
  - Don’t hardcode distro-specific paths in code; keep them in config or Makefile install rules.

## 7. Tools & Dependencies
- Languages and libs
  - C99
  - cairo: Rendering PNGs (display pipeline)
  - libcurl: HTTP client (login, devices, status, uploads)
  - jansson: JSON parsing for device/status data
  - inih: INI config parsing
  - Fonts: ttf-roboto (default font used in examples)
- Build
  - make (targets: make, make clean, make install, make uninstall, make debug, make help)
  - Debugging: AddressSanitizer enabled in debug builds.
- Install (All distros)
  - make install (handles distro detection, dependencies, systemd, and file placement)
  - Systemd service: coolerdash.service (runs as user coolerdash)
  - Logs: journalctl -xeu coolerdash.service -f
- Arch Linux
  - AUR packaging available; see PKGBUILD and .SRCINFO.

## 8. Other Notes
- Orientation
  - Code currently validates orientation as 0, 90, or 180 degrees. Do not assume 270 is supported unless code is updated accordingly.
- LCD upload reliability
  - Image sent twice intentionally; keep this pattern unless proven unnecessary across devices.
- Rendering constraints
  - Labels are suppressed when temperatures are ≥ 99°C to minimize overlapping content.
  - Ensure bar widths/heights and gaps fit the configured display dimensions.
- Versioning
  - VERSION file is read from repo or /opt/coolerdash/VERSION post-install; keep it updated for user-facing diagnostics.
- LLM guidance for new code
  - Add new features as separate modules with .h/.c pairs; expose only necessary APIs.
  - Conform to Config-driven behavior; never hardcode device or path constants.
  - Mirror existing return conventions and logging patterns.
  - When adding HTTP operations, reuse response buffer utilities and practice strict error handling and cleanup.
  - Maintain C99 portability; guard non-portable features and ensure headers/link flags match APIs (e.g., -pthread if using pthread_sigmask).