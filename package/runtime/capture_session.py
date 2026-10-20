# SPDX-License-Identifier: MIT
"""Resident root USB owner. Algorithms remain in the separate unprivileged worker.

Only inherited pipes accept commands. Idle work acquires guarded blank images,
never fingerprint bursts. Suspend/error/cancellation destroys the USB/TLS host.
"""
from pathlib import Path
import os
import select
import subprocess
import sys
import tempfile
import time

import bridge
from contact_reference import load as load_reference
from capture_cache import context,epoch

HERE=Path(__file__).resolve().parent


class CaptureHost:
    def __init__(self,reference):
        if reference is None:raise ValueError('Retained capture requires the installed clear-contact reference')
        self.reference=reference;self.process=None;self.buffer=bytearray()
        self.power_context=None;self.calibration_epoch=None
        self.learned_current_contact=False

    def line(self,deadline):
        while b'\n' not in self.buffer:
            remaining=deadline-time.monotonic()
            if remaining<=0:raise TimeoutError('Retained capture host timed out')
            ready,_,_=select.select([self.process.stdout],[],[],remaining)
            if not ready:raise TimeoutError('Retained capture host timed out')
            data=os.read(self.process.stdout.fileno(),4096)
            if not data:raise RuntimeError('Retained capture host disconnected')
            self.buffer.extend(data)
            if len(self.buffer)>1024*1024:raise ValueError('Capture host output exceeded its limit')
        line,_,rest=self.buffer.partition(b'\n');self.buffer=bytearray(rest)
        if len(line)>16384:raise ValueError('Oversized capture host line')
        return line.decode('utf-8')

    def start(self):
        if self.process is not None and self.process.poll() is None:return
        self.close()
        env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','G_MESSAGES_DEBUG':'all',
            'LD_LIBRARY_PATH':str(bridge.CAPTURE_LIB),'GOODIX521D_CAPTURE_PROFILE':'pr81-full',
            'GOODIX521D_SAVE_FRAMES':'1','GOODIX521D_FRAME_DUMP_PREFIX':'capture',
            'GOODIX521D_TIMED_CAPTURE':'1','GOODIX521D_RETAIN_CAPTURE':'1',
            'GOODIX521D_CLEAR_REFERENCE':','.join(map(str,self.reference['reference'])),
            'GOODIX521D_CLEAR_OTP_SHA256':self.reference['otp_sha256']}
        if 'post_reference' in self.reference:
            env['GOODIX521D_POST_CLEAR_REFERENCE']=','.join(map(str,self.reference['post_reference']))
        self.process=subprocess.Popen([str(HERE/'goodix-capture-session')],stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0,env=env)
        deadline=time.monotonic()+15
        while True:
            line=self.line(deadline)
            if line=='SESSION_OPENED':break
            print(line,file=sys.stderr,flush=True)

    def capture(self,directory,operation,events=True):
        self.start();directory=directory.absolute()
        before=context(self.reference);now=epoch()
        if before is None:raise ValueError('Cannot bind retained reader power identity')
        if self.calibration_epoch is not None:
            old=self.calibration_epoch
            sleep=(now['boottime']-old['boottime'])-(now['monotonic']-old['monotonic'])
            if (before!=self.power_context or
                now['monotonic']<old['monotonic'] or abs(sleep)>.1):
                self.close();self.start();before=context(self.reference);now=epoch()
                if before is None:raise ValueError('Cannot bind resumed reader power identity')
        command={'enroll':'ENROLL','verify':'VERIFY','calibrate':'CALIBRATE'}[operation]
        self.process.stdin.write((command+' '+str(directory)+'\n').encode());self.process.stdin.flush()
        deadline=time.monotonic()+45;status=None;lease=None;baseline=None;contact_kind=None;failure=None
        with (directory/'capture.log').open('w') as log:
            while status is None:
                line=self.line(deadline);log.write(line+'\n')
                if line.startswith('Session capture failed:'):
                    failure=' '.join(line.split())[:300]
                if line.startswith('SESSION_DONE '):status=int(line.split()[1]);continue
                if line.startswith('SESSION_REUSED '):
                    age=int(line.split()[1])/1000
                    if not 0<=age<=80:raise bridge.RemoveFingerRequired('Retained calibration age invalid')
                    lease=time.monotonic()-age
                if line.startswith('SESSION_CONTACT '):
                    contact_kind=line.split()[1]
                    if contact_kind not in ('HELD','CLEAR'):raise ValueError('Invalid retained contact kind')
                if line.startswith(('CONTACT_BASELINE ','CAPTURE_BASELINE ')):
                    baseline=list(map(int,line.split()[1:]));
                    if len(baseline)!=8:raise ValueError('Invalid retained contact baseline')
                if not events:continue
                if line=='CLEAR_GUARD WAIT':bridge.emit('STATE REMOVE')
                elif line=='CLEAR_GUARD READY':bridge.emit('STATE WAIT')
                elif line=='CONTACT_CAPTURE_READY' or 'Timed capture: image burst starts in 3 seconds' in line:
                    bridge.emit('STATE PRESS')
                elif line=='CONTACT_CAPTURE_CONFIRMED':bridge.emit('STATE HOLD')
        if events:bridge.emit('STATE LIFT')
        if status not in (0,10):
            self.close();raise bridge.RemoveFingerRequired(failure or 'Retained capture failed; lift for fresh calibration')
        if operation=='calibrate':
            if not all((directory/f'clear-{i}.pgm').is_file() for i in range(4)):
                self.close();raise ValueError('Incomplete retained clear calibration')
            for i in range(4):bridge.read12(directory/f'clear-{i}.pgm')
        else:
            raw=list(directory.glob('capture-burst-*-raw-*.pgm'))
            if len(raw)!=10:raise ValueError('Retained capture did not save ten raw frames')
            for number in range(1,11):bridge.read12(directory/f'capture-burst-0001-raw-{number:02d}.pgm')
            bridge.read12(directory/'capture-burst-0001-background.pgm')
        if context(self.reference)!=before:
            self.close();raise bridge.RemoveFingerRequired('Reader power changed during retained acquisition')
        if lease is None:
            self.power_context=before;self.calibration_epoch=now
        elif contact_kind is None:
            raise ValueError('Missing retained stable-contact confirmation')
        if lease is None or contact_kind=='CLEAR':self.learned_current_contact=False
        return {'status':status,'calibration_monotonic':lease or now['monotonic'],'baseline':baseline,
                'capture_context':before,'capture_epoch':now,
                'retained':lease is not None}

    def close(self):
        process=self.process;self.process=None;self.buffer.clear()
        self.power_context=None;self.calibration_epoch=None
        self.learned_current_contact=False
        if process is None:return
        try:
            if process.poll() is None:
                try:process.stdin.write(b'QUIT\n');process.stdin.flush()
                except (BrokenPipeError,OSError):pass
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    try:process.wait(timeout=2)
                    except subprocess.TimeoutExpired:process.kill();process.wait()
        finally:
            process.stdin.close();process.stdout.close()


