/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 calibration exports independently expressed from3.0.21.290
 * RVAs0x4c500/0x4c820. CRC polynomial is standard reflected CRC32. */
#include "521d-calidata.h"
#include <string.h>
static const char version[]="Preprocess_v_1.01.01";
static void put32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put_words(uint8_t *p,const uint16_t *v){for(int i=0;i<GX521D_PIXELS;i++){p[i*2]=(uint8_t)v[i];p[i*2+1]=(uint8_t)(v[i]>>8);}}
static void get_words(uint16_t *v,const uint8_t *p){for(int i=0;i<GX521D_PIXELS;i++)v[i]=(uint16_t)(p[i*2]|p[i*2+1]<<8);}
static uint32_t crc(const uint8_t *p,size_t bytes)
{
  uint32_t value=0xffffffffU;
  for(size_t i=0;i<bytes;i++) {
    value^=p[i];for(int bit=0;bit<8;bit++)value=(value>>1)^((value&1)?0xedb88320U:0);
  }
  return value;
}
int gx521d_calidata_store(const Gx521dCalidata *s,uint8_t *b,size_t bytes)
{
  if(!s || !b || bytes<GX521D_CALIDATA_BYTES)return 0x81;
  put_words(b+8,s->calibration);put_words(b+0x9928,s->setup);
  memcpy(b+0x13248,s->coefficients,sizeof(s->coefficients));
  memcpy(b+0x13a48,s->parameters,sizeof(s->parameters));
  put32(b+0x18488,(uint32_t)s->sample_count);
  memcpy(b+0x1848c,s->classifier,sizeof(s->classifier));put32(b+0x2248c,(uint32_t)s->retained_flag);
  memset(b+0x22490,0,32);memcpy(b+0x22490,version,sizeof(version)-1);
  put32(b,crc(b+8,GX521D_PIXELS*2));put32(b+4,crc(b+0x9928,GX521D_PIXELS*2));return 0;
}
int gx521d_calidata_load(Gx521dCalidata *s,const uint8_t *b,size_t bytes)
{
  if(!s || !b || bytes<GX521D_CALIDATA_BYTES)return 0x81;
  if(memcmp(b+0x22490,version,sizeof(version)-1) || get32(b)!=crc(b+8,GX521D_PIXELS*2) ||
     get32(b+4)!=crc(b+0x9928,GX521D_PIXELS*2))return 0x80;
  get_words(s->calibration,b+8);get_words(s->setup,b+0x9928);
  memcpy(s->coefficients,b+0x13248,sizeof(s->coefficients));memcpy(s->parameters,b+0x13a48,sizeof(s->parameters));
  s->sample_count=(int32_t)get32(b+0x18488);memcpy(s->classifier,b+0x1848c,sizeof(s->classifier));
  s->retained_flag=(int32_t)get32(b+0x2248c);return 0;
}
