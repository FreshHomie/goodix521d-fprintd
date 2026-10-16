/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Copyright (C) 2026 goodix-fp-linux-dev contributors (included source).
 * Local type17 adapter reuses LGPL erosion and fixed-point filtering kernels.
 */
#ifndef MILAN_UPSTREAM_CLASSIFICATION
#error Define the pinned upstream classification source path
#endif
#include MILAN_UPSTREAM_CLASSIFICATION
#include "521d-classification.h"

static int histogram_groups(const uint16_t source[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS],int maximum,int minimum,int groups,
    Gx521dClassificationHistogram *out)
{
  if (!source || !valid || !out || minimum>maximum || groups<1 || groups>20) return -1;
  memset(out,0,sizeof(*out));
  int low=0x7fffff00,high=0,accepted=0;
  for (size_t i=0;i<GX521D_PIXELS;i++) {
    if (!valid[i]) continue;
    out->statistics[4]++;
    int value=(int16_t)source[i];
    if (value>maximum) out->statistics[2]++;
    else if (value<minimum) out->statistics[3]++;
    else {
      if (value>high) high=value;
      if (value<low) low=value;
      accepted++;
    }
  }
  int range=high-low;out->statistics[5]=accepted;
  if (range<=0) {
    out->statistics[1]=low<5000 ? low:5000;
    out->status=(int32_t)0x80000003U;
    return 0;
  }
  for (size_t i=0;i<GX521D_PIXELS;i++) {
    int value=(int16_t)source[i];
    if (!valid[i] || value<minimum || value>maximum) continue;
    int scaled=(value-low)*399,bin=scaled/range,remainder=scaled-bin*range;
    out->bins[bin]+=range-remainder;
    if (bin<399) out->bins[bin+1]+=remainder;
  }
  /* 0x71ed0 crosses at most one target per bin iteration, including plateaus.
   * Its two-bin lookahead and later monotonic repair differ from a usual CDF. */
  int64_t cumulative=0,total=(int64_t)accepted*range;
  int found=0;
  for (int bin=0;bin<399 && found<groups;bin++) {
    cumulative+=out->bins[bin];
    int64_t target=(int64_t)(found+1)*(100/groups)*total/100;
    if (cumulative>=target) out->quantiles[found++]=bin>0 ? bin-1:0;
    else if (cumulative+out->bins[bin+1]>=target) out->quantiles[found++]=bin;
  }
  for (int i=1;i<groups;i++)
    if (out->quantiles[i]<out->quantiles[i-1]) out->quantiles[i]=out->quantiles[i-1];
  out->statistics[0]=range;out->statistics[1]=low;
  return 0;
}

int gx521d_classification_histogram(const uint16_t source[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS],int maximum,int minimum,
    Gx521dClassificationHistogram *out)
{
  return histogram_groups(source,valid,maximum,minimum,20,out);
}

