/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Initial type17 extrema; dimension/scale adaptation of pinned Milan extrema.c.
 * Reviewed3.0.21.290 owners: scan0x506b0, neighborhood0x537c0.
 */
#include "521d-feature-candidates.h"
#include <stdlib.h>
#include <string.h>
int gx521d_feature_candidates_from_scales(const uint16_t scales[GX521D_FEATURE_SCALES][GX521D_PIXELS],Gx521dCandidates *out)
{
 if(!scales||!out)return -1;
 memset(out,0,sizeof(*out));
 for(int scale=1;scale<=6;scale++)for(int y=6;y<58;y++)for(int x=6;x<74;x++) {
  int pixel=y*80+x;
  int32_t response=(int16_t)(scales[scale+1][pixel]-scales[scale][pixel]);
  if(response>=-328&&response<=328)continue;
  int extremum=1;
  for(int s=-1;extremum&&s<=1;s++)for(int dy=-1;extremum&&dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
   if(!s&&!dy&&!dx)continue;
   int neighbor=(y+dy)*80+x+dx;
   int32_t other=(int32_t)scales[scale+s+1][neighbor]-scales[scale+s][neighbor];
   if((response>0&&other>response)||(response<0&&other<response)){extremum=0;break;}
  }
  if(extremum)out->points[out->count++]=(Gx521dCandidate){x,y,scale,response};
 }
 return 0;
}
int gx521d_feature_candidates(const uint8_t image[GX521D_PIXELS],Gx521dCandidates *out)
{
 if(!image||!out)return -1;
 Gx521dFeatureScale *space=malloc(sizeof(*space));if(!space)return -1;
 int status=gx521d_feature_scale(image,space);
 if(!status)status=gx521d_feature_candidates_from_scales(space->scales,out);
 free(space);return status;
}
