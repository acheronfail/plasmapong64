#ifndef FLUID_PRESSURE_Q3_H
#define FLUID_PRESSURE_Q3_H
/* Four-term parallel approximation to the lexicographic recurrence p=b+pL/4.
   The remaining 1/256 tail uses the pressure available before this batch.
   Q3 storage fits eight pressure/divergence rows on a wider RSP grid. The
   public output remains Q12 for the existing gradient and visualization code. */
static int pressure_short(int value) { return value>32767?32767:value<-32768?-32768:value; }
static void fluid_pressure_q3_cpu(int32_t *pressure,const int32_t *divergence) {
    int16_t p[FN];
#if PLASMAPONG_PRESSURE_WARM_START
    for(int k=0;k<FN;k++) p[k]=(int16_t)pressure_short(pressure[k]>>9);
    for(int y=1;y<FH-1;y++) { p[y*FW]=p[y*FW+1]; p[y*FW+FW-1]=p[y*FW+FW-2]; }
    memcpy(p,p+FW,FW*sizeof(*p));
    memcpy(p+(FH-1)*FW,p+(FH-2)*FW,FW*sizeof(*p));
#else
    memset(p,0,sizeof(p));
#endif
    for(int pass=0;pass<PLASMAPONG_PRESSURE_PASSES;pass++) {
        for(int y=1;y<FH-1;y++) {
            int previous[3]={0,0,p[y*FW]};
            for(int x=1;x<FW-1;x+=8) {
                int base[8],tail[8],count=FW-1-x;
                if(count>8) count=8;
                for(int j=0;j<count;j++) {
                    int k=y*FW+x+j;
                    int d=pressure_short(divergence[k]>>11); /* Q3 divergence /4. */
                    base[j]=pressure_short(d+((p[k-FW]+p[k+FW]+p[k+1]+2)>>2));
                    tail[j]=x+j<4?0:p[k-4];
                }
                for(int j=0;j<count;j++) {
                    int a=j>=1?base[j-1]:previous[2];
                    int b=j>=2?base[j-2]:previous[j+1];
                    int c=j>=3?base[j-3]:previous[j];
                    p[y*FW+x+j]=(int16_t)pressure_short(base[j]+((a*64+b*16+c*4+tail[j]+128)>>8));
                }
                if(count>=3) for(int j=0;j<3;j++) previous[j]=base[count-3+j];
            }
            p[y*FW]=p[y*FW+1]; p[y*FW+FW-1]=p[y*FW+FW-2];
        }
        memcpy(p,p+FW,FW*sizeof(*p));
        memcpy(p+(FH-1)*FW,p+(FH-2)*FW,FW*sizeof(*p));
    }
    for(int k=0;k<FN;k++) pressure[k]=(int32_t)p[k]*512;
}
#endif
