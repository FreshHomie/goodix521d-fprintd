# Build directly from the Git source

The repository contains the native engine, the modified libfprint and fprintd
source, USB capture helpers and build scripts. No RPM, Windows DLL or Wine is
needed as a source input. The hardware target remains USB **27c6:521d**, firmware
**GFUSB_GM168SEC_APP_10019**, x86_64.

## Clone

```sh
git clone https://github.com/FreshHomie/goodix521d-fprintd.git
cd goodix521d-fprintd
```

## Fedora: compile the checkout

Install build dependencies from Fedora repositories:

```sh
sudo dnf install gcc gcc-c++ meson ninja-build pkgconf-pkg-config \
  glib2-devel libgusb-devel libusb1-devel json-glib-devel libdeflate-devel \
  openssl-devel polkit-devel python3 python3-packaging
python3 package/build-runtime.py --native --stage package/stage
```

Build as your normal user. This compiles two private libfprint libraries, three
capture/contact helpers, the private fprintd daemon and the native algorithm
library. Outputs are staged under `package/stage/usr`; compilation does not
install files or change authentication. The library's SHA256 is generated into
both clients and the worker configuration after compilation.

Source layout:

- `package/experiments/milan521d/native/`: native processing, feature extraction,
  templates, enrollment, matching and learning.
- `package/experiments/milan521d/upstream/`: pinned upstream Milan kernels.
- `package/native-engine/recipe.json`: relocatable compiler recipe and native
  source integrity hashes. Deliberate native-source edits require updating the
  corresponding source hashes before a modified build.
- `libfprint/`: libfprint source and driver registration.
- `package/upstream-fprintd/`: modified private fprintd source.
- `package/capture/` and `re/vendor-harness/live/`: scanner capture source.
- `package/runtime/` and `package/native-runtime/`: controller, worker and setup.

Large upstream replay recordings and example fingerprint images are omitted
from Git. They are unnecessary for this device's production build; unrelated
upstream replay tests need their original fixtures. `SOURCE-TREE.json` lists
these omissions. No local/user fingerprints are included.

## Fedora: make an installable RPM from this checkout

This compiles the source above; it does not extract or reuse an existing RPM:

```sh
sudo dnf install rpm-build checkpolicy policycoreutils
mkdir -p build/rpm/{SOURCES,SPECS,BUILD,BUILDROOT,RPMS,SRPMS}
git archive --format=tar.gz --prefix=goodix521d-fprintd-0.1.0/ HEAD \
  > build/rpm/SOURCES/goodix521d-fprintd-0.1.0.tar.gz
rpmbuild -ba --define "_topdir $PWD/build/rpm" \
  --define '_enable_debug_packages 0' --define 'debug_package %{nil}' \
  package/rpm/goodix521d-fprintd-native.spec
```

`git archive HEAD` uses committed source. Commit intentional source changes
before packaging them. Binary and source RPMs appear under `build/rpm/RPMS`
and `build/rpm/SRPMS`. Install the resulting x86_64 RPM with DNF (use reinstall
when replacing an already installed package with the same version), then run
`sudo goodix521d-setup` for first-time reader calibration and enroll through KDE
or GNOME. See [INSTALL.md](INSTALL.md) for service restart and upgrade details.

## Arch: compile this Git checkout

In a clean clone on Arch, install the development dependencies, adapt its paths
and setup code for Arch, then compile:

```sh
sudo pacman -S --needed base-devel meson ninja pkgconf glib2-devel libgusb \
  libusb json-glib libdeflate openssl polkit python python-packaging fprintd
python packaging/arch/arch-prepare.py "$PWD"
python package/build-runtime.py --native --stage package/stage
```

Arch preparation modifies the checkout's paths/configuration and compiler
recipe to use `/usr/lib/goodix521d`. Outputs are staged for packaging, not
installed automatically. The [AUR instructions](packaging/arch/README.md) cover
`makepkg -si` for an installable package of the pinned release; that recipe
fetches the release source archive rather than arbitrary uncommitted Git edits.
Arch setup does not edit PAM. Live hardware behavior on Arch remains unverified.

Included components retain their own copyright and license notices; see their
COPYING/LICENSE files and source headers. No optional engine mode or new sensor
support is implied by making the source available.
