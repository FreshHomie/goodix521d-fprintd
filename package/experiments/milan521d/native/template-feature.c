/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 feature serialization, recovered from reference RVA6c6d0.
 * This layout differs from upstream type12 descriptor serialization. */
#include "521d-template-feature.h"
#include <string.h>
static uint32_t read32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write32(uint8_t *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint8_t *scalar(uint8_t *p,uint8_t tag,uint32_t v)
{*p++=tag;write32(p,v);return p+4;}
static uint8_t *bitmap(uint8_t *p,uint8_t tag,const uint8_t bits[640])
{
 *p++=tag;write32(p,665);p+=4;
 p=scalar(p,0xc1,80);p=scalar(p,0xc2,64);
 p=scalar(p,0xc3,UINT32_MAX);p=scalar(p,0xc4,8);
 *p++=0xc5;write32(p,640);p+=4;memcpy(p,bits,640);return p+640;
}
size_t gx521d_template_feature_pack(const Gx521dFeatureObject *f,uint8_t *dst,size_t capacity)
{
 if(!f||!dst||read32(f->scalar_header)!=80||read32(f->scalar_header+4)!=64)return 0;
 static const unsigned pointers[]={8,16,24,32,0xf8,0x100,0x140,0x158};
 for(unsigned i=0;i<sizeof(pointers)/sizeof(pointers[0]);i++)
  for(unsigned j=0;j<8;j++)if(f->scalar_header[pointers[i]+j])return 0;
 uint32_t count=read32(f->scalar_header+0xf0),extra=read32(f->scalar_header+0x150);
 if(count>GX521D_RECORDS_MAX||read32(f->scalar_header+0x108))return 0;
 size_t size=5+2*670+45+5+5+32*count+55+(extra?5:0);
 if(capacity<size)return 0;
 Gx521dStoredFeature stored={0};stored.count=count;stored.embedded_tag=extra;
 memcpy(stored.high,f->high,640);memcpy(stored.low,f->low,640);
 memcpy(stored.coarse_mask,f->scalar_header+0x28,40);
 for(uint32_t i=0;i<count;i++){
  const GoodixMilanFeatureRecord *r=&f->records[i];
  int angle=r->orientation;
  uint8_t packed_angle=(uint8_t)(angle<0?((-angle)>>8)+128:angle>>8);
  uint32_t position=((((uint32_t)(uint16_t)r->refined_x<<12)|(uint16_t)r->refined_y)<<4)|packed_angle;
  uint8_t *p=stored.records[i];
  write32(p,position);memcpy(p+4,r->payload+4,24);
  uint32_t tail=(read32(r->payload)<<16)+(uint32_t)(int32_t)(int16_t)r->foreground;
  write32(p+28,tail);
 }
 static const unsigned offsets[]={0x110,0x114,0x118,0x11c,0x120,0x124,0x128,0x12c,0x130,0x134,0x14c};
 for(unsigned i=0;i<11;i++)stored.scalars[i]=read32(f->scalar_header+offsets[i]);
 return gx521d_stored_feature_pack(&stored,dst,capacity);
}
static const uint8_t scalar_tags[]={0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xc0};
size_t gx521d_stored_feature_pack(const Gx521dStoredFeature *f,uint8_t *dst,size_t capacity)
{
 if(!f||!dst||f->count>GX521D_RECORDS_MAX)return 0;
 size_t size=1455+32*f->count+(f->embedded_tag?5:0);
 if(capacity<size)return 0;
 uint8_t *p=dst;*p++=0x95;write32(p,(uint32_t)size-5);p+=4;
 p=bitmap(p,0xb2,f->high);p=bitmap(p,0xcf,f->low);
 *p++=0xce;write32(p,40);p+=4;memcpy(p,f->coarse_mask,40);p+=40;
 p=scalar(p,0xb3,f->count);*p++=0xb4;write32(p,f->count*32);p+=4;
 memcpy(p,f->records,f->count*32);p+=f->count*32;
 for(unsigned i=0;i<11;i++)p=scalar(p,scalar_tags[i],f->scalars[i]);
 if(f->embedded_tag)p=scalar(p,0xc7,f->embedded_tag);
 return (size_t)(p-dst);
}
typedef struct {const uint8_t *p;size_t left;} Reader;
static int scalar_read(Reader *r,uint8_t tag,uint32_t *value)
{
 if(r->left<5||r->p[0]!=tag)return 0;
 *value=read32(r->p+1);r->p+=5;r->left-=5;return 1;
}
static int block_read(Reader *r,uint8_t tag,uint32_t size,const uint8_t **bytes)
{
 uint32_t length;
 if(!scalar_read(r,tag,&length)||length!=size||r->left<size)return 0;
 *bytes=r->p;r->p+=size;r->left-=size;return 1;
}
static int bitmap_read(Reader *r,uint8_t tag,uint8_t *bits)
{
 const uint8_t *data;uint32_t value;
 if(!block_read(r,tag,665,&data))return 0;
 Reader inner={data,665};
 static const uint8_t tags[]={0xc1,0xc2,0xc3,0xc4};
 static const uint32_t values[]={80,64,UINT32_MAX,8};
 for(unsigned i=0;i<4;i++)if(!scalar_read(&inner,tags[i],&value)||value!=values[i])return 0;
 if(!block_read(&inner,0xc5,640,&data)||inner.left)return 0;
 memcpy(bits,data,640);return 1;
}
int gx521d_stored_feature_read(const uint8_t *src,size_t length,Gx521dStoredFeature *dst)
{
 if(!src||!dst||length<1455||length>GX521D_PACKED_FEATURE_MAX)return 0;
 Reader r={src,length};uint32_t declared;const uint8_t *bytes;
 if(!scalar_read(&r,0x95,&declared)||declared!=r.left)return 0;
 Gx521dStoredFeature out={0};
 if(!bitmap_read(&r,0xb2,out.high)||!bitmap_read(&r,0xcf,out.low))return 0;
 if(!block_read(&r,0xce,40,&bytes))return 0;
 memcpy(out.coarse_mask,bytes,40);
 if(!scalar_read(&r,0xb3,&out.count)||out.count>GX521D_RECORDS_MAX)return 0;
 if(!block_read(&r,0xb4,out.count*32,&bytes))return 0;
 memcpy(out.records,bytes,out.count*32);
 for(unsigned i=0;i<11;i++)if(!scalar_read(&r,scalar_tags[i],&out.scalars[i]))return 0;
 if(r.left&&(!scalar_read(&r,0xc7,&out.embedded_tag)||!out.embedded_tag))return 0;
 if(r.left)return 0;
 *dst=out;return 1;
}
