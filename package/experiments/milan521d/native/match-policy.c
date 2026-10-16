/* SPDX-License-Identifier: MIT
 * Type17 candidate policy, independently transcribed from RVA61f10.
 * Constant table RVAs d4950,d49a0,d49f0,d4f00,d4f60,d4ee0,d4e30,d4d50. */
#include "521d-match-policy.h"
#include <stddef.h>
static int clamp_index(int x){return x<0?0:x>17?17:x;}
int gx521d_match_policy(const Gx521dMatchPolicyInput *in,Gx521dMatchPolicy *out)
{
 if(!in||!out||in->mode<0||in->mode>2||in->counter<-50||in->counter>50)return -1;
 for(unsigned i=0;i<24;i++)if(in->metrics[i]<-4096||in->metrics[i]>65536)return -1;
 static const int first[18]={256,227,225,224,222,219,216,216,216,216,209,207,202,201,196,196,196,196};
 static const int second[18]={256,230,230,230,225,224,222,222,222,215,214,210,208,201,201,201,201,201};
 static const int topology_limit[18]={0,90,90,100,120,120,180,250,300,350,380,380,380,380,500,500,500,500};
 static const int detailed[18]={256,237,235,235,233,233,231,227,227,223,223,218,218,218,218,218,218,218};
 static const int enhanced[18]={256,235,232,229,227,227,224,218,217,214,214,214,214,214,214,214,214,214};
 const int32_t *m=in->metrics;Gx521dMatchPolicy result={in->counter,0,0};
 int primary=m[0],retained=m[1],gated=m[12],angle=m[13],mass=m[6];
 int minimum_quality=m[9]<m[8]?m[9]:m[8];
 if(gated>retained+7&&angle<36&&minimum_quality>60)gated=primary;
 if(angle>60&&primary>4&&mass>2048)primary+=1+(angle-60)/5;
 int base=m[11]<70&&primary<8?5:4;
 int pi=clamp_index(primary-base),gi=clamp_index(gated-base);
 int original=m[4],score=original,topology=m[7];
 if(retained<6&&minimum_quality>85){
  int penalty=4;if(mass>1500)penalty+=(mass-1500)/512;
  if(mass>2048){if(pi)pi--;if(gi)gi--;}
  score-=penalty;
 }
 if((primary>15||gated>17)&&score>192&&topology<600)goto quality;
 if(score>first[pi]||score>second[gi])goto quality;
 if(m[21]+m[22]>=2||m[9]>=40||primary<=7)goto done;
 if(gated>16)goto quality;
 /* The Windows path uses the gated index against the first table here. */
 if(score<=first[gi]-4)goto done;
quality:
 if(m[17]||m[15])goto done;
 if(primary<=14&&original<=first[pi]+7&&m[11]>=60&&m[10]>=60&&m[9]>=70&&m[8]>=70){
  if(m[11]<=85||topology>=topology_limit[pi])goto done;
 }
 if(topology>=500&&m[9]>=50&&m[8]>=50)goto done;
 result.possible=1;
 if(in->mode!=1||m[9]<=15)goto done;
 int count=m[1],detail=m[3],extra=m[5];
 if(count<7&&mass<1280&&m[9]>70&&m[8]>70){detail-=4;extra-=4;}
 if(minimum_quality>85&&count>14&&angle<30)count-=1+(30-angle)/2;
 int index=clamp_index(count-4);
 if(count<=13&&(count<=4||(detail<=detailed[index]&&extra<=enhanced[index])))goto done;
 int detail_limit=detailed[index]+7,extra_limit=enhanced[index]+7;
 if(detail_limit>238)detail_limit=238;
 if(extra_limit>235)extra_limit=235;
 if(count>15||(count>5&&(detail>detail_limit||extra>extra_limit)))result.counter=0;
 else result.counter--;
 if(!result.counter)result.accepted=1;
done:
 *out=result;return 0;
}
