# SPDX-License-Identifier: MIT
"""Root-private, daemon-scoped clear calibration; never stores fingerprint images."""
from pathlib import Path
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
import stat
import struct
import time

DIRECTORY=Path('/run/goodix521d-capture')
MAX_SECONDS=120
START_MAX_SECONDS=80
LIMIT=32768


def epoch():
    return {'monotonic':time.monotonic(),'boottime':time.clock_gettime(time.CLOCK_BOOTTIME)}


def elapsed(seed,current):
    if set(seed)!= {'monotonic','boottime'} or set(current)!=set(seed):
        raise ValueError('Invalid cache clock fields')
    if any(type(v) not in (int,float) or not 0<=v<1e12 for v in (*seed.values(),*current.values())):
        raise ValueError('Invalid cache clock')
    mono=current['monotonic']-seed['monotonic'];boot=current['boottime']-seed['boottime']
    if mono<0 or boot<0 or abs(boot-mono)>.1:
        raise ValueError('Calibration invalidated by sleep or clock reversal')
    return boot


def context(reference):
    """Only kernel/root inputs identify the boot, daemon, reader and power epoch."""
    try:
        parent=os.getppid();fields=Path(f'/proc/{parent}/stat').read_text().rsplit(')',1)[1].split()
        readers=[]
        for device in Path('/sys/bus/usb/devices').iterdir():
            try:
                if (device/'idVendor').read_text().strip()!='27c6' or (device/'idProduct').read_text().strip()!='521d':continue
                readers.append({'path':str(device.resolve()),'bus':int((device/'busnum').read_text()),
                    'address':int((device/'devnum').read_text()),
                    'power_state':(device/'power/runtime_status').read_text().strip(),
                    'power_suspended_ms':int((device/'power/runtime_suspended_time').read_text())})
            except (OSError,ValueError):continue
        if len(readers)!=1 or readers[0]['power_state']!='active':return None
        return {'boot':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'daemon_pid':parent,'daemon_start_ticks':int(fields[19]),'reader':readers[0],
            'reference_sha256':hashlib.sha256(json.dumps(reference,sort_keys=True).encode()).hexdigest()}
    except (OSError,ValueError,IndexError):return None


def validate(record,current_context,current_epoch,starting=False):
    if not isinstance(record,dict) or set(record)!= {'version','context','epoch','baseline','background','sha256'} or type(record['version'])!=int or record['version']!=1:
        raise ValueError('Invalid cache record')
    if current_context is None or record['context']!=current_context:
        raise ValueError('Calibration context changed')
    age=elapsed(record['epoch'],current_epoch)
    if (starting and age>START_MAX_SECONDS) or age>=MAX_SECONDS:
        raise ValueError('Calibration cache expired')
    baseline=record['baseline']
    if not isinstance(baseline,list) or len(baseline)!=8 or any(type(v)!=int or not 0<=v<=65535 for v in baseline):
        raise ValueError('Invalid cached contact baseline')
    if not isinstance(record['background'],str) or len(record['background'])!=20480:
        raise ValueError('Invalid cached background size')
    raw=bytes.fromhex(record['background'])
    if any(v>4095 for v in struct.unpack('<5120H',raw)) or hashlib.sha256(raw).hexdigest()!=record['sha256']:
        raise ValueError('Invalid cached background pixels')
    return raw


class CaptureCache:
    def __init__(self,directory=DIRECTORY,owner=0):
        self.directory=Path(directory);self.owner=owner;self.active=False;self.dir_fd=None
    @contextmanager
    def locked(self):
        lock=None
        try:
            if os.geteuid()!=self.owner:yield self;return
            # Production storage is created/removed by systemd, never mkdir'd by
            # a controller in an arbitrary or user-owned parent directory.
            if self.directory==DIRECTORY:
                for parent in self.directory.parents:
                    info=parent.lstat()
                    if not stat.S_ISDIR(info.st_mode) or info.st_uid!=0 or info.st_mode&0o022:
                        raise ValueError('Untrusted cache parent')
            self.dir_fd=os.open(self.directory,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW|os.O_CLOEXEC)
            info=os.fstat(self.dir_fd)
            if info.st_uid!=self.owner or stat.S_IMODE(info.st_mode)!=0o700:
                raise ValueError('Expected private calibration directory')
            lock=os.open('lock',os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW|os.O_CLOEXEC,0o600,dir_fd=self.dir_fd)
            info=os.fstat(lock)
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=self.owner or info.st_nlink!=1 or stat.S_IMODE(info.st_mode)!=0o600:
                raise ValueError('Invalid cache lock')
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            self.active=True
        except (OSError,ValueError):
            # Cache availability never prevents the existing fresh path.
            if lock is not None:os.close(lock);lock=None
            if self.dir_fd is not None:os.close(self.dir_fd);self.dir_fd=None
        try:yield self
        finally:
            self.active=False
            if lock is not None:os.close(lock)
            if self.dir_fd is not None:os.close(self.dir_fd);self.dir_fd=None
    def invalidate(self):
        if not self.active:return
        try:os.unlink('calibration.json',dir_fd=self.dir_fd)
        except FileNotFoundError:pass
        except OSError:self.active=False
    def load(self,current_context,current_epoch):
        if not self.active:return None
        fd=None
        try:
            fd=os.open('calibration.json',os.O_RDONLY|os.O_NOFOLLOW|os.O_CLOEXEC,dir_fd=self.dir_fd)
            info=os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=self.owner or info.st_nlink!=1 or stat.S_IMODE(info.st_mode)!=0o400 or not 0<info.st_size<=LIMIT:
                raise ValueError('Invalid calibration cache file')
            data=os.read(fd,LIMIT+1)
            if len(data)!=info.st_size:raise ValueError('Incomplete calibration cache file')
            record=json.loads(data);validate(record,current_context,current_epoch,starting=True)
            return record
        except (OSError,ValueError,TypeError,KeyError,struct.error):
            self.invalidate();return None
        finally:
            if fd is not None:os.close(fd)
    def store(self,record,current_context,current_epoch):
        if not self.active:return False
        try:
            validate(record,current_context,current_epoch,starting=True)
            data=json.dumps(record,separators=(',',':')).encode()
            if len(data)>LIMIT:raise ValueError('Oversized calibration cache')
            # The directory is root-private and pinned by dir_fd. Atomic replace
            # never follows an existing destination symlink.
            name='.'+os.urandom(16).hex()
            fd=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW|os.O_CLOEXEC,0o400,dir_fd=self.dir_fd)
            try:
                with os.fdopen(fd,'wb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
                os.replace(name,'calibration.json',src_dir_fd=self.dir_fd,dst_dir_fd=self.dir_fd)
            finally:
                try:os.unlink(name,dir_fd=self.dir_fd)
                except FileNotFoundError:pass
            return True
        except (OSError,ValueError,TypeError,KeyError,struct.error):
            self.invalidate();return False


def record(background,baseline,identity,stamp):
    raw=struct.pack('<5120H',*background)
    return {'version':1,'context':identity,'epoch':stamp,'baseline':baseline,
        'background':raw.hex(),'sha256':hashlib.sha256(raw).hexdigest()}