class SessionController(bridge.Controller):
    def __init__(self,client=None,host=None,reference=None):
        super().__init__(client)
        self.clear_reference=reference if reference is not None else load_reference()
        self.host=host if host is not None else CaptureHost(self.clear_reference)
        self.verification_cache=None;self.calibration_candidate=None
        self.capture_context=None;self.capture_epoch=None;self.calibration_mono=None

    def capture(self,folder,stage,attempt):
        directory=folder/f'capture-{stage:02d}-{attempt:02d}';directory.mkdir(mode=0o700)
        bridge.emit('STATE WAIT')
        self.host.start()
        before=context(self.clear_reference);stamp=epoch()
        result=self.host.capture(directory,self.operation)
        self.capture_context=result.get('capture_context',before);self.capture_epoch=result.get('capture_epoch',stamp)
        self.calibration_mono=result['calibration_monotonic']
        self.learning_allowed=not self.host.learned_current_contact
        self.verify_guard()
        background=bridge.transform(bridge.read12(directory/'capture-burst-0001-background.pgm')).hex()
        first=self.preprocess(background=background,
            raw=bridge.transform(bridge.read12(directory/'capture-burst-0001-raw-01.pgm')).hex())
        print('Goodix capture: retained session' if result['retained'] else 'Goodix capture: fresh session calibration',
              file=sys.stderr,flush=True)
        return first

    def verify_guard(self):
        if self.capture_epoch is None:return
        now=epoch();old=self.capture_epoch
        sleep=(now['boottime']-old['boottime'])-(now['monotonic']-old['monotonic'])
        if (now['monotonic']<old['monotonic'] or abs(sleep)>.1 or
            now['monotonic']-self.calibration_mono>120 or context(self.clear_reference)!=self.capture_context):
            self.host.close()
            raise bridge.RemoveFingerRequired('Retained power/device/calibration epoch changed')

    def learning_complete(self):
        # A held retry may authenticate again, but cannot repeatedly teach
        # correlated images until an actual stable clear report is observed.
        self.host.learned_current_contact=True

    def perform(self,kind,native=None):
        # The base controller uses this attribute to choose contact/countdown.
        self.operation='verify' if kind=='identify' else kind
        try:
            with self.cache.locked():
                self.cache.invalidate()
                with tempfile.TemporaryDirectory(prefix='goodix-capture-') as temporary:
                    folder=Path(temporary)
                    if kind=='enroll':
                        self.enroll(folder)
                    elif kind=='identify':self.identify(folder,native)
                    else:self.verify(folder,native)
        except BaseException:
            self.host.close();self.cache.invalidate();raise
        finally:
            del self.operation

    def maintain(self):
        # No fingerprint is captured during idle servicing. Every full blank
        # refresh still passes the existing clear/OTP/post-clear guards.
        self.capture_epoch=None;self.capture_context=None;self.calibration_mono=None
        with self.cache.locked(),tempfile.TemporaryDirectory(prefix='goodix-idle-clear-') as temporary:
            self.cache.invalidate()
            self.host.capture(Path(temporary),'calibrate',events=False)


