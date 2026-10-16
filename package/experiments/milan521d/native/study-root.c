/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 graph repair93e00. Forest traversal matches the existing native
 * template export, adapted from Milan relations.c (C)2026 contributors. */
#include "521d-study-root.h"
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
static unsigned slot(unsigned a,unsigned b){unsigned high=a>b?a:b,low=a>b?b:a;return 1+high*(high-1)/2+low;}
int gx521d_study_root(Gx521dTemplate *t)
{
 if(!t)return -1;
 unsigned n=t->stored.header[4];if(!n||n>50||t->stored.header[1]!=17)return -1;
 for(unsigned i=0;i<n;i++)if(word(t->features[i].scalar_header+0x114)!=(int32_t)(i?1+i*(i-1)/2:0))return -1;
 int32_t component[50],counts[50]={0};for(unsigned i=0;i<50;i++)component[i]=-1;
 for(unsigned root=0;root<n;root++){
  if(component[root]>=0)continue;
  component[root]=(int32_t)root;unsigned work[50],used=1;work[0]=root;
  while(used){
   unsigned current=work[--used];
   for(unsigned candidate=n;candidate-->0;)if(candidate!=current&&component[candidate]<0&&t->relations[slot(current,candidate)][0]>2){component[candidate]=(int32_t)root;work[used++]=candidate;}
  }
  for(unsigned candidate=0;candidate<n;candidate++)if(candidate!=root&&component[candidate]<0&&t->relations[slot(root,candidate)][0]>-1)component[candidate]=(int32_t)root;
 }
 for(unsigned i=0;i<n;i++)counts[component[i]]++;
 int32_t selected=-1,maximum=0;for(unsigned i=0;i<n;i++)if(counts[i]>maximum){maximum=counts[i];selected=(int32_t)i;}
 t->stored.graph[0]=-1;t->stored.graph[3]=0;
 if(maximum>1){t->stored.graph[0]=selected;t->stored.graph[3]=1;}
 for(unsigned i=0;i<n;i++){
  int32_t active=maximum>1&&component[i]==selected;put(t->features[i].scalar_header+0x110,active);t->stored.features[i].scalars[0]=(uint32_t)active;
 }
 return 0;
}
