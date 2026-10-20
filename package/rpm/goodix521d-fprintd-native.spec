Name:           goodix521d-fprintd
Version:        0.1.0
Release:        20%{?dist}
Summary:        Experimental Goodix 27c6:521d backend for fprintd
License:        LGPL-2.1-or-later AND GPL-2.0-or-later AND MIT
Source0:        %{name}-%{version}.tar.gz
ExclusiveArch:  x86_64
BuildRequires:  gcc gcc-c++ meson ninja-build pkgconf-pkg-config
BuildRequires:  libdeflate-devel json-glib-devel
BuildRequires:  glib2-devel libgusb-devel openssl-devel polkit-devel checkpolicy policycoreutils
Requires:       fprintd >= 1.94.5
Requires:       python3 systemd policycoreutils fprintd-pam authselect polkit
Requires(pre):  systemd
Requires(post): systemd policycoreutils
Requires(preun): systemd
Requires(postun): systemd policycoreutils
# Private libfprint copies must not satisfy the system libfprint capability.
%global __provides_exclude_from ^%{_libdir}/goodix521d/.*$

%description
Native fingerprint support for Fedora 44 x86_64 and USB 27c6:521d with
firmware GFUSB_GM168SEC_APP_10019. fprintd uses a private patched libfprint.
The native image-processing and matching engine runs in an unprivileged
worker. No Windows driver DLL or download is required. Run goodix521d-setup
for first-time reader calibration, then enroll through KDE or GNOME.

%prep
%autosetup

%build
python3 package/build-runtime.py --native --stage package/stage
mkdir -p package/policy
checkmodule -M -m -o package/policy/goodix521d.mod package/native-runtime/goodix521d.te
semodule_package -o package/policy/goodix521d.pp -m package/policy/goodix521d.mod -f package/native-runtime/goodix521d.fc

%install
mkdir -p "%{buildroot}"
cp -a package/stage/usr "%{buildroot}/"
install -Dpm0755 package/runtime/import-contact-reference.py %{buildroot}%{_bindir}/goodix521d-import-contact-reference
install -Dpm0755 package/native-runtime/setup.py %{buildroot}%{_bindir}/goodix521d-setup
install -Dpm0644 package/desktop/goodix521d-setup.desktop %{buildroot}%{_datadir}/applications/goodix521d-setup.desktop
install -Dpm0644 package/systemd/goodix521d-worker.socket %{buildroot}%{_unitdir}/goodix521d-worker.socket
install -Dpm0644 package/native-runtime/goodix521d-worker.service %{buildroot}%{_unitdir}/goodix521d-worker.service
install -Dpm0644 package/native-runtime/50-goodix521d.conf %{buildroot}%{_unitdir}/fprintd.service.d/50-goodix521d.conf
install -Dpm0644 package/systemd/goodix521d.sysusers %{buildroot}%{_sysusersdir}/goodix521d.conf
install -Dpm0644 package/policy/goodix521d.pp %{buildroot}%{_datadir}/selinux/packages/goodix521d.pp
install -Dpm0644 package/systemd/70-goodix521d.rules %{buildroot}%{_udevrulesdir}/70-goodix521d.rules
mkdir -p %{buildroot}%{_sharedstatedir}/goodix521d

%pre
/usr/bin/systemd-sysusers --inline 'u goodix-fp - "Goodix fingerprint algorithm worker" / -' || exit 1

%post
if /usr/sbin/selinuxenabled 2>/dev/null; then
    /usr/sbin/semodule -i %{_datadir}/selinux/packages/goodix521d.pp || exit 1
    /usr/sbin/restorecon -R %{_libexecdir}/goodix521d %{_sharedstatedir}/goodix521d || exit 1
fi
/usr/bin/systemctl daemon-reload || :
/usr/bin/udevadm control --reload || :
# First-time guided setup activates the socket after reader measurement.
# Package scripts do not unmask fprintd or change PAM.

%preun
if [ "$1" -eq 0 ]; then
    /usr/bin/systemctl disable --now goodix521d-worker.socket || :
    /usr/bin/systemctl stop goodix521d-worker.service || :
fi

%postun
/usr/bin/systemctl daemon-reload || :
/usr/bin/udevadm control --reload || :
if [ "$1" -eq 0 ]; then
    if /usr/sbin/selinuxenabled 2>/dev/null; then
        /usr/sbin/semodule -r goodix521d || :
        /usr/sbin/restorecon -R %{_sharedstatedir}/goodix521d || :
    fi
    /usr/bin/systemctl try-restart fprintd.service || :
