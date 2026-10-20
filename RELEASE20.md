## Preprocessing retry recovery — Fedora 44 x86_64

**Pre-release: offline validation passed; live hardware testing of release20 is pending.**

Known native blank-background and raw-image rejections now request a lift, discard cached calibration, and retry with fresh calibration instead of disconnecting the fingerprint controller. Initialization diagnostics retain the native stage and status code. Unexpected internal errors, malformed inputs and engine-identity failures remain errors.

Matching thresholds, stored templates and the release19 quick-tap recovery are unchanged. Existing configured users do not need to re-enroll or repeat setup solely for this upgrade.

### Validation
- Binary/source RPM audit passed; all seven ELF components rebuilt from source.
- Seven packaged preprocessing recovery checks passed, including successful matching after a rejected input in the same worker/controller session.
- 18 native replacement checks passed, including enrollment, template persistence, matching and wrong-finger rejection.
- 62-frame quick-tap regression passed.

### Install / upgrade
Download the x86_64 RPM and follow the attached INSTALL.md. New users run `sudo goodix521d-setup` after installation; existing configured users retain enrollment and calibration.

Source RPM and full source archive are included. No Windows DLL or personal fingerprint scans/templates are shipped.

Arch package 0.1.0-2 still uses release19 code and does not include this change. Keep password access available.
