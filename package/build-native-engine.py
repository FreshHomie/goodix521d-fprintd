#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Rebuild the native library from the relocatable, source-only compile recipe."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
P=Path(__file__).resolve().parent
ROOT=P.parent

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--stage',required=True,type=Path);args=parser.parse_args()
 stage=args.stage.resolve();recipes=json.loads((P/'native-engine/recipe.json').read_text())
 for relative,expected in recipes['source_sha256'].items():
  if hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()!=expected:raise ValueError('Native source changed: '+relative)
 for command in recipes['commands']:
  command=[part.replace('@ROOT@',str(ROOT)) for part in command]
  output=Path(command[command.index('-o')+1]);output.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run(command,check=True)
 library=ROOT/recipes['output'];subprocess.run(['strip','--strip-debug',str(library)],check=True)
 identity=hashlib.sha256(library.read_bytes()).hexdigest()
 runtime=stage/'usr/libexec/goodix521d';native=runtime/'native';native.mkdir(exist_ok=True)
 dest=stage/'usr/lib64/goodix521d/libgoodix521d-native.so';shutil.copyfile(library,dest);dest.chmod(0o644)
 for name in ('worker.py','goodix-matcher-windows18','goodix-preprocess-one','goodix-template-probe'):(runtime/name).unlink(missing_ok=True)
 rpc=(P/'runtime/rpc.py').read_text().replace('5995d6565e602416cbe00f171ad4941203e345ccaddb490199274166adac4f5c',identity)
 (runtime/'rpc.py').write_text(rpc);(native/'native_rpc.py').write_text(rpc)
 for name in ('worker.py','native_engine.py'):shutil.copyfile(P/'native-runtime'/name,native/name)
 (native/'worker.py').chmod(0o755)
 (native/'native-engine.json').write_text(json.dumps({'backend':'native','library':'/usr/lib64/goodix521d/libgoodix521d-native.so','sha256':identity},indent=2)+'\n')
 shutil.copyfile(P/'native-runtime/setup_support.py',runtime/'setup_support.py')
 for path in runtime.rglob('*.py'):
  if path.name not in ('worker.py','bridge.py'):path.chmod(0o644)
 (native/'native-engine.json').chmod(0o644)
 print(json.dumps({'native_library_sha256':identity,'built_from_source':True}))
if __name__=='__main__':main()