int gx521d_classification_prepare(const uint16_t live[GX521D_PIXELS],
    const uint16_t setup[GX521D_PIXELS],const Gx521dMask *mask,
    Gx521dClassificationPrepared *out)
{
  if (!live || !setup || !mask || !out || mask->rows!=GX521D_ROWS ||
      mask->columns!=GX521D_COLUMNS) return -1;
  /* Builder 0x70b70, type17 branch 0x70c42..0x70d85. Unassigned words
   * remain zero as in the native caller's initialized parameter buffers. */
  static const int32_t parameters[4][18]={
    {800,100,384,20,300,400,0,8850,5250,800,300,300,180,90,40,40,160,60},
    {250,512,384,20,200,300,1},
    {600,640},
    {1600,768,384,0,700,800},
  };
  memcpy(out->parameters,parameters,sizeof(parameters));
  /* Type17 has no extra offset in 0x70a80; its setup plane already
   * contains the 7095 baseline. Preserve signed-word widening/narrowing. */
  for (size_t i=0;i<GX521D_PIXELS;i++) {
    int value=(int16_t)setup[i]-(int16_t)live[i];
    out->difference[i]=(uint16_t)(value<0 ? 0:value);
  }
  milan_profile9_erode_valid(mask->pixels,out->valid,GX521D_ROWS,GX521D_COLUMNS,6);
  static const uint32_t kernel[3]={6980,51576,6980};
  milan_profile9_filter_q16(out->difference,GX521D_ROWS,GX521D_COLUMNS,kernel,3,out->blurred);
  /* Reviewed dual-gradient owner 0x71780: strict signed-word comparisons,
   * valid neighbors, fixed direction order and first-winner tie handling. */
  static const int dx[8]={-1,1,0,0,-1,-1,1,1};
  static const int dy[8]={0,0,-1,1,-1,1,-1,1};
  memset(out->descending,0,sizeof(out->descending));
  memset(out->ascending,0,sizeof(out->ascending));
  memset(out->descending_direction,-1,sizeof(out->descending_direction));
  memset(out->ascending_direction,-1,sizeof(out->ascending_direction));
  for (int y=0;y<GX521D_ROWS;y++)
    for (int x=0;x<GX521D_COLUMNS;x++) {
      size_t i=(size_t)y*GX521D_COLUMNS+x;
      if (!out->valid[i]) continue;
      for (int d=0;d<8;d++) {
        int nx=x+dx[d],ny=y+dy[d];
        if (nx<0 || nx>=GX521D_COLUMNS || ny<0 || ny>=GX521D_ROWS) continue;
        size_t j=(size_t)ny*GX521D_COLUMNS+nx;
        if (!out->valid[j]) continue;
        int16_t down=(int16_t)(out->blurred[i]-out->blurred[j]);
        int16_t up=(int16_t)(out->blurred[j]-out->blurred[i]);
        if (down>(int16_t)out->descending[i]) {
          out->descending[i]=(uint16_t)down;out->descending_direction[i]=(int8_t)d;
        }
        if (up>(int16_t)out->ascending[i]) {
          out->ascending[i]=(uint16_t)up;out->ascending_direction[i]=(int8_t)d;
        }
      }
    }
  if (gx521d_classification_histogram(out->blurred,out->valid,
      out->parameters[0][7],out->parameters[0][8],&out->primary_histogram)) return -1;
  if (gx521d_classification_primary_summary(&out->primary_histogram,
      out->parameters[0][10],out->parameters[0][11],out->primary_summary)) return -1;
  uint16_t gradient_max[GX521D_PIXELS];
  for (size_t i=0;i<GX521D_PIXELS;i++)
    gradient_max[i]=(int16_t)out->descending[i]>(int16_t)out->ascending[i] ?
      out->descending[i]:out->ascending[i];
  static const uint32_t average_kernel[3]={21845,21845,21845};
  milan_profile9_filter_q16(gradient_max,GX521D_ROWS,GX521D_COLUMNS,average_kernel,3,out->gradient_average);
  out->gradient_summary_status=gx521d_classification_score_summary(out->gradient_average,out->valid,1,0,out->gradient_summary);
  return 0;
}