def read_gallery(stream,count):
    if not count.isascii() or not count.isdecimal() or not 1<=int(count)<=bridge.MAX_GALLERY:
        raise ValueError('Invalid retained gallery count')
    natives=[]
    for _ in range(int(count)):
        line=stream.readline(bridge.MAX_PACKED*2+32)
        if len(line)>bridge.MAX_PACKED*2+20 or not line.endswith(b'\n') or not line.startswith(b'TEMPLATE '):
            raise ValueError('Invalid retained gallery template line')
        native=bytes.fromhex(line[9:-1].decode('ascii'))
        if not bridge.packet_valid(native):raise ValueError('Invalid retained gallery template')
        natives.append(native)
    return natives


def serve():
    controller=SessionController();suspended=False;last_maintenance=0.0
    try:
        while True:
            ready,_,_=select.select([sys.stdin.buffer],[],[],1)
            if not ready:
                if not suspended and controller.host.process is not None and time.monotonic()-last_maintenance>=60:
                    try:controller.maintain()
                    except (RuntimeError,ValueError,TimeoutError) as error:
                        controller.host.close();print('Goodix idle calibration deferred: '+str(error),file=sys.stderr,flush=True)
                    last_maintenance=time.monotonic()
                continue
            raw=sys.stdin.buffer.readline(bridge.MAX_PACKED*2+32)
            if not raw:break
            if len(raw)>bridge.MAX_PACKED*2+20 or not raw.endswith(b'\n'):raise ValueError('Invalid retained controller command')
            line=raw.decode('ascii').rstrip('\n')
            if line in ('OPEN','RESUME'):
                suspended=False
                try:controller.maintain()
                except (RuntimeError,ValueError,TimeoutError) as error:
                    controller.host.close();print('Goodix cold calibration deferred: '+str(error),file=sys.stderr,flush=True)
                last_maintenance=time.monotonic();bridge.emit('READY')
            elif line=='SUSPEND':
                controller.host.close();controller.cache.invalidate();suspended=True;bridge.emit('SUSPENDED')
            elif line=='CLOSE':controller.host.close();bridge.emit('CLOSED');break
            elif line=='ENROLL':
                if suspended:raise ValueError('Sensor is suspended')
                controller.perform('enroll');last_maintenance=time.monotonic()
            elif line.startswith('VERIFY ') or line.startswith('VERIFY-RETRY '):
                if suspended:raise ValueError('Sensor is suspended')
                native=bytes.fromhex(line.split(' ',1)[1])
                if not bridge.packet_valid(native):raise ValueError('Invalid retained verification template')
                controller.perform('verify',native);last_maintenance=time.monotonic()
            elif line.startswith('IDENTIFY ') or line.startswith('IDENTIFY-RETRY '):
                if suspended:raise ValueError('Sensor is suspended')
                natives=read_gallery(sys.stdin.buffer,line.split(' ',1)[1])
                controller.perform('identify',natives);last_maintenance=time.monotonic()
            else:raise ValueError('Unknown retained controller command')
    finally:controller.host.close()
