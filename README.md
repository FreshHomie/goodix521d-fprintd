# Goodix 521d native fingerprint backend

Experimental **Fedora 44 x86_64** support for the USB fingerprint reader GOODIX
**27c6:521d**, firmware **GFUSB_GM168SEC_APP_10019**.

****TESTED ONLY ON ASUS GA503QR - FEEL FREE TO TEST FORK ETC.****

*THIS PROJECT WAS AI ASSISTED*

****This was built solely for my personal needs and is complete for that. If this driver does not work as intended on your machine or setup please feel free to use this as a baseline for further development.****

***BEFORE UNLOCKING THE DRIVER TAKES ROUGHLY 2 SECONDS ON THE FIRST UNLOCK TO CALIBRATE***

Native image processing, enrollment and matching integrated with libfprint,
fprintd and KDE/GNOME. **No Windows DLL, Wine or proprietary engine download
is required.** Other Goodix USB IDs and SPI readers are not supported.

## Install (Fedora 44)

Download the x86_64 RPM from [Releases](https://github.com/FreshHomie/goodix521d-fprintd/releases).

```sh
sudo dnf install ./goodix521d-fprintd-0.1.0-20.fc44.x86_64.rpm
sudo goodix521d-setup
```

Follow setup's clear-sensor and finger prompts to measure this reader's contact
reference. Then open KDE Users or GNOME Users and enroll your finger. Lift fully
between enrollment scans, vary placement slightly while keeping the sensor
covered, and hold still during capture.

See [INSTALL.md](INSTALL.md) for upgrades, diagnostics and removal. Existing
configured readers keep their fingerprints and calibration on upgrade.

## Arch Linux / AUR

Experimental Arch x86_64 builds and the AUR submission bundle are available in
[the Arch release](https://github.com/FreshHomie/goodix521d-fprintd/releases/tag/arch-v0.1.0-2).
The recipe is in [packaging/arch](packaging/arch). Arch package **0.1.0-2**
includes the same release19 quick-tap fix as Fedora. It has **not been submitted
to the AUR registry yet**.

Install the downloaded Arch package and measure your reader:

```sh
sudo pacman -U ./goodix521d-fprintd-0.1.0-2-x86_64.pkg.tar.zst
sudo goodix521d-setup
```

For a source build, extract the AUR source bundle and run `makepkg -si` as a
normal user with Arch's base-devel installed. See
[Arch instructions](packaging/arch/README.md). Arch setup does not edit PAM.

The package built and installed in an official Arch container and passed
152 saved-scan operations, including classified-frame retries and recovery. **Live scanner/KDE/suspend testing on Arch remains
unverified**; the Fedora hardware results above do not establish Arch behavior.

## Status

Release20 is a **pre-release awaiting live hardware confirmation**. It preserves
native preprocessing error codes and turns known rejected calibration/raw images
into lift-and-retry requests with fresh calibration, instead of disconnecting.
Matching thresholds are unchanged. Package/source audits, seven preprocessing
recovery checks, 18 replacement checks and the 62-frame quick-tap regression passed.
Arch 0.1.0-2 still contains the release19 code; it does not include this new fix.


Experimental release, tested on the development machine. It passed KDE
enrollment/unlock, wrong-finger rejection, reboot and suspend/resume tests.
Release19 fixes fingerprint authentication ending after a quick tap: unsupported
classified frames request a retry instead of disconnecting. Subsequent steady
holds were confirmed to unlock on the development machine. Matching thresholds
remain unchanged. It passed the package audit, 18 worker checks and offline
frame/recovery regression tests. Reboot and suspend were tested on earlier
versions; those lifecycle checks have not yet been repeated for release19. First-time setup on another machine has not
been verified. Keep password access available.

Placement can still cause rejection. A cold reader or resumed reader may need
a clear sensor for calibration; lift when requested and retry. Enrollment can
reject repeated areas and ask for a small shift or rotation.

## Source and builds

The buildable source is now tracked directly in this repository: native engine,
modified libfprint/fprintd, capture helpers and build scripts. See
[BUILDING.md](BUILDING.md) for Fedora/Arch dependencies and complete instructions.
No RPM extraction is required.

After installing the documented build dependencies:

```sh
git clone https://github.com/FreshHomie/goodix521d-fprintd.git
cd goodix521d-fprintd
python3 package/build-runtime.py --native --stage package/stage
```

This compiles to `package/stage/usr` without installing or changing authentication.
The guide also explains building an installable RPM directly from the Git checkout.

The release contains the source RPM and full source archive, including the
native engine, pinned upstream kernels, patched libfprint/fprintd, capture
helpers, compile recipe and build scripts. No prebuilt driver binaries are used
as source-build inputs. Install the spec's BuildRequires, then rebuild:

```sh
rpmbuild --rebuild goodix521d-fprintd-0.1.0-20.fc44.src.rpm
```

No personal fingerprints, saved scan results or reader calibration are shipped.
The source preserves the included components' license notices and copyright
attributions; see their COPYING/LICENSE files. The source manifest identifies
the included files and pinned upstream Milan revision.
