#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Socket-activated native-only worker; no USB, DLLs or child commands."""
from pathlib import Path
import argparse,json,os,resource,signal,socket,stat,struct,sys
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from native_rpc import ENGINE_HASH,send,receive
from native_engine import Engine
STOP=False

def trusted(path):
 for part in (path,*path.parents):
  info=part.lstat()
  if stat.S_ISLNK(info.st_mode) or info.st_uid!=0 or info.st_mode&0o022:raise ValueError('Native runtime must be root controlled')
 if not path.is_file():raise ValueError('Native runtime file missing')
def stop(*args):
 global STOP
 STOP=True

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--development',action='store_true');parser.add_argument('--socket',type=Path);args=parser.parse_args()
 if os.geteuid()==0:raise SystemExit('Native algorithm worker must never run as root')
 os.umask(0o077);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 config=json.loads((HERE/'native-engine.json').read_text());library=Path(config['library'])
 if args.development:
  if not args.socket:raise SystemExit('Development socket required')
  parent=args.socket.parent.lstat()
  if not stat.S_ISDIR(parent.st_mode) or parent.st_uid!=os.geteuid() or parent.st_mode&0o077:raise SystemExit('Development socket parent must be private')
  allowed_uid=os.geteuid();listener=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);listener.bind(str(args.socket));args.socket.chmod(0o600);listener.listen(2)
 else:
  if args.socket or os.environ.get('LISTEN_PID')!=str(os.getpid()) or os.environ.get('LISTEN_FDS')!='1':raise SystemExit('One systemd activation socket required')
  for path in [HERE/'worker.py',HERE/'native_engine.py',HERE/'native_rpc.py',HERE/'native-engine.json',library]:trusted(path)
  allowed_uid=0;listener=socket.socket(fileno=3)
  if listener.getsockopt(socket.SOL_SOCKET,socket.SO_ACCEPTCONN)!=1:raise SystemExit('Activation fd is not a listener')
 engine=Engine(library)
 if engine.identity!=ENGINE_HASH:raise ValueError('Native engine identity mismatch')
 for sig in [signal.SIGINT,signal.SIGTERM]:signal.signal(sig,stop)
 listener.settimeout(.5)
 print('Goodix native engine worker ready',flush=True)
 try:
  while not STOP:
   try:connection,_=listener.accept()
   except socket.timeout:engine.expire();continue
   with connection:
    connection.settimeout(5)
    _,uid,_=struct.unpack('3i',connection.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12))
    if uid!=allowed_uid:continue
    try:
     request=receive(connection);result=engine.execute(request)
     reply={'version':1,'ok':True,'engine_sha256':ENGINE_HASH,'result':result}
    except Exception as error:
     # Never log image/template bytes or request contents.
     print('Native operation rejected: '+type(error).__name__,file=sys.stderr,flush=True)
     reply={'version':1,'ok':False,'engine_sha256':ENGINE_HASH,'error':'Native engine rejected the operation'}
    try:send(connection,reply)
    except (OSError,ValueError):engine.close()
 finally:
  engine.close();listener.close()
  if args.development:args.socket.unlink(missing_ok=True)
if __name__=='__main__':main()