static int histogram_intensity(const Gx521dClassificationHistogram *histogram,int bin)
{
  /* Native reciprocal 0xa4402911 divides by the last bin index, 399. */
  return (bin*histogram->statistics[0]+200)/399+histogram->statistics[1];
}
static int histogram_region_mean(const Gx521dClassificationHistogram *histogram,int begin,int end)
{
  int64_t sum=0,weight=0;
  for (int i=begin;i<end;i++) {
    weight+=histogram->bins[i];sum+=(int32_t)((uint32_t)i*(uint32_t)histogram->bins[i]);
  }
  return histogram_intensity(histogram,(int)(sum/(weight+1)));
}
int gx521d_classification_primary_summary(const Gx521dClassificationHistogram *h,
    int upper_span,int lower_span,int32_t out[34])
{
  if (!h || !out || upper_span<0 || lower_span<0) return -1;
  memset(out,0,34*sizeof(*out));
  int low[5],high[5],low_means[5],high_means[5];
  for (int i=0;i<5;i++) {
    int q=h->quantiles[i];low[i]=histogram_intensity(h,q);
    low_means[i]=histogram_region_mean(h,0,q);
    q=h->quantiles[18-i];high[i]=histogram_intensity(h,q);
    high_means[i]=histogram_region_mean(h,q,400);
  }
  int median=histogram_intensity(h,h->quantiles[9]);
  int middle=histogram_region_mean(h,h->quantiles[6],h->quantiles[12]);
  int center=(median+middle)>>1;
  int lower=-1,lower_index=0,upper=-1,upper_index=0;
  if (center-low_means[0]>=lower_span) {
    lower_index=5;
    for (int i=1;i<5;i++)
      if (center-low_means[i]<lower_span) {
        lower_index=i;
        lower=low[i-1]+(low[i]-low[i-1])*(center-low_means[i-1]-lower_span)/
                          (low_means[i]-low_means[i-1]+1);
        break;
      }
    if (lower_index==5) {
      int distance=center-low_means[4];
      lower=low[4]+(center-low[4])*(distance-lower_span)/(distance+1);
      if (lower>center-lower_span) lower=center-lower_span;
    }
  }
  if (high_means[0]-center>=upper_span) {
    upper_index=5;
    for (int i=1;i<5;i++)
      if (high_means[i]-center<upper_span) {
        upper_index=i;
        upper=high[i]+(high[i-1]-high[i])*(upper_span-(high_means[i]-center))/
                        (high_means[i-1]-high_means[i]+1);
        break;
      }
    if (upper_index==5) {
      int distance=high_means[4]-center;
      upper=center+upper_span*distance/(distance+1);
      if (upper<center+upper_span) upper=center+upper_span;
    }
  }
  out[0]=upper;out[1]=upper_index;out[2]=lower;out[3]=lower_index;
  out[5]=median;out[6]=middle;
  memcpy(out+7,high,sizeof(high));memcpy(out+12,low,sizeof(low));
  memcpy(out+17,high_means,sizeof(high_means));memcpy(out+22,low_means,sizeof(low_means));
  /* This owner leaves +0x70 statistics untouched; the later score-summary
   * owner writes those words separately. They are zero at this boundary. */
  return 0;
}

int gx521d_classification_score_summary(const uint16_t source[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS],int minimum,int extended,int32_t out[34])
{
  if (!source || !valid || !out) return -1;
  Gx521dClassificationHistogram h;
  if (histogram_groups(source,valid,32767,minimum,10,&h)) return -1;
  memset(out,0,34*sizeof(*out));memcpy(out+28,h.statistics,sizeof(h.statistics));
  if (h.status) return h.status;
  out[5]=histogram_intensity(&h,h.quantiles[4]);
  out[6]=histogram_region_mean(&h,h.quantiles[2],h.quantiles[6]+1);
  out[7]=histogram_intensity(&h,h.quantiles[8]);
  out[8]=histogram_intensity(&h,h.quantiles[7]);
  out[12]=histogram_intensity(&h,h.quantiles[1]);
  out[13]=histogram_intensity(&h,h.quantiles[2]);
  if (extended) {
    out[17]=histogram_region_mean(&h,h.quantiles[8],400);
    out[18]=histogram_region_mean(&h,h.quantiles[7],400);
    out[22]=histogram_region_mean(&h,0,h.quantiles[0]+1);
    /* Native 0x6f320 then 0x6f380 counts the first cutoff bin twice. */
    int64_t weight=h.bins[h.quantiles[0]],sum=(int32_t)((uint32_t)h.quantiles[0]*(uint32_t)weight);
    for (int i=0;i<=h.quantiles[1];i++) {
      weight+=h.bins[i];sum+=(int32_t)((uint32_t)i*(uint32_t)h.bins[i]);
    }
    out[23]=histogram_intensity(&h,(int)(sum/(weight+1)));
  }
  return 0;
}

void gx521d_classification_walk(const Gx521dClassificationPrepared *p,int down,
    int intensity_cutoff,int raw_cutoff,uint16_t scores[GX521D_PIXELS])
{
  static const int offsets[8]={-1,1,-80,80,-81,79,-79,81};
  const uint16_t *gradient=down ? p->descending:p->ascending;
  const int8_t *direction=down ? p->descending_direction:p->ascending_direction;
  memset(scores,0,GX521D_PIXELS*sizeof(*scores));
  for (int i=0;i<GX521D_PIXELS;i++) {
    int intensity=(int16_t)p->blurred[i];
    if (!p->valid[i] ||
        (!(down ? intensity>intensity_cutoff:intensity<intensity_cutoff) &&
         (int16_t)p->gradient_average[i]<=raw_cutoff)) continue;
    /* 0x738cf overwrites the original difference scratch plane with the
     * filtered maximum gradient before 0x73ad4/0x73bda passes it to the walker. */
    int current=i;uint16_t sum=0;
    /* Reviewed 0x6e900 uses six steps, no per-step intensity cutoff. */
    for (int step=0;step<6;step++) {
      int d=direction[current];if (d<0) break;
      sum=(uint16_t)(sum+gradient[current]);current+=offsets[d];
    }
    scores[i]=sum;
  }
}

