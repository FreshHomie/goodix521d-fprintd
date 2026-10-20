#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Adapt the pinned source release to Arch without changing algorithm code."""
from pathlib import Path
import ast,hashlib,json,sys
root=Path(sys.argv[1]).resolve()
recipe_path=root/'package/native-engine/recipe.json'
recipe=json.loads(recipe_path.read_text())
for name,digest in recipe['source_sha256'].items():
    if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest:
        raise SystemExit('Original native source hash mismatch: '+name)
# Architecture paths apply to build scripts, integration code and installed units.
for path in root.rglob('*'):
    if not path.is_file() or path.suffix not in ('.py','.c','.h','.json','.build','.service','.conf'):
        continue
    try:original=path.read_text()
    except UnicodeError:continue
    adapted=original.replace('/usr/lib64','/usr/lib').replace('/usr/libexec','/usr/lib').replace('usr/lib64/','usr/lib/').replace('usr/libexec/','usr/lib/')
    if adapted!=original:path.write_text(adapted)
# The source recipe had explicit Fedora pkg-config include directories. Resolve
# compiler/linker system flags on Arch, keeping only private source include paths.
recipe=json.loads(recipe_path.read_text())
import subprocess
flags=subprocess.check_output(['pkg-config','--cflags','gio-2.0','gusb','json-glib-1.0'],text=True).split()
libs=subprocess.check_output(['pkg-config','--libs','gio-2.0','gusb','json-glib-1.0','libdeflate'],text=True).split()
for command in recipe['commands']:
    # Explicit /usr includes are superseded by the current pkg-config result.
    command[:]=[x for x in command if not x.startswith('-I/usr/')]
    if '-c' in command:command[1:1]=flags
    else:
        command[1:1]=['-Wl,--as-needed']
        command[:]=[x for x in command if not x.startswith('-l') and not (x.startswith('/usr/') and '.so' in x)]
        command[command.index('-o'):command.index('-o')]=libs+['-lm']
for name in recipe['source_sha256']:
    recipe['source_sha256'][name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
recipe_path.write_text(json.dumps(recipe,indent=2)+'\n')
# Arch uses /usr/lib for libexec; set the Meson default explicitly as well.
build=root/'package/build-runtime.py'
s=build.read_text().replace("'--prefix=/usr']","'--prefix=/usr','--libdir=lib','--libexecdir=lib']").replace("str(upstream),'--prefix=/usr','--localstatedir=/var',","str(upstream),'--prefix=/usr','--libdir=lib','--libexecdir=lib','--localstatedir=/var',")
s=s.replace("['cc','-Wall','-Wextra','-Werror'", "['cc','-Wl,-z,relro,-z,now','-Wl,--as-needed','-Wall','-Wextra','-Werror'")
build.write_text(s)
engine_builder=root/'package/build-native-engine.py'
engine_builder.write_text(engine_builder.read_text().replace("'--strip-debug'", "'--strip-unneeded'"))
setup=root/'package/native-runtime/setup.py'
s=setup.read_text().replace('Fedora44','Arch Linux').replace('Fedora 44','Arch Linux')
s=s.replace("os_release.get('ID')!='fedora' or os_release.get('VERSION_ID')!='44'", "os_release.get('ID')!='arch' and 'arch' not in os_release.get('ID_LIKE','').split()")
# Keep architecture restriction correctly grouped after distro adaptation.
s=s.replace("if os_release.get('ID')!='arch' and 'arch' not in os_release.get('ID_LIKE','').split() or platform.machine()!='x86_64':", "if (os_release.get('ID')!='arch' and 'arch' not in os_release.get('ID_LIKE','').split()) or platform.machine()!='x86_64':")
a=s.index("    try:result['fingerprint_authentication_enabled']");b=s.index('    print(json.dumps(result',a)
s=s[:a]+"    result['pam_configuration']='managed by the administrator; setup does not edit PAM'\n"+s[b:]
s=s.replace("help='Leave authselect configuration unchanged'", "help='Leave authentication configuration unchanged (always true on Arch)'")
s=s.replace("auth='left-unchanged'", "auth='PAM unchanged; see /usr/share/doc/goodix521d-fprintd/README.md'")
# Setup should work on standard Arch without any SELinux tooling/authselect.
a=s.index("                if Path('/usr/sbin/selinuxenabled')");b=s.index("            support.run(['systemctl','enable'",a);s=s[:a]+s[b:]
a=s.index('            if not a.no_authentication:');b=s.index("            print('\\nReader setup complete",a)
s=s[:a]+s[b:]
setup.write_text(s)
support=root/'package/native-runtime/setup_support.py'
s=support.read_text();tree=ast.parse(s)
for node in tree.body:
    if isinstance(node,ast.FunctionDef) and node.name=='configure_authentication':
        lines=s.splitlines(keepends=True);lines[node.lineno-1:node.end_lineno]=["def configure_authentication(runner=None):\n    return 'manual PAM configuration required; setup does not edit PAM'\n"];s=''.join(lines);break
support.write_text(s)
# Record the packaging adaptation separately from the upstream source inventory.
(root/'package/ARCH-ADAPTATION.json').write_text(json.dumps({'base':'Fedora release20 source archive','paths':'/usr/lib/goodix521d','pam_modified':False,'algorithm_settings_changed':False},indent=2)+'\n')
