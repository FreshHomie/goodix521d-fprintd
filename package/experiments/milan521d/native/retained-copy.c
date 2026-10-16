/* SPDX-License-Identifier: LGPL-2.1-or-later; type17 retained-copy65f90. */
#include "521d-retained-copy.h"
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
int gx521d_retained_copy(const Gx521dFeatureObject *source,Gx521dFeatureObject *destination)
{
 if(!source||!destination)return -1;
 const uint8_t *h=source->scalar_header;
 if(word(h)!=80||word(h+4)!=64||word(h+0xf0)<0||word(h+0xf0)>120||word(h+0x118)<0||word(h+0x118)>word(h+0xf0)||word(h+0x108))return -1;
 static const unsigned pointers[8]={8,16,24,32,0xf8,0x100,0x140,0x158};
 for(unsigned i=0;i<8;i++)for(unsigned j=0;j<8;j++)if(h[pointers[i]+j]||destination->scalar_header[pointers[i]+j])return -1;
 Gx521dFeatureObject out=*destination;
 memcpy(out.scalar_header,h,8);memcpy(out.high,source->high,640);memcpy(out.low,source->low,640);memcpy(out.packed_mask,source->packed_mask,640);
 memset(out.validity,0,sizeof(out.validity));memcpy(out.scalar_header+40,h+40,200);
 memcpy(out.records,source->records,(size_t)word(h+0xf0)*sizeof(*out.records));
 unsigned fields[6]={0xf0,0x118,0x11c,0x120,0x14c,0x150};
 for(unsigned i=0;i<6;i++)put(out.scalar_header+fields[i],word(h+fields[i]));
 put(out.scalar_header+0x124,-1);memset(out.scalar_header+0x128,0,8);put(out.scalar_header+0x138,0);put(out.scalar_header+0x148,0);memset(out.scalar_header+0x160,0,8);
 *destination=out;return 0;
}