void gx521d_classification_admit(const Gx521dClassificationPrepared *p,int down,
    int kind,const uint16_t scores[GX521D_PIXELS],const int32_t score_summary[34],
    Gx521dColdClassification *out)
{
  const int32_t *parameters=p->parameters[0],*primary=p->primary_summary;
  uint8_t growth[GX521D_PIXELS]={0};
  int base=(score_summary[5]+score_summary[6])>>1;
  int strong=base+(score_summary[7]-base)*parameters[1]/100;
  if ((down && kind==2) || (!down && kind==1)) strong+=parameters[17];
  if (strong<parameters[9]) strong=parameters[9];
  int delta=(score_summary[8]-base)*50/100;
  if (delta<50) delta=50;
  int medium=base+delta;
  if (medium>parameters[0]-50) medium=parameters[0]-50;
  if (medium<parameters[4]-50) medium=parameters[4]-50;
  int weak=base+(down ? 0:50);
  if (weak>medium) weak=medium;
  if (weak<parameters[4]-50) weak=parameters[4]-50;
  int grow=strong-parameters[15];if (grow<medium) grow=medium;
  int upper=primary[0]+parameters[16];if (upper<primary[10]) upper=primary[10];
  int severe=0;
  for (int i=0;i<GX521D_PIXELS;i++) {
    if (!p->valid[i]) continue;
    int intensity=(int16_t)p->blurred[i],score=(int16_t)scores[i];
    if ((down ? intensity>parameters[7]:intensity<parameters[8]) || score>strong) {
      out->classes[i]=3;growth[i]=255;severe++;
    } else if (down) {
      if (score>medium) {
        out->classes[i]=intensity>upper ? 3:2;
        severe+=intensity>upper;
      } else if (score>weak) out->classes[i]=1;
    } else if (out->classes[i]==0) {
      if (score>medium) out->classes[i]=2;
      else if (score>weak) out->classes[i]=1;
    }
  }
  if (severe>50) {
    /* Shared native flood mechanics: interior seeds, eight neighbors,
     * signed-word admission >= cutoff, final mask bytes 0/255. */
    milan_profile9_flood((const int16_t *)scores,(int16_t)grow,
                        GX521D_ROWS,GX521D_COLUMNS,growth);
    for (int i=0;i<GX521D_PIXELS;i++)
      if (growth[i]) out->classes[i]=3;
  }
}

int gx521d_classification_cold(const uint16_t live[GX521D_PIXELS],
    const Gx521dMask *mask,const Gx521dClassificationPrepared *p,
    Gx521dColdClassification *out)
{
  if (!live || !mask || !p || !out || mask->rows!=GX521D_ROWS ||
      mask->columns!=GX521D_COLUMNS) return -1;
  memset(out,0,sizeof(*out));
  const int32_t *primary=p->primary_summary,*gradient=p->gradient_summary,
                *parameters=p->parameters[0];
  int down=primary[0]>0 && primary[1]>0,up=primary[2]>0 && primary[3]>0;
  int base=(gradient[5]+gradient[6])>>1;
  int delta=(gradient[8]-base)*parameters[14]/100;
  if (delta<30) delta=30;
  int raw_cutoff=base+delta;
  if (raw_cutoff<parameters[13]) raw_cutoff=parameters[13];
  if (raw_cutoff>parameters[12]) raw_cutoff=parameters[12];
  int process=!(down || up) ||
    (gradient[7]-gradient[5]>=40 &&
     (gradient[8]-gradient[5]>=30 || gradient[7]-gradient[5]>=50) &&
     gradient[7]>=100 && (gradient[8]>=90 || gradient[7]>=120));
  if (process && (down || up)) {
    int kind=0;
    if (down && !up) kind=1;
    else if (!down && up) kind=2;
    else {
      int above=primary[17]-primary[6],below=primary[6]-primary[22];
      if (abs(below-above)>100) kind=(below>above)+1;
      else {
        above=primary[18]-primary[6];below=primary[6]-primary[23];
        if (abs(below-above)>80) kind=(below>above)+1;
      }
    }
    uint16_t scores[GX521D_PIXELS];int32_t summary[34];
    if (down) {
      gx521d_classification_walk(p,1,primary[0],raw_cutoff,scores);
      gx521d_classification_score_summary(scores,p->valid,2,1,summary);
      gx521d_classification_admit(p,1,kind,scores,summary,out);
    }
    if (up) {
      gx521d_classification_walk(p,0,primary[2],raw_cutoff,scores);
      gx521d_classification_score_summary(scores,p->valid,2,1,summary);
      gx521d_classification_admit(p,0,kind,scores,summary,out);
    }
  }
  /* 0x6ee90 projects low live ADC values after the primary classifier. */
  for (int y=3;y<GX521D_ROWS-3;y++)
    for (int x=3;x<GX521D_COLUMNS-3;x++) {
      int i=y*GX521D_COLUMNS+x;if ((int16_t)live[i]<50) out->classes[i]=3;
    }
  out->states[3]=gx521d_classification_direction(live,p->valid,mask->count);
  return out->states[3]<0 ? -1:0;
}

