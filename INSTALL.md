# Goodix 521d native backend — Fedora 44 x86_64

Experimental support for USB **27c6:521d**, firmware
**GFUSB_GM168SEC_APP_10019**. Other Goodix USB IDs and SPI readers are not
supported by this package. No Windows DLL, Wine or driver download is required.

## New installation

Download the x86_64 RPM and install it:

```sh
sudo dnf install ./goodix521d-fprintd-0.1.0-18.fc44.x86_64.rpm
sudo goodix521d-setup
```

Setup measures this reader's contact reference using clear-sensor and finger
prompts, then enables fingerprint authentication through Fedora's authselect.
Keep the sensor clear whenever prompted. You can also launch **Goodix
Fingerprint Setup** from the application menu. Use `--no-authentication` to
leave authselect unchanged. Open KDE Users or GNOME Users and enroll your finger
when setup finishes. Lift fully between scans and vary placement slightly while
maintaining contact over the sensor; hold still during each scan.

## Upgrade from release17

Close fingerprint settings and finish any active scan. Existing fingerprints
and reader calibration are retained. Do not run a diagnostic that masks fprintd
at the same time.

```sh
sudo systemctl stop fprintd.service goodix521d-worker.service goodix521d-worker.socket
sudo dnf upgrade ./goodix521d-fprintd-0.1.0-18.fc44.x86_64.rpm
sudo systemctl daemon-reload
sudo systemctl enable --now goodix521d-worker.socket
sudo systemctl start goodix521d-worker.service fprintd.service
```

An existing configured reader does not need a new guided measurement or
re-enrollment solely for this upgrade. First-time users must run setup.

## Known limitations and diagnostics

This is an experimental release tested on the development machine. Setup on
another machine has not been verified. Placement can cause rejection. On cold
startup or wake the sensor may need to be clear for calibration before a scan.
Lift when asked and retry. Enrollment can reject repeated areas and ask for a
slight placement change. Password authentication remains available.

```sh
sudo goodix521d-setup --doctor
journalctl -u fprintd.service -u goodix521d-worker.service -b
```

To undo a package update, retain the previous RPM and use `sudo dnf downgrade
./previous-package.rpm` with capture/worker services stopped, then restart them.
To uninstall, remove `goodix521d-fprintd` with DNF. The system libfprint and
fprintd packages remain installed. Package removal retains local fingerprint
and reader data and does not change PAM/authselect configuration; use Fedora's
normal authentication settings if you want to disable fingerprint login.

## Source build

The source RPM includes the capture and private fprintd source, local native
engine source and pinned upstream Milan kernels, relocatable compile recipe,
RPM spec and build scripts. No prebuilt driver/helper binaries or personal
biometrics are required as build inputs. Install the spec's BuildRequires,
then run `rpmbuild --rebuild goodix521d-fprintd-0.1.0-18.fc44.src.rpm`.
The package does not replace the system libfprint files; its private libraries
are selected only for fprintd through the installed service drop-in.
