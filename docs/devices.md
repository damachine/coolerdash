## Supported Devices

If CoolerDash works (or not) on your device, submit a [Device Confirmation Issue](https://github.com/damachine/coolerdash/issues/new?template=device-confirmation.yml).

Before opening the issue, collect the technical device data:

~~~bash
coolerdash --hardware-report
~~~

This creates a sanitized JSON report and Markdown summary in your home
directory. Review the files, attach the JSON report, and paste the Markdown
summary into the issue. CoolerDash does not upload either file automatically.

For an explicitly confirmed display test, first stop or disable any running
CoolerDash plugin instance and run:

~~~bash
coolerdash --hardware-report --test-lcd
~~~

## Confirmed Devices

| Manufacturer | Model | Status | Tester | Date |
|-------------|-------|--------|--------|------|
| NZXT | Kraken 2023 | ✅ Working | @damachine | 2025-06-08 |
| NZXT | Kraken 2023 | ✅ Working | @Kimloc (discord) | 2025-08-27 |
| NZXT | Kraken 2023 | ✅ Working | @olivetti80 | 2025-09-12 |
| NZXT | Kraken 2023 Elite | ⚠️ Partially | @Mondkeks | 2025-10-09 |
| NZXT | Kraken Z63 | ✅ Working | @SSUPD-Beast | 2025-11-24 |
| NZXT | Kraken Plus 240 | ✅ Working | reported (discord) | 2026-08-03 |

### Legend

- ✅ Working
- ⚠️ Partially working
- ❌ Not working
