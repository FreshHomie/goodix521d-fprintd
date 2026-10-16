/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 classifier persistence, independently expressed from the pinned
 * Goodix 3.0.21.290 store/load boundaries RVA 0x6d570/0x6d9f0.
 */
#include "521d-persistence.h"
#include <string.h>
static const uint8_t magic[8]={0xf4,0xf6,0x28,0xf7,0xf4,0x7b,0xcc,0x24};
static void put16(uint8_t *p,uint16_t v) {p[0]=v;p[1]=v>>8;}
static uint16_t get16(const uint8_t *p) {return (uint16_t)(p[0]|p[1]<<8);}
static void put32(uint8_t *p,int32_t v)
{for (int i=0;i<4;i++) p[i]=(uint32_t)v>>(8*i);}
static int32_t get32(const uint8_t *p)
{return (int32_t)((uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);}
int gx521d_classifier_store(const Gx521dClassifierStoredState *s,uint8_t *blob,size_t bytes)
{
  if (!s || !blob || bytes!=GX521D_CLASSIFIER_STORE_BYTES || s->version!=10001) return -1;
  memcpy(blob,magic,8);put32(blob+8,s->version);put32(blob+12,s->scalar24);
  put32(blob+16,s->scalar20);put32(blob+20,s->scalar28);
  for (int i=0;i<GX521D_PIXELS;i++) {
    uint16_t packed=(uint16_t)((s->reference_age[i]<<10)|((s->component_age[i]&15)<<6)|
      ((s->classes[2][i]&3)<<4)|((s->classes[1][i]&3)<<2)|(s->classes[0][i]&3));
    put16(blob+24+i*2,packed);
  }
  uint8_t *small=blob+24+GX521D_PIXELS*2;
  for (int y=0;y<32;y++) for (int x=0;x<40;x++) {
    int p=y*160+x*2;
    int sum=(int16_t)s->reference[p]+(int16_t)s->reference[p+1]+
      (int16_t)s->reference[p+80]+(int16_t)s->reference[p+81];
    put16(small+(y*40+x)*2,(uint16_t)(sum>>2));
  }
  return 0;
}
int gx521d_classifier_load(Gx521dClassifierStoredState *s,const uint8_t *blob,size_t bytes)
{
  if (!s || !blob || bytes!=GX521D_CLASSIFIER_STORE_BYTES || memcmp(blob,magic,8) ||
      get32(blob+8)!=10001) return -1;
  s->version=get32(blob+8);s->scalar24=get32(blob+12);
  s->scalar20=get32(blob+16);s->scalar28=get32(blob+20);
  for (int i=0;i<GX521D_PIXELS;i++) {
    uint16_t v=get16(blob+24+i*2);
    s->classes[0][i]=v&3;s->classes[1][i]=(v>>2)&3;s->classes[2][i]=(v>>4)&3;
    s->component_age[i]=(v>>6)&15;s->reference_age[i]=v>>10;
  }
  const uint8_t *small=blob+24+GX521D_PIXELS*2;
  for (int y=0;y<64;y++) for (int x=0;x<80;x++) {
    int sy=y/2,sx=x/2,dy=(y&1)*128,dx=(x&1)*128;
    int values[4]={0},sum=0,count=0;
    for (int xx=0;xx<2;xx++) for (int yy=0;yy<2;yy++)
      if (sx+xx<40 && sy+yy<32) {
        int value=(int16_t)get16(small+((sy+yy)*40+sx+xx)*2);
        values[xx*2+yy]=value;sum+=value;count++;
      }
    int value;
    if (count==4) value=(values[0]*(256-dy)*(256-dx)+values[1]*dy*(256-dx)+
      values[2]*(256-dy)*dx+values[3]*dy*dx+32768)>>16;
    else value=sum/count;
    s->reference[y*80+x]=(uint16_t)value;
  }
  return 0;
}
