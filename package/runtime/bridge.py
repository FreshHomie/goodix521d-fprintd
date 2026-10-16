#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""fprintd controller: privileged capture, algorithm requests over a private socket."""
from pathlib import Path
import hashlib
import os
import resource
import signal
import struct
import subprocess
import sys
import tempfile
import time
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parent))
from rpc import Client
from contact_reference import load as load_clear_reference
from capture_cache import CaptureCache,epoch as cache_epoch,context as cache_context,validate as validate_cache,record as cache_record,MAX_SECONDS

HERE=Path(__file__).resolve().parent
CAPTURE_LIB=Path('/usr/lib64/goodix521d/capture-lib')
MAX_PACKED=384*1024
VERIFY_FRAME_COUNT=10
VERIFY_MATCHES_REQUIRED=3
MAX_GALLERY=10


class RemoveFingerRequired(RuntimeError):
    pass


def packet_valid(data):
    return (15<=len(data)<=MAX_PACKED and data[0]==0x87 and data[5]==0x86 and data[10]==0x81
            and struct.unpack_from('<I',data,6)[0]==len(data)-10
            and struct.unpack_from('<I',data,1)[0]==zlib.crc32(data[10:]))


def read12(path):
    tokens=' '.join(line.split('#',1)[0] for line in path.read_text().splitlines()).split()
    if tokens[:4]!=['P2','64','80','4095']: raise ValueError('Invalid raw geometry')
    values=list(map(int,tokens[4:]))
    if len(values)!=5120 or any(not 0<=v<=4095 for v in values): raise ValueError('Incomplete raw image')
    return values


def transform(values):
    pixels=[0]*5120
    for p,value in enumerate(values): pixels[((p&63)*80)+(p>>6)]=value
    return struct.pack('<5120H',*pixels)


def emit(value):
    print(value,flush=True)


def stop(signum,frame):
    raise KeyboardInterrupt


