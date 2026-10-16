# SPDX-License-Identifier: MIT
"""Fedora first-time setup. No matching or enrollment is performed here."""
from contextlib import contextmanager,redirect_stderr
from pathlib import Path
import hashlib,json,os,select,signal,stat,statistics,subprocess,tempfile,time,urllib.request

from contact_reference import PROFILE,derive,validate

ENGINE_HASH='5995d6565e602416cbe00f171ad4941203e345ccaddb490199274166adac4f5c'
CAB_HASH='261dee2257858c5957db89377e4b0cde210fe392f92b7ba7c641f9d899787172'
CAB_URL='https://catalog.s.download.windowsupdate.com/c/msdownload/update/driver/drvs/2022/11/e9705005-5bc1-43e9-b5dc-1b8e4ccf7c45_0bd5b680fb66cacfd4ce9ad37542e688f0b0f930.cab'
CATALOG_URL='https://www.catalog.update.microsoft.com/ScopedViewInline.aspx?updateid=727f2acb-078a-41fa-bdc2-78266da4a44c'
RUNTIME=Path('/usr/libexec/goodix521d')
STATE=Path('/var/lib/goodix521d')


def run(command,**kwargs):
    kwargs.setdefault('env',{'PATH':'/usr/bin:/bin','LANG':'C.UTF-8'})
    return subprocess.run(command,check=True,text=True,timeout=60,**kwargs)


def trusted_file(path,limit,owner=0):
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_CLOEXEC)
    with os.fdopen(fd,'rb') as stream:
        info=os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_uid!=owner or info.st_mode&0o022 or not 1<=info.st_size<=limit:
            raise ValueError('Unexpected permissions or size: '+str(path))
        return stream.read(limit+1)


def download_engine(folder,opener=urllib.request.urlopen,extractor='/usr/bin/cabextract'):
    """Retrieve only the pinned original CAB; validate before extracting it."""
    cab=folder/'driver.cab'
    with opener(CAB_URL,timeout=30) as response,cab.open('xb') as output:
        if not response.geturl().startswith('https://'):raise ValueError('Non-HTTPS driver download')
        remaining=2*1024*1024
        while True:
            block=response.read(min(65536,remaining+1))
            if not block:break
            remaining-=len(block)
            if remaining<0:raise ValueError('Driver download exceeded its size limit')
            output.write(block)
    if hashlib.sha256(cab.read_bytes()).hexdigest()!=CAB_HASH:raise ValueError('Microsoft driver archive hash mismatch')
    extracted=folder/'engine';extracted.mkdir(mode=0o700)
    run([extractor,'-q','-F','GoodixEngineAdapter.dll','-d',str(extracted),str(cab)],capture_output=True)
    path=extracted/'GoodixEngineAdapter.dll'
    if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=ENGINE_HASH:
        raise ValueError('Extracted matching engine hash mismatch')
    return path


def probe_environment(identity=False):
    return {'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','LD_LIBRARY_PATH':'/usr/lib64/goodix521d/capture-lib',
            'GOODIX521D_CAPTURE_PROFILE':'pr81-full',
            'GOODIX521D_IDENTITY_PROBE' if identity else 'GOODIX521D_CONTACT_PROBE':'1'}


def identity():
    try:result=run([str(RUNTIME/'goodix-contact-probe'),'--identity'],env=probe_environment(True),capture_output=True)
    except subprocess.CalledProcessError as error:
        details=' '.join((error.stderr or error.stdout or '').split())[-300:]
        raise ValueError('Reader initialization failed; this build requires firmware10019. '+details) from error
    lines=result.stdout.splitlines();values=[s.split()[1] for s in lines if s.startswith('CONTACT_IDENTITY ')]
    if len(values)!=1 or 'IDENTITY_PROBE_COMPLETE' not in lines:raise ValueError('Reader identity probe did not finish')
    if len(values[0])!=64 or any(s not in '0123456789abcdef' for s in values[0]):raise ValueError('Invalid reader identity')
    return values[0]


def collect_contact(lines,announce):
    samples=[];complete=False
    for line in lines:
        if line.startswith('CONTACT_PROBE '):
            row=list(map(int,line.split()[1:]))
            if len(row)!=10:raise ValueError('Malformed contact report')
            number,phase,*regions=row
            if number!=len(samples) or phase!=number//25 or not 0<=number<100 or any(not 0<=v<=65535 for v in regions):
                raise ValueError('Incomplete or unordered contact report')
            if number%25==0:announce(phase)
            samples.append({'sample':number,'phase':phase,'regions':regions})
        if line.strip()=='CONTACT_PROBE_COMPLETE':complete=True
    report={'mode':'precalibration-contact-only','finished':complete,'image_capture':False,
            'enrollment':False,'matching':False,'firmware_flash':False,'samples':samples}
    derive(report)
    return report


