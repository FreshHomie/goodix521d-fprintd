# SPDX-License-Identifier: MIT
"""Bounded local protocol; filenames and executable commands never cross it."""
import json
import socket
import stat
import struct
from pathlib import Path

LIMIT=1024*1024
SOCKET=Path('/run/goodix521d/worker.sock')
ENGINE_HASH='5995d6565e602416cbe00f171ad4941203e345ccaddb490199274166adac4f5c'

class CaptureRetry(RuntimeError):
    """Validated native input rejection; caller must discard capture/calibration."""


def exact(connection,n):
    data=bytearray()
    while len(data)<n:
        chunk=connection.recv(n-len(data))
        if not chunk: raise ConnectionError('Worker disconnected')
        data.extend(chunk)
    return bytes(data)


def receive(connection):
    n=struct.unpack('!I',exact(connection,4))[0]
    if not 1<=n<=LIMIT: raise ValueError('Oversized worker message')
    value=json.loads(exact(connection,n))
    if not isinstance(value,dict) or value.get('version')!=1: raise ValueError('Invalid worker protocol')
    return value


def send(connection,value):
    data=json.dumps(value,separators=(',',':'),ensure_ascii=True).encode()
    if not 1<=len(data)<=LIMIT: raise ValueError('Oversized worker message')
    connection.sendall(struct.pack('!I',len(data))+data)


class Client:
    def __init__(self,path=SOCKET,development=False):
        self.path=Path(path)
        if not development:
            if self.path!=SOCKET: raise ValueError('Unexpected worker socket')
            for parent in self.path.parents:
                info=parent.lstat()
                if not stat.S_ISDIR(info.st_mode) or info.st_uid!=0 or info.st_mode&0o022:
                    raise ValueError('Worker socket path is not root controlled')
            info=self.path.lstat()
            if not stat.S_ISSOCK(info.st_mode) or info.st_uid!=0 or stat.S_IMODE(info.st_mode)!=0o600:
                raise ValueError('Worker socket is not private and root controlled')
    def call(self,operation,**inputs):
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as connection:
            connection.settimeout(40)
            connection.connect(str(self.path))
            send(connection,{'version':1,'engine_sha256':ENGINE_HASH,'operation':operation,**inputs})
            reply=receive(connection)
        if reply.get('engine_sha256')!=ENGINE_HASH:
            raise RuntimeError('Worker engine identity mismatch')
        if reply.get('ok') is not True:
            if operation=='preprocess' and reply.get('retry')=='clear-calibration':
                raise CaptureRetry('Native input rejected; lift for fresh calibration')
            raise RuntimeError(reply.get('error','Algorithm worker failed'))
        return reply['result']
