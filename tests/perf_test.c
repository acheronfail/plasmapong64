#include "perf.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
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
    puts("PASS: presentation counts swaps, repeated refreshes, longest gaps and reset");
}
