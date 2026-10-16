/* SPDX-License-Identifier: MIT
 * Type17 secondary candidate rejection/recheck owner56f70, default policy tables.
 */
#include "521d-match-secondary-policy.h"
#include <stddef.h>
#include <string.h>
static int minimum(int a,int b){return a<b?a:b;}
static int maximum(int a,int b){return a>b?a:b;}
int gx521d_match_secondary_policy(const Gx521dMatchSecondaryPolicyInput *in,Gx521dMatchSecondaryPolicy *out)
{
 if(!in||!out||in->accepted<0||in->accepted>1||in->possible<0||in->possible>1||in->retry_counter<-50||in->retry_counter>50||in->retry_rejected<0||in->retry_rejected>1||in->rejection_count<0||in->rejection_count>50||in->probe_records>120)return -1;
 for(unsigned i=0;i<24;i++)if(in->metrics[i]<-4096||in->metrics[i]>65536)return -1;
 const int32_t *m=in->metrics;
 if(m[2]<-4084||m[4]<-4084)return -1;
 if(m[19]<0||m[19]>11||m[20]<0||m[20]>11||m[21]<0||m[21]>11||m[22]<0||m[22]>11||m[23]<0||m[23]>3)return -1;
 Gx521dMatchSecondaryPolicy r={in->accepted,in->possible,in->retry_counter,in->retry_rejected,in->rejection_count};
 if(r.accepted!=1){*out=r;return 0;}
 int max_quality=maximum(m[8],m[9]),min_quality=minimum(m[8],m[9]),coverage=minimum(m[10],m[11]);
 int live=maximum(m[19],m[20]),classes=m[21]+m[22],kind=m[23],score=m[4],mismatch=m[7],mass=m[6];
 int disallowed=(kind>1&&(classes>5||live>1))||(classes>1&&kind==3)||(classes>3&&kind==2)||(classes>6&&kind==1)||max_quality<5||(max_quality<10&&coverage>90);
 int high=215,detail_gate=200,distance_gate=400;
 if(kind>0){
  if(classes>5){detail_gate=215;distance_gate=200;}
  else if(classes>3){detail_gate=210;distance_gate=kind==3?200:300;}
  else if(classes>1){detail_gate=205;distance_gate=kind==3?250:350;if(kind==3)detail_gate=210;}
  else if(classes>0&&kind==3){detail_gate=205;distance_gate=350;}
  if(live>1){detail_gate=maximum(detail_gate,210);distance_gate=minimum(distance_gate,150);}
  if((coverage>80&&in->probe_records<60)||(coverage>90&&(in->probe_records<70||(in->probe_records<80&&classes>0&&kind!=3)))){
   detail_gate=maximum(detail_gate,215);distance_gate=minimum(distance_gate,150);
  }
  if(kind>1&&mass<4000)distance_gate=distance_gate*mass/5120;
  if(mass>4864&&(classes>1||(max_quality<30&&live>90))){detail_gate=maximum(detail_gate,215);distance_gate=minimum(distance_gate,150);}
  if(disallowed||score<detail_gate||mismatch>distance_gate)r.accepted=r.possible=0;
 }else if(disallowed)r.accepted=r.possible=0;
 if((classes>1&&(score<200||mismatch>550))||(classes>3&&(score<205||mismatch>450))||(classes>5&&(score<208||mismatch>350))||(classes>7&&(score<high||mismatch>200)))r.accepted=r.possible=0;
 if(classes>7||mass<1500)r.accepted=r.possible=0;
 if((classes>1&&(score<210||mismatch>350))||(classes>3&&(score<high||mismatch>200)))r.possible=0;
 int recheck=0;
 if(classes>2||(classes>0&&kind>0))recheck=mass<2048||(min_quality<60&&coverage>80);
 if(classes>5)recheck=1;
 if(r.accepted==1&&recheck){
  Gx521dMatchPolicyInput input;memcpy(input.metrics,m,sizeof(input.metrics));input.mode=1;input.counter=2;
  int penalty=4;if(classes>5&&mass<1500)penalty=12;else if(classes>3&&mass<1792)penalty=8;
  input.metrics[2]-=penalty;input.metrics[4]-=penalty;
  Gx521dMatchPolicy decision;if(gx521d_match_policy(&input,&decision))return -1;
  if(decision.possible!=1){r.retry_counter--;r.accepted=r.possible=0;}else r.retry_counter=0;
  if((r.possible==1&&decision.accepted==0)||(kind>0&&classes>1)){r.possible=0;r.rejection_count++;}
 }
 if(!r.accepted)r.retry_rejected=1;
 *out=r;return 0;
}
