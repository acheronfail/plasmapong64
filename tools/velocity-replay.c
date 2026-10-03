/* Deterministic comparison data; compile once per backend. Each row includes
   gameplay, decoded flow statistics, saturation count and dye statistics. */
#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    static Game g; game_init(&g); g.phase=LOBBY;
    Input in[MAX_PLAYERS]={{.connected=true,.start=true},{.connected=true}};
    game_step(&g,in); in[0].start=false;
    int isolated=argc>1 && !strcmp(argv[1],"flow");
    puts("frame,bx,by,bvx,bvy,score0,score1,rms_velocity,max_velocity,mean_u,mean_v,saturated,mean_dye");
    for(int t=0;t<3600;t++) {
        if(isolated) {
            /* Identical forcing without ball feedback or scoring divergence. */
            if(t<1800) {
                fluid_splat(&g.fluid,50,90+50*sinf(t*.037f),24,180,70*sinf(t*.053f),.1f,0);
                fluid_splat(&g.fluid,ARENA_W-50,90+50*cosf(t*.027f),24,-180,60*cosf(t*.043f),.1f,1);
            }
            fluid_velocity_step(&g.fluid,STEP); fluid_dye_step(&g.fluid,STEP);
        } else {
            for(int p=0;p<2;p++) {
                in[p].x=sinf(t*.021f+p); in[p].y=sinf(t*.037f+p*2.4f);
                in[p].z=t%110<90; in[p].a=(t+40*p)%145>118;
            }
            in[0].start=g.phase==FINISHED; game_step(&g,in);
        }
        double energy=0,mu=0,mv=0,dye=0;
        float maximum=0; unsigned saturated=0;
        for(int k=0;k<FN;k++) {
            float u=fluid_flow_decode(fluid_velocity(&g.fluid)->u[k]);
            float v=fluid_flow_decode(fluid_velocity(&g.fluid)->v[k]);
            assert(isfinite(u) && isfinite(v));
            energy+=u*u+v*v; mu+=u; mv+=v;
            maximum=fmaxf(maximum,fmaxf(fabsf(u),fabsf(v)));
            saturated+=fabsf(u)>=1023.9375f || fabsf(v)>=1023.9375f;
            dye+=fluid_ink_decode(fluid_dye(&g.fluid)->red[k])+fluid_ink_decode(fluid_dye(&g.fluid)->blue[k]);
        }
        assert(isfinite(g.bx) && isfinite(g.by));
        if(argc>2 && (t==179 || t==539 || t==1799 || t==3599)) {
            char path[512]; snprintf(path,sizeof(path),"%s-%d.ppm",argv[2],t+1);
            FILE *image=fopen(path,"wb"); assert(image);
            fprintf(image,"P6\n%d %d\n255\n",FW,FH);
            for(int k=0;k<FN;k++) {
                uint32_t rgb=fluid_color(&g.fluid,k);
                unsigned char pixel[3]={rgb>>16,rgb>>8,rgb};
                assert(fwrite(pixel,1,3,image)==3);
            }
            assert(fclose(image)==0);
        }
        printf("%d,%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f,%.6f,%.6f,%.6f,%u,%.6f\n",
            t+1,(double)g.bx,(double)g.by,(double)g.bvx,(double)g.bvy,g.score[0],g.score[1],
            sqrt(energy/FN),(double)maximum,mu/FN,mv/FN,saturated,dye/FN);
    }
}
