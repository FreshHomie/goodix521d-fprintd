#!/usr/bin/env python3
"""Build relocatable, code-only runtime; original reader-test builds are retained."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess

HERE=Path(__file__).resolve().parent
TASK=HERE.parent
SOURCE=TASK/'libfprint'


def run(command,log,env=None):
    subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT,env=env)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',type=Path,default=HERE/'stage')
    parser.add_argument('--tests',action='store_true');parser.add_argument('--native',action='store_true');args=parser.parse_args()
    stage=args.stage.resolve();stage.mkdir(parents=True,exist_ok=True)
    integration=TASK/'re/vendor-harness/integration'
    base=(integration/'goodix_vendor_session.c').read_text()
    installed=base.replace('#ifndef GOODIX_BRIDGE','#define GOODIX_BRIDGE "/usr/libexec/goodix521d/bridge.py"\n#ifndef GOODIX_BRIDGE')
    installed=installed.replace('"/usr/bin/python3",GOODIX_BRIDGE','"/usr/bin/python3","-I",GOODIX_BRIDGE')
    installed=installed.replace('Goodix 521d with isolated vendor engine (local test)','Goodix 521d with isolated Goodix engine')
    driver=SOURCE/'libfprint/drivers/goodix_vendor.c'
    original=driver.read_bytes() if driver.exists() else None
    capture_driver=SOURCE/'libfprint/drivers/goodixtls/goodix52xd.c'
    capture_original=capture_driver.read_bytes()
    capture_headers={name:(capture_driver.parent/name).read_bytes() if (capture_driver.parent/name).exists() else None
        for name in ('cache-experiment.h','held-contact.h','retained-session.h')}
    try:
        driver.write_text(installed)
        shutil.copyfile(HERE/'capture/goodix52xd.c',capture_driver)
        for name in capture_headers:shutil.copyfile(HERE/'capture'/name,capture_driver.parent/name)
        if args.tests:
            test=installed.replace('FpiDeviceGoodixVendor','FpiDeviceGoodixWorkerTest').replace('fpi_device_goodix_vendor','fpi_device_goodix_worker_test').replace('"goodix_vendor"','"goodix_worker_test"')
            test=test.replace('{.vid=0x27c6,.pid=0x521d}','{.virtual_envvar="GOODIX_WORKER_TEST"}').replace('FP_DEVICE_TYPE_USB','FP_DEVICE_TYPE_VIRTUAL')
            test=test.replace('/usr/libexec/goodix521d/bridge.py',str(HERE/'tests/dev-session-bridge.py'))
            (SOURCE/'libfprint/drivers/goodix_worker_test.c').write_text(test)
            for path,anchor,addition in [
                (SOURCE/'meson.build',"    'goodix_vendor': { 'optional': true },","    'goodix_worker_test': { 'virtual': true, 'optional': true },"),
                (SOURCE/'libfprint/meson.build',"    'goodix_vendor' : files('drivers/goodix_vendor.c'),","    'goodix_worker_test' : files('drivers/goodix_worker_test.c'),")]:
                text=path.read_text()
                if addition not in text:
                    if text.count(anchor)!=1: raise ValueError('Unexpected Meson registry')
                    path.write_text(text.replace(anchor,anchor+'\n'+addition,1))
        with (HERE/'runtime-build.log').open('w') as log:
            common=['-Ddoc=false','-Dintrospection=false','-Dinstalled-tests=false','-Dgtk-examples=false','-Dudev_rules=disabled','-Dudev_hwdb=disabled','--prefix=/usr']
            for name,drivers in [('build-package','goodix_vendor'),('build-package-capture','goodixtls52xd')]+([('build-package-test','goodix_worker_test')] if args.tests else []):
                build=SOURCE/name
                setup=['meson','setup']+(['--reconfigure'] if (build/'meson-private/coredata.dat').exists() else [])
                run(setup+[str(build),str(SOURCE),'-Ddrivers='+drivers]+common,log)
                run(['ninja','-C',str(build),'libfprint/libfprint-2.so.2.0.0'],log)
            lib=stage/'usr/lib64/goodix521d'
            for folder,build in [('daemon-lib','build-package'),('capture-lib','build-package-capture')]:
                destination=lib/folder;destination.mkdir(parents=True,exist_ok=True)
                shutil.copy2(SOURCE/build/'libfprint/libfprint-2.so.2.0.0',destination/'libfprint-2.so.2.0.0')
                for alias in ('libfprint-2.so.2','libfprint-2.so'):
                    p=destination/alias
                    if p.is_symlink(): p.unlink()
                    p.symlink_to('libfprint-2.so.2.0.0')
            runtime=stage/'usr/libexec/goodix521d';runtime.mkdir(parents=True,exist_ok=True)
            for name in ('bridge.py','rpc.py','contact_reference.py','capture_cache.py','capture_session.py','setup_support.py'):
                shutil.copy2(HERE/'runtime'/name,runtime/name);(runtime/name).chmod(0o755 if name in ('bridge.py','worker.py') else 0o644)
            pkg=subprocess.check_output(['pkg-config','--cflags','--libs','gio-2.0','gusb'],text=True).split()
            run(['cc','-Wall','-Wextra','-Werror',str(TASK/'re/vendor-harness/live/capture-one.c'),
                '-I'+str(SOURCE/'libfprint'),'-I'+str(SOURCE/'build-package-capture/libfprint'),
                '-L'+str(SOURCE/'build-package-capture/libfprint'),'-Wl,-rpath,/usr/lib64/goodix521d/capture-lib',
                '-lfprint-2',*pkg,'-o',str(runtime/'goodix-capture-one')],log)
            run(['cc','-Wall','-Wextra','-Werror',str(TASK/'re/vendor-harness/live/contact-probe.c'),
                '-I'+str(SOURCE/'libfprint'),'-I'+str(SOURCE/'build-package-capture/libfprint'),
                '-L'+str(SOURCE/'build-package-capture/libfprint'),'-Wl,-rpath,/usr/lib64/goodix521d/capture-lib',
                '-lfprint-2',*pkg,'-o',str(runtime/'goodix-contact-probe')],log)
            run(['cc','-Wall','-Wextra','-Werror',str(TASK/'re/vendor-harness/live/capture-session.c'),
                '-I'+str(SOURCE/'libfprint'),'-I'+str(SOURCE/'build-package-capture/libfprint'),
                '-L'+str(SOURCE/'build-package-capture/libfprint'),'-Wl,-rpath,/usr/lib64/goodix521d/capture-lib',
                '-lfprint-2',*pkg,'-o',str(runtime/'goodix-capture-session')],log)
            # Build the paired daemon against our extended private API. Keep the
            # distribution's PAM, D-Bus policy and service hardening unchanged.
            pcdir=HERE/'daemon-pkgconfig';pcdir.mkdir(exist_ok=True)
            (pcdir/'libfprint-2.pc').write_text(
                'Name: libfprint-2\nDescription: Private Goodix libfprint\nVersion: 1.94.100\n'
                'Requires: gio-unix-2.0, gobject-2.0\n'
                f'Libs: -L"{SOURCE / "build-package/libfprint"}" -lfprint-2\n'
                f'Cflags: -I"{SOURCE / "libfprint"}" -I"{SOURCE / "build-package/libfprint"}"\n')
            env=os.environ.copy();paths=[str(pcdir)]
            local_polkit=HERE/'build-deps/sysroot/usr/lib64/pkgconfig'
            if local_polkit.exists():paths.append(str(local_polkit))
            if env.get('PKG_CONFIG_PATH'):paths.append(env['PKG_CONFIG_PATH'])
            env['PKG_CONFIG_PATH']=':'.join(paths)
            build=HERE/'build-fprintd';upstream=HERE/'upstream-fprintd'
            setup=['meson','setup']+(['--reconfigure'] if (build/'meson-private/coredata.dat').exists() else [])
            run(setup+[str(build),str(upstream),'--prefix=/usr','--localstatedir=/var',
                '-Dpam=false','-Dman=false','-Dgtk_doc=false','-Dsystemd=false','-Dgoodix_daemon_only=true'],log,env)
            run(['ninja','-C',str(build),'src/fprintd'],log,env)
            # Meson's install step removes its build-directory RUNPATH. A raw
            # executable copy would retain a development path in the RPM.
            run(['meson','install','-C',str(build),'--no-rebuild',
                '--tags','goodix-daemon','--destdir',str(stage)],log,env)
            if args.native:
                run(['python3',str(HERE/'build-native-engine.py'),'--stage',str(stage)],log)
            else:
                loader=TASK/'re/vendor-harness/loader'
                for output,extra in [('goodix-preprocess-one',[loader/'preprocess-one.c']),
                    ('goodix-matcher-windows18',[loader/'matcher-api.c',TASK/'re/vendor-harness/matcher/runtime-probe.c']),
                    ('goodix-template-probe',[loader/'matcher-api.c',TASK/'re/vendor-harness/templates/template-api.c',TASK/'re/vendor-harness/templates/template-probe.c'])]:
                    command=['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror']
                    if output=='goodix-matcher-windows18': command+=['-DTEMPLATE_CAPACITY=50','-DENROLL_OVERLAP_POLICY']
                    if output=='goodix-template-probe': command+=['-DENROLL_OVERLAP_POLICY']
                    run(command+['-Dmain=goodix_native_main',str(loader/'pe-loader.c'),*[str(p) for p in extra],
                        str(HERE/'runtime/native-guard.c'),'-o',str(runtime/output)],log)
                    shutil.copy2(runtime/output,HERE/'runtime'/output)
        (HERE/'build-report.json').write_text(json.dumps({'stage':str(stage),'installed_driver':'goodix_vendor',
             'system_library_replaced':False,'system_service_changed':False,'vendor_dll_bundled':False,'reader_access':False,
             'worker_separate_from_fprintd':True,'paired_private_fprintd_built':True,'resident_capture_helper_built':True,'test_library':str(SOURCE/'build-package-test/libfprint') if args.tests else None},indent=2)+'\n')
        print(json.dumps({'stage':str(stage),'tests':args.tests,'build_log':str(HERE/'runtime-build.log')}))
    finally:
        if original is not None: driver.write_bytes(original)
        capture_driver.write_bytes(capture_original)
        for name,data in capture_headers.items():
            path=capture_driver.parent/name
            if data is None:path.unlink(missing_ok=True)
            else:path.write_bytes(data)

if __name__=='__main__': main()