def measure_contact():
    print('\nThis measures finger contact; it does not enroll a fingerprint.',flush=True)
    print('\033[1;32m>>> PRESS YOUR RIGHT INDEX AND HOLD STILL <<<\033[0m',flush=True)
    input('With your finger already DOWN, press Enter: ')
    messages=('KEEP HOLDING STILL','\033[1;31m>>> LIFT COMPLETELY — KEEP THE SENSOR CLEAR <<<\033[0m',
              '\033[1;32m>>> PRESS THE SAME RIGHT INDEX AGAIN — HOLD STILL <<<\033[0m',
              '\033[1;31m>>> LIFT COMPLETELY — KEEP THE SENSOR CLEAR <<<\033[0m')
    process=subprocess.Popen([str(RUNTIME/'goodix-contact-probe')],env=probe_environment(),
        stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
    deadline=time.monotonic()+45;pending=bytearray()
    def lines():
        while True:
            if b'\n' in pending:
                line,_,rest=pending.partition(b'\n');pending[:]=rest
                yield line.decode('utf-8');continue
            remaining=deadline-time.monotonic()
            if remaining<=0:raise TimeoutError('Contact setup timed out')
            ready,_,_=select.select([process.stdout],[],[],remaining)
            if not ready:raise TimeoutError('Contact setup timed out')
            block=os.read(process.stdout.fileno(),4096)
            if not block:break
            pending.extend(block)
            if len(pending)>16384:raise ValueError('Oversized contact probe output')
    try:
        report=collect_contact(lines(),lambda phase:print(messages[phase]+'\nWait for the next prompt (about six seconds).',flush=True))
        if process.wait(timeout=3)!=0:raise ValueError('Contact setup failed; repeat with steady touches and full lifts')
        return report
    finally:
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=3)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        process.stdout.close()


def post_reference(reference,folder,host_factory=None):
    """Three guarded clear-only calibrations. Temporary blank frames are deleted."""
    if host_factory is None:
        from capture_session import CaptureHost
        host_factory=CaptureHost
    host=host_factory(reference);baselines=[]
    with open(os.devnull,'w') as quiet,redirect_stderr(quiet):
        try:
            for n in range(3):
                path=folder/str(n);path.mkdir(mode=0o700)
                result=host.capture(path,'calibrate',events=False)
                row=result['baseline']
                if not isinstance(row,list) or len(row)!=8 or any(type(v)!=int or not 0<=v<=65535 for v in row):
                    raise ValueError('Invalid clear calibration report')
                baselines.append(row)
        finally:host.close()
    values=[round(statistics.median(row[i] for row in baselines)) for i in range(8)]
    if any(abs(row[i]-values[i])>6 for row in baselines for i in range(8)):
        raise ValueError('Clear readings changed; keep all fingers off and repeat setup')
    return {'profile':'pr81-full-postclear-report-v1','reports':3,'reference':values}


def save_reference(report,otp,post,destination):
    raw=json.dumps(report,sort_keys=True,separators=(',',':')).encode()
    post_raw=json.dumps(post,sort_keys=True,separators=(',',':')).encode()
    value=validate({'version':1,'profile':PROFILE,'otp_sha256':otp,'reference':derive(report),
        'source_sha256':hashlib.sha256(raw).hexdigest(),'post_reference':post['reference'],
        'post_source_sha256':hashlib.sha256(post_raw).hexdigest()})
    fd,name=tempfile.mkstemp(prefix='.setup-reference-',dir=destination.parent)
    try:
        with os.fdopen(fd,'w') as stream:
            json.dump(value,stream,separators=(',',':'));stream.write('\n');stream.flush();os.fsync(stream.fileno())
            os.fchmod(stream.fileno(),0o600)
        os.replace(name,destination)
    finally:
        if os.path.exists(name):os.unlink(name)
    return value


@contextmanager
def exclusive_reader(runner=None):
    if runner is None:runner=run
    load=runner(['systemctl','show','fprintd.service','-p','LoadState','--value'],capture_output=True).stdout.strip()
    active=runner(['systemctl','show','fprintd.service','-p','ActiveState','--value'],capture_output=True).stdout.strip()
    if load=='masked':raise RuntimeError('fprintd is masked. Unmask it explicitly before configuring normal fingerprint use.')
    masked=False
    try:
        runner(['systemctl','mask','--runtime','fprintd.service']);masked=True
        runner(['systemctl','stop','fprintd.service'])
        yield
    finally:
        if masked:runner(['systemctl','unmask','--runtime','fprintd.service'])
        if active in ('active','activating'):runner(['systemctl','start','fprintd.service'])


def configure_authentication(runner=None):
    if runner is None:runner=run
    # Never choose/replace a profile or force over locally edited PAM files.
    runner(['authselect','check'],capture_output=True)
    current=runner(['authselect','current','--raw'],capture_output=True).stdout.split()
    if not current:raise RuntimeError('No managed authselect profile is selected')
    if 'with-fingerprint' in current[1:]:return 'already-enabled'
    features=runner(['authselect','list-features',current[0]],capture_output=True).stdout.split()
    if 'with-fingerprint' not in features:raise RuntimeError('The current authselect profile has no fingerprint feature')
    runner(['authselect','enable-feature','with-fingerprint','-b'])
    return 'enabled'
