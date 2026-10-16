#!/usr/bin/python3 -I
# SPDX-License-Identifier: MIT
"""Import explicit contact-only evidence and bind it to this sensor's OTP."""
from pathlib import Path
import hashlib
import json
import os
import stat
import subprocess
import sys
import tempfile
sys.path.insert(0,'/usr/libexec/goodix521d')
from contact_reference import DESTINATION,PROFILE,derive,load,validate


def main():
    if os.geteuid()!=0 or len(sys.argv) not in (2,3):
        raise SystemExit('Usage: sudo goodix521d-import-contact-reference CONTACT_REPORT [POST_CLEAR_REPORT]')
    fd=os.open(sys.argv[1],os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW)
    with os.fdopen(fd,'rb') as stream:
        info=os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or not 1<=info.st_size<=128*1024: raise ValueError('Invalid bounded measurement file')
        data=stream.read(128*1024+1)
    values=derive(json.loads(data))
    # This reads firmware/OTP only, then stops before TLS and image calibration.
    result=subprocess.run(['/usr/libexec/goodix521d/goodix-contact-probe','--identity'],
        env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','LD_LIBRARY_PATH':'/usr/lib64/goodix521d/capture-lib',
            'GOODIX521D_CAPTURE_PROFILE':'pr81-full','GOODIX521D_IDENTITY_PROBE':'1'},
        capture_output=True,text=True,timeout=45,check=True)
    hashes=[line.split()[1] for line in result.stdout.splitlines() if line.startswith('CONTACT_IDENTITY ')]
    if len(hashes)!=1 or 'IDENTITY_PROBE_COMPLETE' not in result.stdout.splitlines():
        raise ValueError('Reader identity measurement did not finish')
    reference=validate({'version':1,'profile':PROFILE,'otp_sha256':hashes[0],
        'reference':values,'source_sha256':hashlib.sha256(data).hexdigest()})
    if len(sys.argv)==3:
        fd=os.open(sys.argv[2],os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW)
        with os.fdopen(fd,'rb') as stream:
            info=os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_size>128*1024: raise ValueError('Invalid post-clear report')
            post_bytes=stream.read(128*1024+1)
        post=json.loads(post_bytes)
        if post.get('profile')!='pr81-full-postclear-report-v1' or type(post.get('reports'))!=int or post['reports']<3:
            raise ValueError('Unsupported post-clear measurement')
        reference.update(post_reference=post['reference'],post_source_sha256=hashlib.sha256(post_bytes).hexdigest())
        validate(reference)
    old=load()
    if old==reference: print('Measured contact reference is already installed.');return
    if old is not None and old['otp_sha256']!=reference['otp_sha256']:
        raise ValueError('Existing contact reference belongs to another reader; it was preserved')
    fd,name=tempfile.mkstemp(prefix='.contact-reference-',dir=DESTINATION.parent)
    try:
        with os.fdopen(fd,'w') as stream:
            json.dump(reference,stream,separators=(',',':'));stream.write('\n');stream.flush();os.fsync(stream.fileno())
            os.fchmod(stream.fileno(),0o600)
        os.replace(name,DESTINATION)
    finally:
        if os.path.exists(name): os.unlink(name)
    print('Measured local contact reference installed and bound to the reader OTP.')


if __name__=='__main__': main()
