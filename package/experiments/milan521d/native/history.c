/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 history update, reviewed against pinned owner RVA 0x71250.
 * The upstream Milan history mechanism is in preprocess/classification.c;
 * this adapter retains its signed-word normalization and actual 521d geometry.
 */
#include "521d-history.h"
#include <string.h>
void gx521d_history_init(Gx521dHistory *history)
{
  if (!history) return;
  memset(history,0,sizeof(*history));history->mask_threshold=60;history->mask_average=60;
}
int gx521d_history_update(Gx521dHistory *history,
    const uint16_t difference[GX521D_PIXELS],int sample_count,const Gx521dMask *mask)
{
  if (!history || !difference || !mask || mask->rows!=GX521D_ROWS ||
      mask->columns!=GX521D_COLUMNS || mask->count<0 || mask->count>GX521D_PIXELS ||
      history->count<0 || history->count>50) return -1;
  if (sample_count<=1) {history->count=0;history->mask_threshold=60;}
  if (mask->count*100>history->mask_threshold*GX521D_PIXELS) {
    history->mask_threshold+=5;
    history->mask_average=(mask->count*100/GX521D_PIXELS+
        history->count*history->mask_average)/(history->count+1);
    if (history->mask_average>80) history->mask_threshold=60;
    if (history->mask_threshold>85) history->mask_threshold=85;
    uint16_t current[GX521D_PIXELS];int32_t sum=0;
    for (int i=0;i<GX521D_PIXELS;i++) {
      int16_t value=(int16_t)((difference[i]-4095)*3);
      current[i]=value>0 ? (uint16_t)value:0;sum+=current[i];
    }
    int mean=sum/GX521D_PIXELS;
    if (mean) for (int i=0;i<GX521D_PIXELS;i++) {
      int normalized=((int)current[i]<<13)/mean;
      history->reference[i]=(uint16_t)((normalized+
          (int16_t)history->reference[i]*history->count)/(history->count+1));
    }
    history->count++;
    for (int i=0;i<GX521D_PIXELS;i++) {
      if (mask->pixels[i]==1 && history->age[i]<50) history->age[i]++;
      else if (!mask->pixels[i] && history->age[i]) history->age[i]--;
    }
  }
  if (history->count>50) history->count=50;
  return 0;
}
