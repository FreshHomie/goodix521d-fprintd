# SPDX-License-Identifier: MIT
"""Local contact calibration data. No fingerprint images or templates."""
from pathlib import Path
import json
import os
import stat
import statistics

PROFILE='pr81-full-preclear-report-v1'
DESTINATION=Path('/var/lib/goodix521d/contact-reference.json')


def derive(report):
    if report.get('mode')!='precalibration-contact-only' or report.get('finished') is not True:
        raise ValueError('A completed contact-only measurement is required')
    if any(report.get(k) is not False for k in ('image_capture','enrollment','matching','firmware_flash')):
        raise ValueError('Unexpected contact measurement mode')
    samples=report.get('samples')
    if not isinstance(samples,list) or len(samples)!=100: raise ValueError('Expected 100 ordered contact samples')
    for n,row in enumerate(samples):
        if row.get('sample')!=n or row.get('phase')!=n//25: raise ValueError('Invalid contact phase order')
        regions=row.get('regions')
        if not isinstance(regions,list) or len(regions)!=8 or any(type(v)!=int or not 0<=v<=65535 for v in regions):
            raise ValueError('Invalid contact values')
    phases=[]
    for p in range(4):
        rows=[r['regions'] for r in samples if r['phase']==p][-15:]
        phases.append([statistics.median(r[i] for r in rows) for i in range(8)])
    if any(abs(phases[1][i]-phases[3][i])>10 for i in range(8)):
        raise ValueError('Clear reference changed during the measurement')
    for held,clear in ((0,1),(2,3)):
        if sum(phases[clear][i]-phases[held][i]>20 for i in range(8))<4:
            raise ValueError('Held finger and clear sensor did not separate')
    reference=[round((phases[1][i]+phases[3][i])/2) for i in range(8)]
    for p in (1,3):
        for row in [r['regions'] for r in samples if r['phase']==p][-15:]:
            if any(abs(row[i]-reference[i])>6 for i in range(8)):
                raise ValueError('Clear samples were unstable')
    return reference


def validate(reference):
    required={'version','profile','otp_sha256','reference','source_sha256'}
    post={'post_reference','post_source_sha256'}
    if set(reference) not in (required,required|post):
        raise ValueError('Unexpected clear reference fields')
    if reference['version']!=1 or reference['profile']!=PROFILE: raise ValueError('Unsupported clear reference format')
    for key in ('otp_sha256','source_sha256')+ (('post_source_sha256',) if 'post_reference' in reference else ()):
        value=reference[key]
        if not isinstance(value,str) or len(value)!=64 or any(c not in '0123456789abcdef' for c in value):
            raise ValueError('Invalid reference hash')
    for key in ('reference',)+ (('post_reference',) if 'post_reference' in reference else ()):
        values=reference[key]
        if not isinstance(values,list) or len(values)!=8 or any(type(v)!=int or not 0<=v<=65535 for v in values):
            raise ValueError('Invalid clear reference geometry')
    return reference


def load(path=DESTINATION):
    for item in path.parents:
        info=item.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid!=0 or info.st_mode&0o022:
            raise ValueError('Clear reference path is not root controlled')
    try: fd=os.open(path,os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW)
    except FileNotFoundError: return None
    with os.fdopen(fd,'rb') as stream:
        info=os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_uid!=0 or info.st_mode&0o022 or info.st_size>4096:
            raise ValueError('Clear reference is not a bounded root-owned file')
        data=stream.read(4097)
    return validate(json.loads(data))
