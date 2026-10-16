#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Installed guided setup for Fedora44 USB Goodix27c6:521d."""
from pathlib import Path
import argparse,fcntl,hashlib,json,os,platform,signal,stat,sys,tempfile
sys.path.insert(0,'/usr/libexec/goodix521d')
from contact_reference import load,validate,DESTINATION
import setup_support as support
from rpc import ENGINE_HASH


def hardware():
    readers=[]
    for p in Path('/sys/bus/usb/devices').iterdir():
        try:
            if (p/'idVendor').read_text().strip()=='27c6' and (p/'idProduct').read_text().strip()=='521d':readers.append(p.name)
        except OSError:pass
    return readers


def doctor():
    result={'package':'goodix521d-fprintd','supported_os':'Fedora44 x86_64',
            'usb_27c6_521d_count':len(hardware()),'required_firmware':'GFUSB_GM168SEC_APP_10019'}
    try:result['engine_hash_valid']=hashlib.sha256(support.trusted_file(Path('/usr/lib64/goodix521d/libgoodix521d-native.so'),16*1024*1024)).hexdigest()==ENGINE_HASH
    except (OSError,ValueError):result['engine_hash_valid']=False
    try:
        reference=load();result['measured_contact_reference_present']=reference is not None
        result['post_calibration_guard_present']=bool(reference and 'post_reference' in reference)
    except (OSError,ValueError):result['measured_contact_reference_present']=False;result['post_calibration_guard_present']=False
    for name in ('fprintd.service','goodix521d-worker.socket','goodix521d-worker.service'):
        result[name]=support.run(['systemctl','show',name,'-p','ActiveState','--value'],capture_output=True).stdout.strip()
    try:result['fingerprint_authentication_enabled']='with-fingerprint' in support.run(['authselect','current','--raw'],capture_output=True).stdout.split()[1:]
    except Exception:result['fingerprint_authentication_enabled']=False
    print(json.dumps(result,indent=2));return result


def main():
    p=argparse.ArgumentParser(description='Set up the Goodix521d fingerprint reader for KDE/GNOME on Fedora44.')
    p.add_argument('--recalibrate',action='store_true',help='Repeat the guided contact measurement')
    p.add_argument('--no-authentication',action='store_true',help='Leave authselect configuration unchanged')
    p.add_argument('--doctor',action='store_true',help='Print a diagnostic report without changing anything')
    a=p.parse_args()
    if a.doctor:doctor();return
    if os.geteuid()!=0:raise SystemExit('Run sudo goodix521d-setup, or open Goodix Fingerprint Setup from the application menu.')
    os.umask(0o077)
    def stop(signum,frame):raise KeyboardInterrupt
    for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,stop)
    os_release=platform.freedesktop_os_release()
    if os_release.get('ID')!='fedora' or os_release.get('VERSION_ID')!='44' or platform.machine()!='x86_64':
        raise SystemExit('This package supports Fedora44 x86_64.')
    if len(hardware())!=1:raise SystemExit('Connect exactly one supported Goodix USB reader (27c6:521d).')
    if not sys.stdin.isatty():raise SystemExit('Setup needs an interactive terminal for its press/lift prompts.')
    # Pin an exclusive setup lock; never follow a user-created symbolic link.
    info=support.STATE.lstat()
    if not support.STATE.is_dir() or info.st_uid!=0 or info.st_mode&0o022 or support.STATE.is_symlink():
        raise SystemExit('Unexpected Goodix state-directory permissions.')
    fd=os.open(support.STATE/'setup.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW|os.O_CLOEXEC,0o600)
    try:
        lock=os.fstat(fd)
        if not stat.S_ISREG(lock.st_mode) or lock.st_uid!=0 or lock.st_nlink!=1 or lock.st_mode&0o077:raise ValueError('Invalid setup lock')
        fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        print('Goodix521d setup — existing enrolled fingerprints are preserved.',flush=True)
        with tempfile.TemporaryDirectory(prefix='goodix521d-setup-') as temporary:
            folder=Path(temporary)
            library=Path('/usr/lib64/goodix521d/libgoodix521d-native.so')
            if hashlib.sha256(support.trusted_file(library,16*1024*1024)).hexdigest()!=ENGINE_HASH:
                raise ValueError('Native engine identity mismatch; reinstall the package')
            print('Native matching engine is installed.',flush=True)
            with support.exclusive_reader():
                otp=support.identity();reference=load()
                if reference is not None and reference['otp_sha256']!=otp:
                    raise ValueError('The saved contact reference belongs to a different reader; it was preserved')
                if reference is None or a.recalibrate or 'post_reference' not in reference:
                    report=support.measure_contact()
                    print('\033[1;31m>>> LIFT ALL FINGERS — KEEP THE SENSOR CLEAR <<<\033[0m',flush=True)
                    input('Once every finger is OFF, press Enter for clear-sensor calibration: ')
                    print('CALIBRATING — keep the sensor clear until setup finishes.',flush=True)
                    raw=json.dumps(report,sort_keys=True,separators=(',',':')).encode()
                    provisional=validate({'version':1,'profile':support.PROFILE,'otp_sha256':otp,
                        'reference':support.derive(report),'source_sha256':hashlib.sha256(raw).hexdigest()})
                    clear=folder/'clear';clear.mkdir(mode=0o700)
                    post=support.post_reference(provisional,clear)
                    reference=support.save_reference(report,otp,post,DESTINATION)
                    print('This reader’s contact reference is measured and installed.',flush=True)
                else:print('This reader’s existing contact reference is already configured.',flush=True)
                if Path('/usr/sbin/selinuxenabled').exists():
                    import subprocess
                    if subprocess.run(['/usr/sbin/selinuxenabled']).returncode==0:
                        support.run(['/usr/sbin/restorecon','-R',str(support.STATE)])
            support.run(['systemctl','enable','--now','goodix521d-worker.socket'])
            support.run(['systemctl','try-restart','goodix521d-worker.service'])
            support.run(['systemctl','restart','fprintd.service'])
            auth='left-unchanged'
            if not a.no_authentication:
                try:auth=support.configure_authentication()
                except Exception as e:
                    auth='needs-manual-configuration'
                    print('Reader ready, but fingerprint login needs manual configuration: '+str(e),flush=True)
            print('\nReader setup complete. Open KDE Users or GNOME Users to enroll your finger.',flush=True)
            print('Fingerprint authentication: '+auth+'. Your password remains available.',flush=True)
            print('For diagnostics: sudo goodix521d-setup --doctor',flush=True)
    finally:os.close(fd)


if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:print('\nSetup cancelled; existing fingerprints were preserved.',file=sys.stderr);sys.exit(130)
    except Exception as e:print('Setup did not complete: '+str(e)+'\nRun sudo goodix521d-setup --doctor for diagnostics.',file=sys.stderr);sys.exit(1)