int gx521d_classification_publish(uint8_t classes[GX521D_PIXELS],int active_count,
    const int32_t states[4],int purpose,Gx521dClassificationDecision *out)
{
  if (!classes || !states || !out || active_count<0 || active_count>GX521D_PIXELS) return -1;
  memset(out,0,sizeof(*out));
  for (size_t i=0;i<GX521D_PIXELS;i++) {
    uint8_t value=classes[i];
    if (value>=1 && value<=3) out->counts[value-1]++;
    if (value==1 || value==2) classes[i]=0;
  }
  int c1=(int)out->counts[0],c2=(int)out->counts[1],c3=(int)out->counts[2];
  int combined=c1+c3,total=c2+combined;
  if (c3*100>active_count*15) {out->mode=8;out->apply_mask=1;}
  else if (c3>300) {out->mode=7;out->apply_mask=1;}
  else if (total*100>active_count*25) {out->mode=7;out->apply_mask=c3>150;}
  else if (c3>150) {out->mode=6;out->apply_mask=1;}
  else if (c2>600 || combined>500) out->mode=6;
  else if (c2>300 || combined>300 || total>300) out->mode=5;
  if (c3*100>active_count*30 || (states[0]==2 && c3*100>active_count*10)) {
    out->mode=9;out->status=0xc351;
  }
  if (states[3]==2) {if (out->mode<8) out->mode=8;}
  else if ((states[3]==1 || states[1]>=1) && out->mode<6) out->mode=6;
  int promoted=states[1]+(states[0]==2 || states[3]==2);
  if (out->mode==0 && states[2]>0) out->mode=5;
  out->auxiliary_summary[0]=(uint8_t)states[0];
  out->auxiliary_summary[1]=(uint8_t)purpose;
  out->auxiliary_summary[2]=(uint8_t)promoted;
  return 0;
}

int gx521d_classification_direction(const uint16_t live[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS],int active_count)
{
  if (!live || !valid || active_count<0 || active_count>GX521D_PIXELS) return -1;
  uint8_t left[GX521D_PIXELS]={0},right[GX521D_PIXELS]={0},
          up[GX521D_PIXELS]={0},down[GX521D_PIXELS]={0};
  /* 0x6f400 accumulates nonzero mask pixels on each side, including across
   * holes. It does not reset a consecutive run when encountering a hole.
   * Counts exclude the current pixel and narrow to bytes. */
  for (size_t y=0;y<GX521D_ROWS;y++) {
    uint8_t sum=0;
    for (size_t x=0;x<GX521D_COLUMNS;x++) {
      size_t i=y*GX521D_COLUMNS+x;left[i]=sum;sum+=(valid[i]!=0);
    }
    sum=0;
    for (size_t x=GX521D_COLUMNS;x-- >0;) {
      size_t i=y*GX521D_COLUMNS+x;right[i]=sum;sum+=(valid[i]!=0);
    }
  }
  for (size_t x=0;x<GX521D_COLUMNS;x++) {
    uint8_t sum=0;
    for (size_t y=0;y<GX521D_ROWS;y++) {
      size_t i=y*GX521D_COLUMNS+x;up[i]=sum;sum+=(valid[i]!=0);
    }
    sum=0;
    for (size_t y=GX521D_ROWS;y-- >0;) {
      size_t i=y*GX521D_COLUMNS+x;down[i]=sum;sum+=(valid[i]!=0);
    }
  }
  int strict=0,combined=0;
  for (size_t i=0;i<GX521D_PIXELS;i++) {
    int all4=left[i]>5 && right[i]>5 && up[i]>5 && down[i]>5;
    int pair=(left[i]>5 && right[i]>5) || (up[i]>5 && down[i]>5);
    int strong=(int16_t)live[i]>3800;
    strict+=strong && all4;
    combined+=(!valid[i] && all4) || (strong && pair);
  }
  return strict*10>active_count ? 2:(strict+combined)*10>active_count ? 1:0;
}