fi

%files
%license package/licenses/COPYING.libfprint package/licenses/COPYING.fprintd re/pr81-LICENSE re/vendor-harness/loader/LICENSE
%doc package/NATIVE-INSTALL.md package/native-engine/SOURCE-MANIFEST.json
%{_libdir}/goodix521d/
%{_libexecdir}/goodix521d/
%{_bindir}/goodix521d-import-contact-reference
%{_bindir}/goodix521d-setup
%{_datadir}/applications/goodix521d-setup.desktop
%{_unitdir}/goodix521d-worker.service
%{_unitdir}/goodix521d-worker.socket
%{_unitdir}/fprintd.service.d/50-goodix521d.conf
%{_udevrulesdir}/70-goodix521d.rules
%{_sysusersdir}/goodix521d.conf
%{_datadir}/selinux/packages/goodix521d.pp
%dir %{_sharedstatedir}/goodix521d

%changelog
* Tue Oct 20 2026 Local Goodix development - 0.1.0-20
- Preserve preprocessing failure stages and native input-retry status.
- Reset capture/calibration on known rejected background or raw input and request a lift.
- Retain fatal handling of invalid data, engine identity and unexpected failures.
* Sun Oct 18 2026 Local Goodix development - 0.1.0-19
- Treat unsupported probe classifications as frame retries without matching or learning.
- Preserve independent same-template frame quorum and cached capture behavior.
- Log bounded native failure stage/codes without fingerprint or request contents.
* Fri Oct 16 2026 Local Goodix development - 0.1.0-18
- Build native matching, capture libraries, helpers and paired daemon from source.
- Include no Windows DLL, PE loader, user fingerprints or calibration.
- Retain release17 enrollment, matching and cached-calibration behavior.
* Wed Oct 14 2026 Local Goodix development - 0.1.0-16
- Add installed guided first-time setup and a desktop menu launcher.
- Fetch the pinned Microsoft driver with archive and engine hash verification.
- Measure reader-specific pre/post-calibration contact references locally.
- Configure the existing authselect fingerprint feature and provide diagnostics.
- Retain release15 capture and matching behavior and existing enrollments.
* Wed Oct 14 2026 Local Goodix development - 0.1.0-15
- Identify against the requesting user's enrolled templates from one capture.
- Require three positive frames against one template without pooling votes.
- Persist learning only to the matched template; retain existing enrollment flow.
* Wed Oct 14 2026 Local Goodix development - 0.1.0-14
- Keep one guarded USB/TLS capture session across fprintd claims.
- Apply native enrollment overlap rollback and successful-match learning.
- Pair libfprint with private fprintd to persist learned templates.
- Recalibrate after sleep, device/power change, expiry or capture failure.
* Tue Oct 13 2026 Local Goodix development - 0.1.0-13
- Permit systemd to bind-mount the private calibration RuntimeDirectory.
- Retain the existing daemon sandbox and root-only cache permissions.
* Tue Oct 13 2026 Local Goodix development - 0.1.0-12
- Reuse a bounded root-private clear calibration across verification helpers.
- Accept stable already-held contact only with a valid calibration cache.
- Discard cache after sleep, device/power changes, expiry, cancellation or errors.
* Mon Oct 12 2026 Local Goodix development - 0.1.0-6
- Keep one accepted clear background/baseline during an enrollment session.
- Preserve enrollment stages through contact timeouts and centering retries.
- Report progress promptly; retain clear/lift detection between scans.
* Mon Oct 12 2026 Local Goodix development - 0.1.0-5
- Gate background calibration on an explicit local clear contact reference.
- Bind reference to sensor OTP and request finger removal through fprintd retries.
* Mon Oct 12 2026 Local Goodix development - 0.1.0-4
- Report capture readiness, duration and bounded final helper errors in journal.
* Mon Oct 12 2026 Local Goodix development - 0.1.0-3
- Label tempfile probe files private; permit bounded Python descriptor checks.
- Log worker exception type and reason without request data.
* Mon Oct 12 2026 Local Goodix development - 0.1.0-2
- Permit systemd to create the worker-labelled activation socket under SELinux.
* Mon Oct 12 2026 Local Goodix development - 0.1.0-1
- Package the isolated Goodix 521d backend without proprietary engine or prints.
