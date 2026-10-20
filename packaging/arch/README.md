# Goodix 521d for Arch Linux / AUR

Experimental **x86_64** support for USB **27c6:521d**, firmware
**GFUSB_GM168SEC_APP_10019**. Builds the native matching engine, capture helpers,
private libfprint libraries and private fprintd daemon from the pinned release20
source archive. No Windows DLL, Wine, RPM extraction or Fedora binaries are used.
This Arch port requires separate hardware testing; Fedora results do not certify
Arch/KDE/GNOME behavior. Other USB IDs and SPI models are unsupported.

## Build and install

Install Arch's base-devel and git packages first. Download this AUR recipe, review
its files, then run as a normal user:

```sh
makepkg -si
sudo goodix521d-setup
```

Setup measures your reader's clear/contact reference. Follow its lift/press prompts,
then enroll through KDE Users, GNOME Users or `fprintd-enroll`. Each reader needs
its own measurement. No personal scans or calibration are shipped. Keep password
access available. Placement and cold/resume calibration may require retries.

## Authentication on Arch

This package keeps the repository's fprintd and libfprint installed. It selects
private binaries under `/usr/lib/goodix521d` through a fprintd service drop-in.
The fprintd package supplies D-Bus policy, service definition, tools and
`pam_fprintd.so`; this package does not overwrite them.

Arch does not use Fedora's authselect. Neither installation nor setup edits
`/etc/pam.d`. Configure fingerprint authentication for your login/lock screen
using the ArchWiki guide and your display manager's instructions:
https://wiki.archlinux.org/title/Fingerprint_authentication
Enrollment alone does not configure every PAM service automatically.

Version0.1.0-20 includes the release20 quick-tap fix: unsupported classified
frames request retries instead of disconnecting fingerprint authentication.
Known rejected background/raw images now request a lift and fresh calibration instead of disconnecting. Offline tests passed; live hardware testing of release20 is pending. Matching thresholds are unchanged. Existing fingerprints and calibration are
retained; this upgrade does not require re-enrollment.

## Upgrade, diagnostics, removal

Finish active scans before upgrading. Restart services after an upgrade:

```sh
sudo systemctl restart goodix521d-worker.service fprintd.service
sudo goodix521d-setup --doctor
journalctl -u fprintd.service -u goodix521d-worker.service -b
```

Remove with `sudo pacman -R goodix521d-fprintd`. Package removal restores the
repository fprintd executable selection and retains fingerprint/contact state.
PAM files are not changed on removal; undo your own PAM changes separately if
needed. This package is for normal Arch installations; SELinux-enabled custom
Arch configurations need an appropriate local policy.

## AUR submission

Upload only `PKGBUILD`, `.SRCINFO`, `arch-prepare.py`, `README.md` and
`goodix521d-fprintd.install` to the AUR Git repository. Do not commit built
packages, source tarballs, `src/` or `pkg/`. Regenerate metadata after edits:

```sh
makepkg --printsrcinfo > .SRCINFO
makepkg --source
```

Arch package version0.1.0-20 uses upstream release20. The source archive and local
adaptation files have SHA256 pins. `!strip` preserves the native library hash
that the worker checks at runtime; the upstream builder strips its debug data
before generating this hash.