/* Type17 secondary histogram owner0x6de50. Adapted LGPL upstream mechanism:
 * exclusion endpoints remain eligible; after a failed local peak candidate the
 * pinned loop advances eleven bins, rather than testing its next ten bins. */
static uint8_t
gx521d_secondary_histogram_state (
  const MilanProfile9Histogram *clipped)
{
  int32_t smooth[MILAN_HISTOGRAM_BIN_COUNT];
  int32_t box[MILAN_HISTOGRAM_BIN_COUNT];

  if (clipped->mode_bin < MILAN_HISTOGRAM_SMOOTH_RADIUS ||
      clipped->mode_bin > 251 ||
      clipped->range <= 0)
    return 0;
  milan_profile9_smooth_histogram (
    clipped->histogram, MILAN_SECONDARY_HISTOGRAM_SHIFT, smooth, box);
  int global_peak = milan_profile9_histogram_global_peak (box);
  if ((int64_t) box[global_peak] * MILAN_PERCENT_SCALE >
      (clipped->total_weight >> 5) * 35)
    return 0;

  int exclusion = MILAN_SECONDARY_EXCLUSION_NUMERATOR / clipped->range;
  for (int bin = MILAN_HISTOGRAM_SMOOTH_RADIUS;
       bin <= MILAN_HISTOGRAM_INTERIOR_LAST_BIN; bin++)
    {
      if (bin > global_peak - exclusion &&
          bin < global_peak + exclusion)
        continue;
      if (!milan_profile9_histogram_local_maximum (smooth, bin))
        continue;
      int32_t candidate_reduction = smooth[bin] * 5 / MILAN_PERCENT_SCALE;
      int32_t peak_reduction = smooth[global_peak] / MILAN_PERCENT_SCALE;
      int32_t reduction = candidate_reduction > peak_reduction
                            ? candidate_reduction : peak_reduction;
      int32_t prominence = smooth[bin] - reduction;
      if (smooth[bin - MILAN_HISTOGRAM_SMOOTH_RADIUS] > prominence ||
          smooth[bin + MILAN_HISTOGRAM_SMOOTH_RADIUS] > prominence ||
          (int64_t) box[bin] * MILAN_PERCENT_SCALE <
            (int64_t) box[global_peak] * 25)
        { bin += 10; continue; }
      int32_t valley = milan_profile9_histogram_valley (
        box, bin, global_peak);
      if ((int64_t) valley * MILAN_PERCENT_SCALE <=
            (int64_t) box[bin] * MILAN_HISTOGRAM_VALLEY_CANDIDATE_PERCENT &&
          (int64_t) valley * MILAN_PERCENT_SCALE <=
            (int64_t) box[global_peak] * MILAN_HISTOGRAM_VALLEY_GLOBAL_PERCENT)
        return 1;
      bin += 10;
    }
  return 0;
}

static int gx521d_history_histogram_state(const uint16_t *source,const uint8_t *valid)
{
  MilanProfile9Histogram input,clipped;
  milan_profile9_build_histogram(source,valid,GX521D_PIXELS,&input);
  if (input.range<=0) return 0;
  milan_profile9_clip_histogram(source,valid,GX521D_PIXELS,&input,&clipped);
  return gx521d_secondary_histogram_state(&clipped);
}