class Controller:
    def __init__(self,client=None):
        self.client=client or Client()
        self.cache=CaptureCache()
        self.verification_cache=None
        self.calibration_candidate=None
    def capture(self,folder,stage,attempt):
        started=time.monotonic()
        directory=folder/f'capture-{stage:02d}-{attempt:02d}'
        directory.mkdir(mode=0o700)
        print('Goodix capture: preparing; keep sensor clear',file=sys.stderr,flush=True)
        emit('STATE WAIT')
        if not hasattr(self,'clear_reference'): self.clear_reference=load_clear_reference()
        if self.clear_reference is None: time.sleep(5)
        env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','G_MESSAGES_DEBUG':'all',
             'LD_LIBRARY_PATH':str(CAPTURE_LIB),'GOODIX521D_CAPTURE_PROFILE':'pr81-full',
             'GOODIX521D_SAVE_FRAMES':'1','GOODIX521D_FRAME_DUMP_PREFIX':'capture',
             'GOODIX521D_TIMED_CAPTURE':'1',
             'GOODIX521D_CONTACT_CAPTURE':'1' if getattr(self,'operation',None)=='verify' else '0'}
        if self.clear_reference is not None:
            env['GOODIX521D_CLEAR_REFERENCE']=','.join(map(str,self.clear_reference['reference']))
            env['GOODIX521D_CLEAR_OTP_SHA256']=self.clear_reference['otp_sha256']
            if 'post_reference' in self.clear_reference:
                env['GOODIX521D_POST_CLEAR_REFERENCE']=','.join(map(str,self.clear_reference['post_reference']))
        cache=getattr(self,'enrollment_calibration',None) if getattr(self,'operation',None)=='enroll' else None
        verifying=getattr(self,'operation',None)=='verify'
        if verifying:
            self.calibration_candidate=None
            self.verification_cache=self.cache.load(cache_context(self.clear_reference),cache_epoch()) if self.cache.active and self.clear_reference is not None else None
            if self.verification_cache is not None:
                cache={'background':list(struct.unpack('<5120H',bytes.fromhex(self.verification_cache['background']))),
                       'baseline':self.verification_cache['baseline']}
            else:print('Goodix capture: fresh calibration required',file=sys.stderr,flush=True)
        if cache is not None:
            cache_file=folder/'session-background.bin'
            if not cache_file.exists():
                cache_file.write_bytes(struct.pack('<5120H',*cache['background']));cache_file.chmod(0o400)
            env['GOODIX521D_SESSION_BACKGROUND']=str(cache_file)
            env['GOODIX521D_SESSION_BASELINE']=','.join(map(str,cache['baseline']))
            if verifying:
                env.update(GOODIX521D_SKIP_BACKGROUND_IMAGES='1',GOODIX521D_ALLOW_HELD_CONTACT='1',
                    GOODIX521D_CACHE_DEADLINE_US=str(int((self.verification_cache['epoch']['boottime']+MAX_SECONDS)*1e6)))
                print('Goodix capture: reusing valid clear calibration; already-held contact supported',file=sys.stderr,flush=True)
            else:print('Goodix capture: reusing this enrollment session clear reference',file=sys.stderr,flush=True)
        process=subprocess.Popen(['/usr/bin/timeout','--signal=TERM','--kill-after=3s','50s',
            str(HERE/'goodix-capture-one'),'--capture'],cwd=directory,env=env,stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,text=True,start_new_session=True)
        failure_reason=None
        baseline=None
        skipped=[];startup_contact=None;final_contact=None;observed_baseline=None
        calibration_stamp=None;calibration_context=None;contact_confirmed=False
        try:
            for line in process.stdout:
                if line.startswith('CAPTURE_BASELINE '):
                    baseline=list(map(int,line.split()[1:]))
                    if len(baseline)!=8 or any(not 0<=v<=65535 for v in baseline):
                        raise ValueError('Invalid capture baseline')
                if line.startswith('CACHED_STARTUP SKIPPED_IMAGE '):skipped.append(line.strip().split()[-1])
                if line.startswith('CACHED_STARTUP OBSERVED_BASELINE '):observed_baseline=list(map(int,line.split()[2:]))
                if line.strip() in ('EARLY_STARTUP HELD','EARLY_STARTUP CLEAR'):startup_contact=line.strip().split()[-1]
                if line.strip() in ('EARLY_STARTUP BASELINE_HELD','EARLY_STARTUP BASELINE_CLEAR'):final_contact=line.strip().split()[-1].split('_')[-1]
                if line.strip()=='CLEAR_GUARD WAIT':
                    print('Goodix capture: lift required before calibration',file=sys.stderr,flush=True)
                    emit('STATE REMOVE')
                    if getattr(self,'operation',None)=='verify' and os.environ.get('GOODIX521D_CLEAR_RETRY')!='1':
                        # Complete one remove-finger retry, so PAM can ask for
                        # a lift. The automatically restarted operation waits
                        # for stable clear readings before taking any image.
                        raise RemoveFingerRequired('Lift required before calibration')
                if line.strip()=='CLEAR_GUARD READY':
                    print('Goodix capture: sensor confirmed clear; calibrating',file=sys.stderr,flush=True)
                    emit('STATE WAIT')
                    if verifying and self.verification_cache is None and self.cache.active:
                        calibration_stamp=cache_epoch();calibration_context=cache_context(self.clear_reference)
                if line.strip()=='CONTACT_CAPTURE_READY' or 'Timed capture: image burst starts in 3 seconds' in line:
                    print(f'Goodix capture: finger requested after {time.monotonic()-started:.2f}s',file=sys.stderr,flush=True)
                    emit('STATE PRESS')
                if line.strip()=='CONTACT_CAPTURE_CONFIRMED':
                    contact_confirmed=True
                    print(f'Goodix capture: contact confirmed after {time.monotonic()-started:.2f}s; collecting images',file=sys.stderr,flush=True)
                # Keep only the helper's final error, never raw debug/packet/image
                # output. This distinguishes contact timeout from initialization
                # or USB failures without saving biometric data in the journal.
                if line.startswith(('Capture failed:','Open failed:','Close failed:','Capture exceeded')):
                    failure_reason=' '.join(line.split())[:300]
            status=process.wait()
        finally:
            if process.poll() is None:
                os.killpg(process.pid,signal.SIGINT)
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: os.killpg(process.pid,signal.SIGKILL);process.wait()
            process.stdout.close()
        emit('STATE LIFT')
        if status not in (0,10):
            if self.verification_cache is not None and failure_reason and any(reason in failure_reason for reason in (
                'Early contact ambiguous','Early contact changed or expired','Early-contact cache expired',
                'Cached contact baseline changed or expired','Cached background expired')):
                self.cache.invalidate();raise RemoveFingerRequired('Cached calibration invalid; lift for fresh calibration')
            if failure_reason and any(reason in failure_reason for reason in ('Clear sensor wait timed out','PR81 contact detection timed out','Finger appeared during calibration')):
                raise RemoveFingerRequired(failure_reason)
            raise RuntimeError(f'Reader capture failed (exit {status}, after {time.monotonic()-started:.2f}s): '+(failure_reason or 'no helper error summary'))
        print(f'Goodix capture: raw burst completed after {time.monotonic()-started:.2f}s',file=sys.stderr,flush=True)
        background=read12(directory/'capture-burst-0001-background.pgm')
        raws=[directory/f'capture-burst-0001-raw-{i:02d}.pgm' for i in range(1,11)]
        if len(list(directory.glob('capture-burst-*-raw-*.pgm')))!=10: raise ValueError('Incomplete capture burst')
        frames=[read12(path) for path in raws]
        if self.verification_cache is not None:
            try:
                expected=validate_cache(self.verification_cache,cache_context(self.clear_reference),cache_epoch())
                if (skipped!=['clear-0','clear-1','clear-2','clear-3'] or list(directory.glob('clear-*.pgm')) or
                    startup_contact not in ('CLEAR','HELD') or startup_contact!=final_contact or
                    not contact_confirmed or observed_baseline is None or len(observed_baseline)!=8 or
                    baseline!=self.verification_cache['baseline'] or struct.pack('<5120H',*background)!=expected):
                    raise ValueError('Cached capture trace/background invalid')
                print('Goodix capture: cached startup contact '+startup_contact+' confirmed',file=sys.stderr,flush=True)
            except (ValueError,TypeError,struct.error) as error:
                self.cache.invalidate();raise RemoveFingerRequired('Cached calibration changed during capture; lift for fresh calibration') from error
        elif verifying and calibration_context is not None and baseline is not None and contact_confirmed:
            if read12(directory/'clear-1.pgm')!=background:raise ValueError('Fresh calibration background mismatch')
            self.calibration_candidate=cache_record(background,baseline,calibration_context,calibration_stamp)
        # Same first temporal frame as the validated API; no voting/selection.
        result=self.client.call('preprocess',background=transform(background).hex(),raw=transform(frames[0]).hex())
        self.last_calibration={'background':background,'baseline':baseline} if baseline is not None else None
        return result
    def capture_records(self,folder):
        first=self.capture(folder,1,1)
        directory=folder/'capture-01-01'
        background=transform(read12(directory/'capture-burst-0001-background.pgm')).hex()
        records=[first]
        for number in range(2,VERIFY_FRAME_COUNT+1):
            records.append(self.client.call('preprocess',background=background,
                raw=transform(read12(directory/f'capture-burst-0001-raw-{number:02d}.pgm')).hex()))
        return records
    def enroll(self,folder):
        records=[]
        deadline=time.monotonic()+1200
        for stage in range(1,19):
            for attempt in range(1,13):
                if time.monotonic()>=deadline: raise TimeoutError('Enrollment time limit exceeded')
                try: record=self.capture(folder,stage,attempt)
                except RemoveFingerRequired as error:
                    print(f'Goodix enrollment: stage {stage}, attempt {attempt}, lift retry: {error}',file=sys.stderr,flush=True)
                    emit('RETRY REMOVE');continue
                print(f"Goodix enrollment: stage {stage}, quality {record['quality']}, coverage {record['coverage']}",file=sys.stderr,flush=True)
                if record['quality']<25 or record['coverage']<65:
                    emit('RETRY CENTER' if record['coverage']<65 else 'RETRY');continue
                result=self.client.call('enroll-check',records=records+[record])
                if result['accepted']:
                    if not hasattr(self,'enrollment_calibration') and getattr(self,'clear_reference',None) is not None and getattr(self,'last_calibration',None) is not None:
                        self.enrollment_calibration=self.last_calibration
                    records.append(record);emit(f'PROGRESS {stage}');break
                if result.get('overlap_retry'):
                    print('Goodix enrollment: repeated area; lift and shift slightly while maintaining full contact',file=sys.stderr,flush=True)
                emit('RETRY')
            else: raise RuntimeError('Enrollment stage rejected twelve times')
        result=self.client.call('save',records=records)
        native=bytes.fromhex(result['native'])
        if not packet_valid(native): raise ValueError('Invalid native enrollment template')
        self.client.call('verify',native=native.hex(),records=[])
        emit('TEMPLATE '+native.hex())
    def verify(self,folder,native):
        self.authenticate(folder,[native])
    def identify(self,folder,natives):
        if not 1<=len(natives)<=MAX_GALLERY:raise ValueError('Invalid identification gallery size')
        self.authenticate(folder,natives,identifying=True)
    def authenticate(self,folder,natives,identifying=False):
        # Keep each RPC below its existing limit: send one bounded template
        # at a time, never concatenate a gallery into a worker request.
        for native in natives:self.client.call('verify',native=native.hex(),records=[])
        prefix='IDENTIFY' if identifying else 'MATCH'
        try: records=self.capture_records(folder)
        except RemoveFingerRequired as error:
            self.cache.invalidate()
            print(f'Goodix verification: lift retry: {error}',file=sys.stderr,flush=True)
            emit(prefix+' REMOVE');return
        if len(records)!=VERIFY_FRAME_COUNT: raise ValueError('Incomplete verification burst')
        matched_index=None;learning_records=[];valid_any=False
        for index,native in enumerate(natives):
            matches=0;valid=0;candidate_records=[]
            for number,record in enumerate(records,1):
                # Each frame gets a fresh native child, exactly as in release14.
                decision=self.client.call('verify',native=native.hex(),records=[record])['decisions'][0]
                if decision['status'] not in (0,131): raise RuntimeError('Unexpected matching status')
                matched=decision['status']==0 and decision['matched_index']==0 and decision['decision_value']>0
                matches+=matched;valid+=decision['status']==0
                if matched and record['quality']>=25 and record['coverage']>=65:
                    candidate_records.append(record)
                template=f', template {index+1}/{len(natives)}' if identifying else ''
                print(f"Goodix verification: quality {record['quality']}, coverage {record['coverage']}, "
                      f"status {decision['status']}, result {'retry' if decision['status']==131 else 'match' if matched else 'no-match'}, frame {number}/10{template}",
                      file=sys.stderr,flush=True)
            valid_any|=valid>=VERIFY_MATCHES_REQUIRED
            outcome='1' if matches>=VERIFY_MATCHES_REQUIRED else 'RETRY' if valid<VERIFY_MATCHES_REQUIRED else '0'
            template=f'; template {index+1}/{len(natives)}' if identifying else ''
            print(f'Goodix verification burst: {matches}/10 matching frames; {VERIFY_MATCHES_REQUIRED} required; result {outcome}{template}',file=sys.stderr,flush=True)
            if outcome=='1':
                matched_index=index;learning_records=candidate_records;break
        outcome='1' if matched_index is not None else '0' if valid_any else 'RETRY'
        if hasattr(self,'verify_guard'):
            try:self.verify_guard()
            except RemoveFingerRequired:
                emit(prefix+' REMOVE');return
        if self.verification_cache is not None:
            try:validate_cache(self.verification_cache,self.verification_cache['context'],cache_epoch())
            except ValueError:
                self.cache.invalidate()
                print('Goodix verification: cached calibration expired during matching; lift retry',file=sys.stderr,flush=True)
                emit(prefix+' REMOVE');return
            if cache_context(self.clear_reference)!=self.verification_cache['context']:self.cache.invalidate()
        elif self.calibration_candidate is not None:
            if self.cache.store(self.calibration_candidate,cache_context(self.clear_reference),cache_epoch()):
                print('Goodix capture: clear calibration cached for this daemon/power session',file=sys.stderr,flush=True)
        if identifying:
            print(f'Goodix identification: result {outcome}; matched template {matched_index+1 if matched_index is not None else "none"}/{len(natives)}',file=sys.stderr,flush=True)
        learned=None
        if outcome=='1' and learning_records and getattr(self,'learning_allowed',True):
            # One native-selected update per authenticated physical touch;
            # the10 correlated verification frames must not teach10 times.
            sample=max(learning_records,key=lambda record:(record['quality'],record['coverage']))
            try:
                study=self.client.call('study',native=natives[matched_index].hex(),records=[sample])
                if study.get('updated'):
                    candidate=bytes.fromhex(study['native'])
                    if not packet_valid(candidate): raise ValueError('Invalid learned template')
                    learned=candidate
                print(f"Goodix learning: action {study.get('action',0)}, update {bool(learned)}",file=sys.stderr,flush=True)
            except (RuntimeError,ValueError,KeyError,OSError) as error:
                # A learning/storage failure cannot manufacture a match or
                # invalidate the preceding independent three-vote decision.
                print('Goodix learning skipped: '+str(error),file=sys.stderr,flush=True)
        if hasattr(self,'verify_guard'):
            try:self.verify_guard()
            except RemoveFingerRequired:
                emit(prefix+' REMOVE');return
        if outcome=='1' and hasattr(self,'learning_complete'):self.learning_complete()
        token=str(matched_index) if identifying and outcome=='1' else 'NONE' if identifying and outcome=='0' else outcome
        emit(prefix+' '+token+(' '+learned.hex() if learned is not None else ''))
    def run(self,operation):
        self.operation=operation
        native=None
        if operation=='verify':
            text=sys.stdin.buffer.read(MAX_PACKED*2+2)
            if len(text)>MAX_PACKED*2+1: raise ValueError('Oversized native template')
            native=bytes.fromhex(text.decode('ascii').strip())
            if not packet_valid(native): raise ValueError('Invalid native template packet')
        with self.cache.locked():
            try:
                if operation=='enroll':self.cache.invalidate()
                with tempfile.TemporaryDirectory(prefix='goodix-capture-') as directory:
                    folder=Path(directory)
                    if operation=='enroll': self.enroll(folder)
                    else: self.verify(folder,native)
            except BaseException:
                self.cache.invalidate();raise


def main():
    if len(sys.argv)!=2 or sys.argv[1] not in ('enroll','verify','--session'): raise SystemExit('Expected enroll, verify or --session')
    if os.geteuid()!=0: raise SystemExit('Installed controller requires fprintd privileges')
    os.umask(0o077);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    for sig in (signal.SIGINT,signal.SIGTERM): signal.signal(sig,stop)
    if sys.argv[1]=='--session':
        from capture_session import serve
        serve()
    else:Controller().run(sys.argv[1])

if __name__=='__main__':
    try: main()
    except KeyboardInterrupt: sys.exit(130)
    except Exception as error:
        print('Goodix controller failed: '+str(error),file=sys.stderr,flush=True);sys.exit(1)
