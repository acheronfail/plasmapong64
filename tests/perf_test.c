#include "perf.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    PerfControls controls={0};
    assert(!perf_controls_step(&controls,false,true,true) && !controls.visible);
    assert(!perf_controls_step(&controls,true,true,true) && controls.visible);
    assert(!perf_controls_step(&controls,true,false,false));
    assert(!perf_controls_step(&controls,true,true,false) && !controls.visible);
    assert(!perf_controls_step(&controls,true,true,false) && !controls.visible);
    assert(!perf_controls_step(&controls,true,false,false));
    assert(perf_controls_step(&controls,true,true,true) && controls.visible);
    assert(!perf_controls_step(&controls,true,true,true));
    assert(!perf_controls_step(&controls,false,false,false) && !controls.visible);
    assert(!perf_controls_step(&controls,false,true,true) && !controls.visible);
    assert(!perf_controls_step(&controls,false,false,false) && !controls.visible);
    assert(!perf_controls_step(&controls,true,false,false) && controls.visible);
    PresentationStats s={0};
    presentation_sample(&s,100,1000);
    assert(s.seeded && !s.frames && !s.refreshes && !s.repeats);
    for(unsigned i=1;i<=60;i++) presentation_sample(&s,100+(i%3)*100,1000+i*100);
    assert(s.frames==60 && s.refreshes==60 && !s.repeats && s.longest_gap==1);
    presentation_sample(&s,100,7100);
    presentation_sample(&s,100,7200);
    assert(s.frames==60 && s.refreshes==62 && s.repeats==2 && s.longest_gap==3);
    presentation_sample(&s,200,7300);
    assert(s.frames==61 && s.gap==1 && s.longest_gap==3);
    s=(PresentationStats){0};
    presentation_sample(&s,200,8000);
    assert(!s.repeats && !s.frames && s.longest_gap==1);
    s=(PresentationStats){.allowed_gap=2};
    presentation_sample(&s,100,0);
    for(unsigned i=1;i<=60;i++) presentation_sample(&s,100+((i/2)%3)*100,i*100);
    assert(s.frames==30 && s.repeats==30 && s.misses==0 && s.longest_gap==2);
    presentation_sample(&s,100,6100); presentation_sample(&s,100,6200);
    assert(s.misses==1 && s.longest_gap==3);
    /* 480i alternates base and base+stride on the same framebuffer. Count
       actual swaps, including consecutive fields from different buffers. */
    s=(PresentationStats){.allowed_gap=2};
    const uint32_t stride=1280;
    for(unsigned i=0;i<=60;i++) {
        uint32_t base=0x100000+((i/2)%3)*0x96000;
        bool odd=(i&1)!=0;
        presentation_sample(&s,presentation_origin(base+(odd?0:stride),true,odd,stride),i*100);
    }
    assert(s.frames==30 && s.refreshes==60 && s.repeats==30 && !s.misses && s.longest_gap==2);
    assert(presentation_origin(0,false,false,stride)==0);
    assert(presentation_origin(0x100000,false,false,stride)==0x100000);
    uint32_t base=0x200000;
    presentation_sample(&s,presentation_origin(base,true,true,stride),6100);
    assert(s.frames==31);
    presentation_sample(&s,presentation_origin(base+stride,true,false,stride),6200);
    assert(s.frames==31 && s.repeats==31 && !s.misses);
    puts("PASS: FPS option gates L/R with debounce; presentation counts swaps, repeated fields/refreshes, interlaced origins, longest gaps and reset");
}