static int gx521d_history_count_state(const uint16_t source[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS])
{
  static const int dx[8]={-1,1,0,0,-1,-1,1,1};
  static const int dy[8]={0,0,-1,1,-1,1,-1,1};
  uint16_t gradient[GX521D_PIXELS]={0},scores[GX521D_PIXELS]={0};
  int8_t direction[GX521D_PIXELS];memset(direction,-1,sizeof(direction));
  for (int y=0;y<GX521D_ROWS;y++) for (int x=0;x<GX521D_COLUMNS;x++) {
    int i=y*GX521D_COLUMNS+x;if (!valid[i]) continue;
    for (int d=0;d<8;d++) {
      int nx=x+dx[d],ny=y+dy[d];
      if (nx<0 || nx>=GX521D_COLUMNS || ny<0 || ny>=GX521D_ROWS) continue;
      int j=ny*GX521D_COLUMNS+nx;if (!valid[j]) continue;
      int16_t delta=(int16_t)(source[i]-source[j]);
      if (delta>(int16_t)gradient[i]) {gradient[i]=(uint16_t)delta;direction[i]=(int8_t)d;}
    }
  }
  int sum=0,samples=0,selected=0;
  for (int i=0;i<GX521D_PIXELS;i++) if (valid[i]) {
    int current=i;uint16_t score=0;
    for (int step=0;step<4;step++) {
      int d=direction[current];if (d<0) break;
      score=(uint16_t)(score+gradient[current]);current+=dy[d]*GX521D_COLUMNS+dx[d];
    }
    scores[i]=score;
    /* Type17 parameters[2]={600,640}; upstream type12 uses {400,512}. */
    if ((int16_t)score>0 && (int16_t)score<600) {sum+=(int16_t)score;samples++;}
  }
  int threshold=0;
  if (samples) {
    threshold=((sum+samples/2)/samples)*640>>8;
    if (threshold<600) threshold=600;
  }
  for (int i=0;i<GX521D_PIXELS;i++) selected+=(int16_t)scores[i]>threshold;
  return selected>600 ? 2:selected>300 ? 1:0;
}

void gx521d_classifier_init(Gx521dClassifierContext *context)
{
  if (context) gx521d_history_init(&context->history);
}
int gx521d_classification_step(Gx521dClassifierContext *context,
    const uint16_t live[GX521D_PIXELS],const uint16_t setup[GX521D_PIXELS],
    const Gx521dMask *mask,const Gx521dColdGain *gain,int purpose,
    Gx521dColdClassification *out)
{
  if (!context || !gain || purpose<0 || purpose>2) return -1;
  Gx521dClassificationPrepared prepared;
  if (gx521d_classification_prepare(live,setup,mask,&prepared)) return -1;
  Gx521dHistory *history=&context->history;int initialized=0;
  if (purpose!=2) {
    if (!history->count && gain->sample_count>1) {
      history->count=gain->sample_count<21 ? (int)gain->sample_count:21;
      memcpy(history->reference,gain->calibration,sizeof(history->reference));
      memset(history->age,history->count,sizeof(history->age));initialized=1;
    } else if (gx521d_history_update(history,prepared.difference,(int)gain->sample_count,mask)) return -1;
  }
  if (gx521d_classification_cold(live,mask,&prepared,out)) return -1;
  if (history->count>20) {
    uint8_t qualified[GX521D_PIXELS]={0};
    for (int y=3;y<GX521D_ROWS-3;y++) for (int x=3;x<GX521D_COLUMNS-3;x++) {
      int i=y*GX521D_COLUMNS+x;
      qualified[i]=prepared.valid[i]!=0 && history->age[i]*100>=history->count*50;
    }
    out->states[1]=gx521d_history_histogram_state(history->reference,qualified);
    out->states[2]=gx521d_history_count_state(history->reference,qualified);
    if (!out->states[1] && !out->states[2] && !initialized) {
      out->states[1]=gx521d_history_histogram_state(gain->calibration,qualified);
      out->states[2]=gx521d_history_count_state(gain->calibration,qualified);
    }
    GoodixMilanPreprocessState *temporal=calloc(1,sizeof(*temporal));
    if (!temporal) return -1;
    memcpy(temporal->profile9_history_reference,history->reference,sizeof(history->reference));
    milan_profile9_temporal_class3(temporal,qualified,GX521D_ROWS,GX521D_COLUMNS,out->classes);
    free(temporal);
  }
  return 0;
}
