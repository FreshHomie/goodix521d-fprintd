# SPDX-License-Identifier: MIT
"""Native preprocessing, enrollment and persistent matching worker API."""
import collections,ctypes as C,hashlib,struct,time
from native_rpc import ENGINE_HASH
MAX=384*1024

def data(value,length=None):
 if not isinstance(value,str) or len(value)>MAX*2 or len(value)%2:raise ValueError('Invalid bounded data')
 b=bytes.fromhex(value)
 if length is not None and len(b)!=length:raise ValueError('Invalid geometry')
 return b

def buffer(b):return C.create_string_buffer(b,len(b))

class Engine:
 def __init__(self,library):
  self.identity=hashlib.sha256(library.read_bytes()).hexdigest()
  if self.identity!=ENGINE_HASH:raise ValueError('Changed native library')
  self.lib=C.CDLL(str(library));self.owners=collections.OrderedDict();self.last_use=time.monotonic();self.sleep_epoch=self.epoch()
  self.lib.gx_worker_load.argtypes=[C.c_void_p,C.c_size_t];self.lib.gx_worker_load.restype=C.c_void_p
  self.lib.gx_worker_delete.argtypes=[C.c_void_p];self.lib.gx_worker_delete.restype=None
  self.lib.gx_worker_export.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t];self.lib.gx_worker_export.restype=C.c_size_t
  self.lib.gx_worker_identify.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_void_p];self.lib.gx_worker_identify.restype=C.c_int
  self.lib.gx_worker_enroll.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_void_p,C.c_size_t,C.c_void_p];self.lib.gx_worker_enroll.restype=C.c_int
  self.lib.gx_worker_preprocess.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p];self.lib.gx_worker_preprocess.restype=C.c_int
 def epoch(self):
  return time.clock_gettime(time.CLOCK_BOOTTIME)-time.monotonic()
 def expire(self):
  now=time.monotonic();epoch=self.epoch()
  if now-self.last_use>120 or abs(epoch-self.sleep_epoch)>1:self.close()
  self.sleep_epoch=epoch
 def close(self):
  for owner in self.owners.values():self.lib.gx_worker_delete(owner)
  self.owners.clear()
 def export(self,owner):
  output=C.create_string_buffer(MAX);size=self.lib.gx_worker_export(owner,output,MAX)
  if not 15<=size<=MAX:raise RuntimeError('Native template export failed')
  return output.raw[:size]
 def owner(self,packet):
  key=hashlib.sha256(packet).hexdigest()
  if key not in self.owners:
   owner=self.lib.gx_worker_load(buffer(packet),len(packet))
   if not owner:raise ValueError('Invalid native template')
   self.owners[key]=owner
   while len(self.owners)>16:self.lib.gx_worker_delete(self.owners.popitem(last=False)[1])
  self.owners.move_to_end(key);return key,self.owners[key]
 def execute(self,request):
  self.expire();self.last_use=time.monotonic()
  op=request.get('operation');fields={'preprocess':{'raw','background'},'enroll-check':{'records'},'save':{'records'},'verify':{'records','native'},'study':{'records','native'}}
  if op not in fields or set(request)!=fields[op]|{'version','engine_sha256','operation'}:raise ValueError('Invalid operation/fields')
  if request['version']!=1 or request['engine_sha256']!=self.identity:raise ValueError('Wrong native engine identity')
  if op=='preprocess':
   background=data(request['background'],10240);raw=data(request['raw'],10240)
   output=C.create_string_buffer(5120);result=(C.c_int32*3)()
   if self.lib.gx_worker_preprocess(buffer(background),buffer(raw),output,result) or result[0]:raise RuntimeError('Native preprocessing rejected frame')
   return {'pixels':output.raw.hex(),'quality':result[1],'coverage':result[2]}
  records=request['records'];verifying=op in ('verify','study')
  if not isinstance(records,list) or not 0<=len(records)<=18 or op!='verify' and len(records)==0 or verifying and len(records)>1 or op=='save' and len(records)!=18:raise ValueError('Invalid record count')
  normalized=[]
  for r in records:
   if not isinstance(r,dict) or set(r)!={'pixels','quality','coverage'}:raise ValueError('Invalid image descriptor')
   q,cov=r['quality'],r['coverage']
   if type(q)!=int or type(cov)!=int or not 0<=q<=100 or not 0<=cov<=100:raise ValueError('Invalid quality/coverage')
   normalized.append((data(r['pixels'],5120),q,cov))
  if not verifying:
   encoded=b''.join(struct.pack('<2i',q,cov)+pixels for pixels,q,cov in normalized);result=(C.c_int32*4)();out=C.create_string_buffer(MAX)
   size=self.lib.gx_worker_enroll(buffer(encoded),len(normalized),1,out,MAX,result)
   if size<0:raise RuntimeError('Native enrollment failed')
   accepted=result[0]==0 and result[1]==len(records)
   if op=='enroll-check':return {'accepted':accepted,'progress':result[2],'overlap_retry':bool(result[3])}
   if not accepted or size==0:raise RuntimeError('Enrollment is incomplete')
   return {'native':out.raw[:size].hex()}
  packet=data(request['native']);key,owner=self.owner(packet);before=self.export(owner)
  if not normalized:return {'decisions':[],'repack_byte_identical':before==packet,'owned_template_deleted':False}
  pixels,q,cov=normalized[0];result=(C.c_int32*4)()
  if self.lib.gx_worker_identify(owner,buffer(pixels),q,cov,int(op=='study'),result):raise RuntimeError('Native identification failed')
  status,decision,action,selected=result;positive=status==0 and decision>0
  answer={'decisions':[{'phase':'identify','sample':0,'kind':1,'status':status,'matched_index':0 if positive else -1,'decision_value':decision,'coverage':cov,'quality':q}],
          'repack_byte_identical':before==packet,'owned_template_deleted':False}
  if op=='study':
   updated=positive and action>0;answer.update(updated=updated,action=action)
   if updated:
    changed=self.export(owner);answer['native']=changed.hex();newkey=hashlib.sha256(changed).hexdigest()
    del self.owners[key]
    old=self.owners.pop(newkey,None)
    if old and old!=owner:self.lib.gx_worker_delete(old)
    self.owners[newkey]=owner
  return answer
