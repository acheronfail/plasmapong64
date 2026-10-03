/* Frozen sample-major mixer: exact-output oracle for the blocked mixer. */
static void sound_render_reference(Sound *s,int16_t *stereo,size_t frames) {
    for(size_t frame=0;frame<frames;frame++) {
        int left=0,right=0;
        for(int i=0;i<SOUND_VOICES;i++) {
            SoundVoice *v=&s->voice[i]; if(!v->pcm) continue;
            if(v->gain<v->target) { v->gain+=12; if(v->gain>v->target) v->gain=v->target; }
            if(v->gain>v->target) { v->gain-=12; if(v->gain<v->target) v->gain=v->target; }
            if(!v->gain && !v->target) { if(!v->loop) v->pcm=NULL; continue; }
            unsigned pos=v->position>>16;
            int value=v->pcm[pos]*(v->gain>>8)/256;
            left+=value*v->left/256; right+=value*v->right/256;
            v->position+=s->step;
            if((v->position>>16)>=v->length) {
                if(v->loop) v->position-=(v->length-v->loop_start)<<16;
                else v->pcm=NULL;
            }
        }
        /* Saturating output; modest loop gains leave headroom for impacts. */
        stereo[frame*2]=(int16_t)(left<-32767?-32767:left>32767?32767:left);
        stereo[frame*2+1]=(int16_t)(right<-32767?-32767:right>32767?32767:right);
    }
}
