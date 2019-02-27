#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "jaytrax.h"

#define M_PI (3.14159265359)
#define SE_OVERLAP (100)			// overlap duration in samples, for declick

int32_t m_frequencyTable[SE_NROFFINETUNESTEPS][128];
int16_t m_sineTab[256];
int16_t m_emptyWave[256];
uint8_t isStaticInit = 0;

Song* song;
Voice    m_ChannelData[SE_NROFCHANS];
int32_t  m_CurrentSubsong;
int16_t  m_TimeCnt;      // Samplecounter which stores the njumber of samples before the next songparams are calculated (is reinited with m_TimeSpd)
int16_t  m_TimeSpd;      // Sample accurate counter which indicates every how many samples the song should progress 1 tick. Is dependant on rendering frequency and BPM
uint8_t  m_PlayFlg;      // 0 if playback is stopped, 1 if song is being played
uint8_t  m_PauseFlg;     // 0 if playback is not paused, 1 if playback is paused
int32_t  m_PlaySpeed;    // Actual delay in between notes
int32_t  m_PatternDelay; // Current delay in between notes (resets with m_PlaySpeed)
int32_t  m_PlayPosition;	// Current position in song (coarse)
int32_t  m_PlayStep;		// Current position in song (fine)
int32_t  m_MasterVolume; // Mastervolume of the replayer (256=max - 0=min)
int16_t  m_LeftDelayBuffer[65536];   // buffer to simulate an echo on the left stereo channel
int16_t  m_RightDelayBuffer[65536];  // buffer to simulate an echo on the right stereo channel
int16_t  m_OverlapBuffer[SE_OVERLAP*2];	// Buffer which stores overlap between waveforms to avoid clicks
int16_t  m_OverlapCnt;   // Used to store how much overlap we have already rendered
uint16_t m_DelayCnt;				// Internal counter used for delay

//void SoundEngine::DeAllocate()

void clearSoundBuffers(JayPlayer* _this) {
	int32_t i,j;

	// clear delaybuffers
	memset(m_LeftDelayBuffer,  0, 65536*sizeof(short));
	memset(m_RightDelayBuffer, 0, 65536*sizeof(short));

	//initialize channel data
	for (i=0;i<SE_NROFCHANS;i++) {
        Voice* vc = &m_ChannelData[i];
        
		vc->songpos = 0;
		vc->patpos = 0;
		vc->instrument = -1;
		vc->volcnt = 0;
		vc->arpcnt = 0;
		vc->pancnt = 0;
		vc->curnote = 0;
		vc->curfreq = 0;
		vc->bendadd = 0;
		vc->destfreq = 0;
		vc->bendspd = 0;
		vc->freqcnt = 0;
		vc->freqdel = 0;
		vc->sampledata = 0;
		vc->endpoint = 0;
        vc->synthPos = 0;
		vc->samplepos = 0;
		vc->lastplaypos = 0;
		vc->curvol = 0;
		vc->curpan = 0;
		vc->bendtonote = 0;
		vc->looppoint = 0;
		vc->loopflg = 0;
		vc->bidirecflg = 0;
		vc->curdirecflg = 0;
		for(j=0;j<4;j++) {
            VoiceEffect* vfx = &vc->fx[j];
            
			vfx->fxcnt1 = 0;
			vfx->fxcnt2 = 0;
			vfx->osccnt = 0;
			vfx->a0 = 0;
			vfx->b1 = 0;
			vfx->b2 = 0;
			vfx->y1 = 0;
			vfx->y2 = 0;
			vfx->Vhp = 0;
			vfx->Vbp = 0;
			vfx->Vlp = 0;
		}
		memset(vc->waves, 0, 4096*sizeof(short));
	}
}

void handleEffects(JayPlayer* _this, int32_t channr) {
	int32_t f;
	for (f=0; f<SE_EFF_INST; f++) {
        Voice* vc; VoiceEffect* vfx; Inst* ins; Effect* fx;
		int16_t s;
        
        vc  = &m_ChannelData[channr];
        ins = song->instruments[vc->instrument];
        fx  = &ins->fx[f];
        vfx = &vc->fx[f];
        
		// increase oscilator
		s = (int16_t) fx->oscspd;
		vfx->osccnt += s;
		vfx->osccnt &= 255;

		switch (fx->effecttype) {
            case 0: //none
                break;
            case 1: { //negate
                    int32_t dest;
                    int16_t *dw;
                    int16_t i,s,c;
                    dest = fx->dsteffect;
                    s = (int16_t)fx->effectspd;
                    c = (int16_t)vfx->fxcnt1;
                    dw = &vc->waves[256*dest];
                    for (i=0; i<s;i++) {
                        c++;
                        c&=255;
                        dw[c] = 0-dw[c];
                    }
                    vfx->fxcnt1 = (int32_t)c;
                }
                break;
            case 2: { // warp
                    int32_t dest;
                    int16_t *dw;
                    int16_t i,s,c;
                    dest = fx->dsteffect;
                    s = (int16_t) fx->effectspd;
                    dw = &vc->waves[256*dest];
                    c = 0;
                    for (i=0; i<256; i++) {
                        dw[i] += c;
                        c+=s;
                    }
                }
                break;
            case 3: { // Filter
                    int32_t dest, src;
                    int16_t *dw, *sw;
                    int16_t i, s, t;
                    dest = fx->dsteffect;
                    src = fx->srceffect1;
                    dw = &vc->waves[256*dest];
                    sw = &vc->waves[256*src];
                    s = (int16_t) fx->effectspd;
                    if(s>12) s=12; //not more than 12 times...it slowes down too much
                    for (t=0; t<s; t++) {
                        dw[0] = (sw[255] +sw[1])>>1;
                        for (i=1; i<255; i++) {
                            dw[i] = (sw[i-1] +sw[i+1])>>1;
                        }
                        dw[255] = (sw[254] +sw[0])>>1;
                    }
                }
                break;
            case 4: { // Wavemix
                    int32_t dest, src1, src2;
                    int16_t *dw, *sw1, *sw2;
                    int16_t i, s, c;
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->srceffect2;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];
                    s = (int16_t) fx->effectspd;
                    vfx->fxcnt1 += s;
                    vfx->fxcnt1 &= 255;
                    c = (int16_t)vfx->fxcnt1;
                    for (i=0; i<256; i++) {
                        dw[i] = (sw1[i] +sw2[c])>>1;
                        c++;
                        c&=255;
                    }
                }
                break;
            case 5: { // Resonance
                    int32_t dest, src1, src2;
                    int16_t *dw, *sw1, *sw2;
                    int16_t i, c;
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];

                    c = (int16_t)vfx->osccnt;

    // init
                    double centerFreq,bandwidth;
                    if(fx->osceffect==0) {
                        centerFreq = (double)(fx->effectvar1*20);
                        bandwidth = (double)(fx->effectvar2*16);
                    } else {
                        if(fx->oscflg) {
                            centerFreq = (double)(fx->effectvar1*20);
                            bandwidth = (double)(sw2[c]+32768)/16;
                        } else {
                            centerFreq = (double)(sw2[c]+32768)/13;
                            bandwidth = (double)(fx->effectvar2*16);
                        }
                    }

                    vfx->b2 = exp(-(2 * M_PI) * (bandwidth / 22000));
                    vfx->b1 = (-4.0 * vfx->b2) / (1.0 + vfx->b2) * cos(2 * M_PI * (centerFreq / 22000));
                    vfx->a0 = (1.0 - vfx->b2) * sqrt(1.0 - (vfx->b1 * vfx->b1) / (4.0 * vfx->b2));

                    for (i=0; i<256; i++) {
                        double o;
                        o = vfx->a0 * ((double)(sw1[i])/32768) - vfx->b1 * vfx->y1 - vfx->b2 * vfx->y2;

                        vfx->y2 = vfx->y1;
                        vfx->y1 = o;
                        if(o>.9999)o=.9999;
                        if(o<-.9999)o=-.9999;
                        dw[i] = (int16_t)(o*32768);
                    }
                }
                break;
            case 6: { // Reso Whistle
                    int32_t dest,src1,src2;
                    int16_t *dw,*sw1,*sw2;
                    int16_t i,c;
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];

                    c = (int16_t)vfx->osccnt;

    // init
                    double centerFreq, bandwidth;
                    
                    if(fx->osceffect==0) {
                        centerFreq = (double)(fx->effectvar1*20);
                        bandwidth = (double)(fx->effectvar2*16);
                    } else {
                        if(fx->oscflg) {
                            centerFreq = (double)(fx->effectvar1*20);
                            bandwidth = (double)(sw2[c]+32768)/16;
                        } else {
                            centerFreq = (double)(sw2[c]+32768)/13;
                            bandwidth = (double)(fx->effectvar2*16);
                        }
                    }

                    vfx->b2 = exp(-(2 * M_PI) * (bandwidth / 22000));
                    vfx->b1 = (-4.0 * vfx->b2) / (1.0 + vfx->b2) * cos(2 * M_PI * (centerFreq / 22000));
                    vfx->a0 = (1.0 - vfx->b2) * sqrt(1.0 - (vfx->b1 * vfx->b1) / (4.0 * vfx->b2));

                    vfx->b2*=1.2; // do the reso whistle
                    for (i=0; i<256; i++) {
                        double o;
                        o = vfx->a0 * ((double)(sw1[i])/32768) - vfx->b1 * vfx->y1 - vfx->b2 * vfx->y2;

                        vfx->y2 = vfx->y1;
                        vfx->y1 = o;
                        if(o>.9999)o=.9999;
                        if(o<-.9999)o=-.9999;
                        dw[i] = (int16_t)(o*32768);
                    }
                }
                break;
            case 7: { // Morphing
                    int32_t dest,src1,src2,osc;
                    int16_t *dw,*sw1,*sw2,*ow;
                    int16_t i,c;
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->srceffect2;
                    osc = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];
                    ow = &vc->waves[256*osc];

                    c = (int16_t)vfx->osccnt;

    // init
                    int16_t m1,m2;
                    if(fx->osceffect==0) {
                        m1 = fx->effectvar1;
                    } else {
                        if(fx->oscflg) {
                            m1 = fx->effectvar1;
                        } else {
                            m1 = (ow[c]+32768)/256;
                        }
                    }

                    m2 = 255-m1;
                    for (i=0; i<256; i++) {
                        int32_t a;
                        a=(((int32_t)sw1[i]*m1)/256)+(((int32_t)sw2[i]*m2)/256);
                        dw[i] = (int16_t)(a);
                    }
                }
                break;
            case 8: { // Dyna-Morphing
                    int32_t dest,src1,src2,osc;
                    int16_t *dw,*sw1,*sw2,*ow,*si;
                    int16_t i,c;
                    si = &m_sineTab[0];
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->srceffect2;
                    osc = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];
                    ow = &vc->waves[256*osc];

                    c = (int16_t)vfx->osccnt;

    // init
                    int16_t m1,m2,sc; //sc is sincnt
                    if(fx->osceffect==0) {
                        sc = fx->effectvar1;
                    } else {
                        if(fx->oscflg)
                        {
                            sc = fx->effectvar1;
                        }
                        else
                        {
                            sc = (ow[c]+32768)/256;
                        }
                    }

                    for (i=0; i<256; i++) {
                        int32_t a;
                        m1=(si[sc]>>8)+128;
                        m2 = 255-m1;
                        a=(((int32_t)sw1[i]*m1)/256)+(((int32_t)sw2[i]*m2)/256);
                        dw[i] = (int16_t)(a);
                        sc++;
                        sc&=255;
                    }
                }
                break;
            case 9: { // Distortion
                    int32_t dest,src1,osc;
                    int16_t *dw,*sw1,*ow;
                    int16_t i,c;
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    osc = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    ow = &vc->waves[256*osc];

                    c = (int16_t)vfx->osccnt;

    // init
                    int16_t m1;
                    if(fx->osceffect==0) {
                        m1 = fx->effectvar1;
                    } else {
                        if(fx->oscflg) {
                            m1 = fx->effectvar1;
                        } else {
                            m1 = (ow[c]+32768)/256;
                        }
                    }

                    for (i=0; i<256; i++) {
                        int32_t a;
                        a=((int32_t)sw1[i]*m1)/16;
                        a+=32768;
                        if(a<0)a=-a;
                        a%=131072;
                        if(a>65535) a = 131071-a;
                        a-=32768;
                        dw[i] = (int16_t)(a);
                    }
                }
                break;
            case 10: { // Scroll left
                
                    int32_t dest;
                    int16_t *dw;
                    int16_t i,t;
                    dest = fx->dsteffect;
                    dw = &vc->waves[256*dest];

                    t=dw[0];
                    for (i=0; i<255; i++) {
                        dw[i] = dw[i+1];
                    }
                    dw[255]=t;
                }
                break;
            case 11: { // Upsample
                
                    int32_t dest;
                    int16_t *dw;
                    int16_t i,c;
                    c = (int16_t)vfx->fxcnt1;
                    if(c!=0) { // timeout ended?
                        vfx->fxcnt1--;
                        break;
                    }
                    vfx->fxcnt1 = fx->effectvar1;
                    dest = fx->dsteffect;
                    dw = &vc->waves[256*dest];

                    for (i=0; i<128; i++) {
                        dw[i]=dw[i*2];
                    }
                    memcpy(&dw[128],&dw[0],256);
                }
                break;
            case 12: { // Clipper
                    int32_t dest,src1,osc;
                    int16_t *dw,*sw1,*ow;
                    int16_t i,c;
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    osc = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    ow = &vc->waves[256*osc];

                    c = (int16_t)vfx->osccnt;

    // init
                    int16_t m1;
                    if(fx->osceffect==0) {
                        m1 = fx->effectvar1;
                    } else {
                        if(fx->oscflg) {
                            m1 = fx->effectvar1;
                        } else {
                            m1 = (ow[c]+32768)/256;
                        }
                    }

                    for (i=0; i<256; i++) {
                        int32_t a;
                        a=((int32_t)sw1[i]*m1)/16;
                        if(a<-32767)a=-32767;
                        if(a>32767)a=32767;
                        dw[i] = (int16_t)(a);
                    }
                }
                break;
            case 13: { // bandpass
                    int32_t dest,src1,src2;
                    int16_t *dw,*sw1,*sw2;
                    int16_t i,c;
                    int32_t _2_pi_w0;
                    int32_t _1000_Q;

                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];

                    c = (int16_t)vfx->osccnt;

    // init
                    int32_t freq,reso;
                    if(fx->osceffect==0) {
                        freq = fx->effectvar1;
                        reso = fx->effectvar2;
                        freq*=16; //(freq 0 - 16000hz)
                    } else {
                        if(fx->oscflg) {
                            freq = fx->effectvar1;
                            reso = (sw2[c]+32768)>>8;
                            freq*=16; //(freq 0 - 16000hz)
                        } else {
                            freq = (sw2[c]+32768)/16;
                            reso = fx->effectvar2;
                        }
                    }
                    //calc freq;
                    //double x = freq - 920.0;
                    //double w0 = 228 + 3900/2*(1 + tanh(_copysign(pow(fabs(x), 0.85)/95, x)));
                    double w0 = 228+freq;
                    _2_pi_w0 = (int32_t)(2*M_PI*w0);

                    //calc Q
                    _1000_Q = 707 + 1000*reso/128;

                    int32_t _2_pi_w0_delta_t;
                    int32_t Vhp_next;
                    int32_t Vbp_next;
                    int32_t Vlp_next;
                    int32_t Vbp;
                    int32_t	Vlp;
                    int32_t	Vhp;
                    int32_t Vi;
                    int32_t s;
                    int32_t delta_t;

                    Vbp = vfx->Vbp;
                    Vlp = vfx->Vlp;
                    Vhp = vfx->Vhp;
                    delta_t=8;
                    
                    // now let's throw our waveform through the resonator!
                    for (i=0; i<256; i++) {
                        // delta_t is converted to seconds given a 1MHz clock by dividing
                        // with 1 000 000. This is done in three operations to avoid integer
                        // multiplication overflow.
                        _2_pi_w0_delta_t = _2_pi_w0*delta_t/100;

                        // Calculate filter outputs.
                        Vi=sw1[i];
                        Vhp_next = Vbp*1000/_1000_Q - Vlp + Vi;
                        Vbp_next = Vbp - _2_pi_w0_delta_t*(Vhp/100)/100;
                        Vlp_next = Vlp - _2_pi_w0_delta_t*(Vbp/100)/100;
                        Vhp = Vhp_next;
                        Vbp = Vbp_next;
                        Vlp = Vlp_next;

                        s = Vlp;
                        if(s>32767)s=32767;
                        if(s<-32767)s=-32767;

                        dw[i] = (int16_t)(s);
                    }
                    vfx->Vbp = Vbp;
                    vfx->Vlp = Vlp;
                    vfx->Vhp = Vhp;
                }
                break;
            case 14: { // highpass
                    int32_t dest,src1,src2;
                    int16_t *dw,*sw1,*sw2;
                    int16_t i,c;
                    int32_t _2_pi_w0;
                    int32_t _1000_Q;

                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];

                    c = (int16_t)vfx->osccnt;

    // init
                    int32_t freq, reso;
                    if(fx->osceffect==0) {
                        freq = fx->effectvar1;
                        reso = fx->effectvar2;
                        freq*=32; //(freq 0 - 16000hz)
                    } else {
                        if(fx->oscflg) {
                            freq = fx->effectvar1;
                            reso = (sw2[c]+32768)>>8;
                            freq*=32; //(freq 0 - 16000hz)
                        } else {
                            freq = (sw2[c]+32768)/8;
                            reso = fx->effectvar2;
                        }
                    }
                    //calc freq;
                    //double x = freq - 920.0;
                    //double w0 = 228 + 3900/2*(1 + tanh(_copysign(pow(fabs(x), 0.85)/95, x)));
                    double w0 = 228+freq;
                    _2_pi_w0 = (int32_t)(2*M_PI*w0);

                    //calc Q
                    _1000_Q = 707 + 1000*reso/128;

                    int32_t _2_pi_w0_delta_t;
                    int32_t Vhp_next;
                    int32_t Vbp_next;
                    int32_t Vlp_next;
                    int32_t Vbp;
                    int32_t	Vlp;
                    int32_t	Vhp;
                    int32_t Vi;
                    int32_t s;
                    int32_t delta_t;

                    Vbp = vfx->Vbp;
                    Vlp = vfx->Vlp;
                    Vhp = vfx->Vhp;
                    delta_t=8;
                    
                    // now let's throw our waveform through the resonator!
                    for (i=0; i<256; i++) {
                        // delta_t is converted to seconds given a 1MHz clock by dividing
                        // with 1 000 000. This is done in three operations to avoid integer
                        // multiplication overflow.
                        _2_pi_w0_delta_t = _2_pi_w0*delta_t/100;

                        // Calculate filter outputs.
                        Vi=sw1[i];
                        Vhp_next = Vbp*1000/_1000_Q - Vlp + Vi;
                        Vbp_next = Vbp - _2_pi_w0_delta_t*(Vhp/100)/100;
                        Vlp_next = Vlp - _2_pi_w0_delta_t*(Vbp/100)/100;
                        Vhp = Vhp_next;
                        Vbp = Vbp_next;
                        Vlp = Vlp_next;

                        s = Vhp;
                        if(s>32767)s=32767;
                        if(s<-32767)s=-32767;

                        dw[i] = (int16_t)(s);
                    }
                    vfx->Vbp = Vbp;
                    vfx->Vlp = Vlp;
                    vfx->Vhp = Vhp;
                }
                break;
            case 15: { // bandpass
                    int32_t dest,src1,src2;
                    int16_t *dw,*sw1,*sw2;
                    int16_t i,c;
                    int32_t _2_pi_w0;
                    int32_t _1000_Q;

                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    src2 = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    sw2 = &vc->waves[256*src2];

                    c = (int16_t)vfx->osccnt;

    // init

                    int32_t freq,reso;
                    if(fx->osceffect==0) {
                        freq = fx->effectvar1;
                        reso = fx->effectvar2;
                        freq*=16; //(freq 0 - 16000hz)
                    } else {
                        if(fx->oscflg) {
                            freq = fx->effectvar1;
                            reso = (sw2[c]+32768)>>8;
                            freq*=16; //(freq 0 - 16000hz)
                        } else {
                            freq = (sw2[c]+32768)/16;
                            reso = fx->effectvar2;
                        }
                    }
                    //calc freq;
                    //double x = freq - 920.0;
                    //double w0 = 228 + 3900/2*(1 + tanh(_copysign(pow(fabs(x), 0.85)/95, x)));
                    double w0 = 228+freq;
                    _2_pi_w0 = (int32_t)(2*M_PI*w0);

                    //calc Q
                    _1000_Q = 707 + 1000*reso/128;

                    int32_t _2_pi_w0_delta_t;
                    int32_t Vhp_next;
                    int32_t Vbp_next;
                    int32_t Vlp_next;
                    int32_t Vbp;
                    int32_t	Vlp;
                    int32_t	Vhp;
                    int32_t Vi;
                    int32_t s;
                    int32_t delta_t;

                    Vbp = vfx->Vbp;
                    Vlp = vfx->Vlp;
                    Vhp = vfx->Vhp;
                    delta_t=8;
                    
                    // now let's throw our waveform through the resonator!
                    for (i=0; i<256; i++) {
                        // delta_t is converted to seconds given a 1MHz clock by dividing
                        // with 1 000 000. This is done in three operations to avoid integer
                        // multiplication overflow.
                        _2_pi_w0_delta_t = _2_pi_w0*delta_t/100;

                        // Calculate filter outputs.
                        Vi=sw1[i];
                        Vhp_next = Vbp*1000/_1000_Q - Vlp + Vi;
                        Vbp_next = Vbp - _2_pi_w0_delta_t*(Vhp/100)/100;
                        Vlp_next = Vlp - _2_pi_w0_delta_t*(Vbp/100)/100;
                        Vhp = Vhp_next;
                        Vbp = Vbp_next;
                        Vlp = Vlp_next;

                        s = Vbp;
                        if(s>32767)s=32767;
                        if(s<-32767)s=-32767;

                        dw[i] = (int16_t)(s);
                    }
                    vfx->Vbp = Vbp;
                    vfx->Vlp = Vlp;
                    vfx->Vhp = Vhp;
                }
                break;
            case 16: { // Noise
                
                    int32_t dest;
                    int16_t *dw;
                    int16_t i;
                    dest = fx->dsteffect;
                    dw = &vc->waves[256*dest];

                    for (i=0; i<256; i++) {
                        //TODO: LCG or twin LSFR noise
                        dw[i]=(rand()*2)-32768;
                    }
                }
                break;
            case 17: { // Squash
                    int32_t dest,src1,osc;
                    int16_t *dw,*sw1,*ow;
                    int16_t i,c;
                    dest = fx->dsteffect;
                    src1 = fx->srceffect1;
                    osc = fx->osceffect-1;
                    dw = &vc->waves[256*dest];
                    sw1 = &vc->waves[256*src1];
                    ow = &vc->waves[256*osc];

                    c = (int16_t)vfx->osccnt;

    // init
                    uint16_t m1a, m1b, m2;
                    if(fx->osceffect==0) {
                        m1a = fx->effectvar1;
                        m1b = fx->effectvar2;
                    } else {
                        if(fx->oscflg) {
                            m1a = fx->effectvar1;
                            m1b = (ow[c]+32768)/256;
                        } else {
                            m1a = (ow[c]+32768)/256;
                            m1b = fx->effectvar2;
                        }
                    }

                    m1b<<=8;
                    m1a+=m1b; //m1 is now the counter for the squash
                    m2=0;     //m2 is the actual counter which is 256 times too large (fixed point)
                    for (i=0; i<256; i++) {
                        int32_t a,b;
                        b=sw1[m2>>8];
                        a=sw1[(m2>>8)+1];
                        a*=(m2&255);
                        b*=(255-(m2&255));
                        a=(a>>8)+(b>>8);
                        dw[i] = a;
                        m2+=m1a;
                    }
                }
                break;
		}
	}
}

void handleInstrument(JayPlayer* _this, int32_t channr) {
    Voice* vc; Inst* ins;
	int32_t vol, freq, pan;
    
    vc = &m_ChannelData[channr];
    ins = song->instruments[vc->instrument];
    
    //vol
	if (!ins->amwave) { //volume wave?
		vol = 0;
	} else {
		vc->volcnt += ins->amspd;
		if (vc->volcnt >= 256) {
			vc->volcnt -= 256;
			vc->volcnt += ins->amlooppoint;
			if(vc->volcnt >= 256) {
				vc->volcnt = ins->amlooppoint;
			}
		}

		vol = vc->waves[(256*(ins->amwave-1))+vc->volcnt];
		vol = vol+32768;
		vol /= 6;
		vol = -vol;                //10930;
		if (vol <-10000) vol = -10000;
	}	

    //last but not least, the master volume
	vol += 10000;
	vol *= ins->mastervol;
	vol >>=8;
	vol *= m_MasterVolume; //and the replayers master master volume
	vol >>=8;
	vol -= 10000;
	vc->curvol = vol;
    //if(vc->buf) vc->buf->SetVolume(vol);

    //update panning
	if(ins->panwave == 0) { //panning wave?
		pan = 0;
	} else {
		vc->pancnt += ins->panspd;
		if (vc->pancnt >= 256) {
			vc->pancnt -= 256;
			vc->pancnt += ins->panlooppoint;
			if(vc->pancnt >= 256) {
				vc->pancnt = ins->panlooppoint;
			}
		}

		pan = vc->waves[(256*(ins->panwave-1))+vc->pancnt];
		pan >>=7;
	}	
    //if(vc->buf) vc->buf->SetPan(pan);
	vc->curpan = pan;
	
    //update freq
	int32_t k;
	k = 0;
	k = song->arpTable[(ins->arpeggio*16)+vc->arpcnt];
	vc->arpcnt++;
	vc->arpcnt&=15;

	freq = m_frequencyTable[ins->finetune][k+vc->curnote]; 

	if(vc->freqdel) {
		vc->freqdel--;
	} else {
		if(ins->fmwave != 0) { //frequency wave?
			vc->freqcnt += ins->fmspd;
			if (vc->freqcnt >= 256) {
				vc->freqcnt -= 256;
				vc->freqcnt += ins->fmlooppoint;
				if(vc->freqcnt >= 256) {
					vc->freqcnt = ins->fmlooppoint;
				}
			}
			freq -= vc->waves[(256*(ins->fmwave-1))+vc->freqcnt];
		}
	}
	freq += vc->bendadd;
	vc->curfreq = freq;
	
    //update pitchbend
    
	if(vc->bendspd != 0) {
		if(vc->bendspd >0) {
			if(vc->bendadd < vc->destfreq) {
				vc->bendadd += vc->bendspd;
				if(vc->bendadd > vc->destfreq) {
					vc->bendadd = vc->destfreq;
				}
			}
		} else {
			if(vc->bendadd > vc->destfreq) {
				vc->bendadd += vc->bendspd;
				if(vc->bendadd < vc->destfreq) {
					vc->bendadd = vc->destfreq;
				}
			}
		}
	}
}

void playInstrument(JayPlayer* _this, int32_t channr, int32_t instNum, int32_t note) {
    Voice* vc; Inst* ins;
	int32_t f;
    
    // instruments init
	if(instNum > song->header.nrofinst) return; // not allowed!
    vc = &m_ChannelData[channr];
	if(vc->instrument == -1 && instNum == 0) return; //geen instrument 0 op een gemute channel...er was namelijk geen previous instrument
    ins = song->instruments[instNum-1];
    
	vc->arpcnt = 0;
	vc->volcnt = 0;
	vc->pancnt = 0;
	vc->freqcnt = 0;
	vc->curnote = note;
	vc->curfreq = 0;
	vc->bendtonote = note;
	vc->bendadd = 0;
	vc->destfreq = 0;
	vc->bendspd = 0;

	if(instNum) { // do not copy if 0
		int32_t i;
        
        //TODO: check sample usage
		if(!ins->sharing) { // no sample sharing
			vc->sampledata = song->samples[instNum-1];
		} else {
			vc->sampledata = song->samples[ins->sharing-1];
		}
		vc->samplepos = ins->startpoint<<8;
		vc->looppoint = ins->looppoint<<8;
		vc->endpoint = ins->endpoint<<8;
		vc->loopflg = ins->loopflg;
		vc->bidirecflg = ins->bidirecflg;
		vc->curdirecflg = 0;

		vc->lastplaypos = -1; // first time
		vc->freqdel = ins->fmdelay;
		for (i=0; i<SE_WAVES_INST; i++) {
			if (ins->resetwave[i]) memcpy(&vc->waves[i*256], &ins->waves[i*256],512);
		}
		vc->instrument = instNum-1;
	}
    
    // effects init
	for (f=0; f<SE_EFF_INST; f++) {
        Effect* fx; VoiceEffect* vfx;
        
        fx  = &song->instruments[vc->instrument]->fx[f];
        vfx = &vc->fx[f];
		if (fx->effecttype && fx->reseteffect) {
            vfx->osccnt = 0;
            vfx->fxcnt1 = 0;
            vfx->fxcnt2 = 0;
            vfx->y2     = 0;
            vfx->y1     = 0;
            vfx->Vhp    = 0;
            vfx->Vbp    = 0;
            vfx->Vlp    = 0;
		}
	}
}

void handleSong(JayPlayer* _this) {
	Subsong* subsong;
    int16_t i;
    
	if (!m_PlayFlg) return; 
	if (m_PauseFlg) return;
    subsong = song->subsongs[m_CurrentSubsong];
	m_PatternDelay--;
	if (m_PatternDelay==0) {
        if (!(m_PlayStep&1)) { // change the groove
            m_PlaySpeed = 8-subsong->groove;
        } else {
            m_PlaySpeed = 8+subsong->groove;
        }
		m_PatternDelay = m_PlaySpeed;
        
        for (i=0; i<subsong->nrofchans; i++) {
            Voice* vc = &m_ChannelData[i];
            
            vc->patpos++;
            //the ==-1 part is that the song counter always is 1 before the start...so if the song starts at the beginning, the pos is -1
            if (vc->patpos == subsong->orders[i][vc->songpos].patlen || vc->songpos == -1) {
                vc->patpos = 0;
                vc->songpos++;
            }
        }

        m_PlayStep++;
        if (m_PlayStep==64) {
            m_PlayStep=0;
            m_PlayPosition++;
        }

        //has endpos been reached?
        if (m_PlayPosition == subsong->endpos && m_PlayStep == subsong->endstep) {
            if (subsong->songloop) {  //does song loop?
                int32_t maat, pos, t;
                uint8_t isSkipLoop = 0;
                
                // now me must reset all the playpointers to the loop positions
                for (t=0; t<SE_NROFCHANS; t++) {
                    Order* orders = subsong->orders[t];
                    Voice* vc = &m_ChannelData[t];
                    int32_t endpos;
                    int32_t lastmaat;
                    
                    maat = 0;
                    pos = 0;
                    lastmaat=0;
                    
                    endpos = (subsong->looppos*64)+subsong->loopstep;
                    while (pos<256) {
                        if (maat > endpos) {
                            if (pos != endpos) pos--;
                            break;
                        }
                        lastmaat=maat;
                        maat+=orders[pos].patlen;
                        pos++;
                    }
                    //oops! starting position too far!
                    if (pos == 256) { //WARN: >= 256?
                        m_PlayFlg = 0;
                        isSkipLoop = 1;
                        break;
                    }

                    endpos -= lastmaat;
                    endpos &= 63;
                    
                    vc->songpos = pos;
                    vc->patpos = endpos;
                }
                if (isSkipLoop) {
                    m_PlayPosition = subsong->looppos;
                    m_PlayStep     = subsong->loopstep;
                }
            } else { // stop song
                m_PlayFlg  = 0;
                m_PauseFlg = 0;
                m_PlayPosition = subsong->songpos;
                m_PlayStep     = subsong->songstep;
            }
		}
	}
}

void handleScript(JayPlayer* _this, int32_t f,int32_t s, int32_t d, int32_t p, int32_t channr) { //note, script,dstnote,param,channr
    Voice* vc; Inst* ins; Subsong* subsong;
	int32_t a;
    
    vc = &m_ChannelData[channr];
	if(vc->instrument==-1) return; //no change
    ins = song->instruments[vc->instrument];
    subsong = song->subsongs[m_CurrentSubsong];
    
	switch(s) {
        default:
        case 0:
            return;
        case 1:  //pitch bend
            if (vc->bendtonote) { //hebben we al eens gebend?
                a = m_frequencyTable[ins->finetune][vc->bendtonote]; // begin frequentie
                vc->curnote = vc->bendtonote;
            } else {
                a = m_frequencyTable[ins->finetune][f]; // begin freqeuntie
            }
            vc->bendadd = 0;
            vc->destfreq = m_frequencyTable[ins->finetune][d] - a;
            vc->bendspd = p*20;
            vc->bendtonote = d;
            break;
        case 2:  //waveform
            if (d>15) d = 15;
            ins->waveform = d;
            break;
        case 3:  //wavelength
            d = (d>192 ? 256 : (d>96 ? 128 : (d>48 ? 64 : 32)));
            ins->wavelength = d;
            break;
        case 4:  //mastervol
            ins->mastervol = d;
            break;
        case 5:  //amwaveform
            if (d>15) d = 15;
            ins->amwave = d;
            break;
        case 6:  //amspd
            ins->amspd = d;
            break;
        case 7:  //amlooppoint
            ins->amlooppoint = d;
            break;
        case 8:  //finetune
            if (d>15) d = 15;
            ins->finetune = d;
            break;
        case 9:  //fmwaveform
            if (d>15) d = 15;
            ins->fmwave = d;
            break;
        case 10: //fmspd
            ins->fmspd = d;
            break;
        case 11: //fmlooppoint
            ins->fmlooppoint = d;
            break;
        case 12: //fmdelay
            ins->fmdelay = d;
            break;
        case 13: //arpeggio
            if (d>15) d = 15;
            ins->arpeggio = d;
            break;
            
        case 14: //fx 0 fxdstwave
            if (d>15) d = 15;
            ins->fx[0].dsteffect = d;
            break;
        case 15: //fx 0 fxsrcwave1
            if (d>15) d = 15;
            ins->fx[0].srceffect1 = d;
            break;
        case 16: //fx 0 fxsrcwave2
            if (d>15) d = 15;
            ins->fx[0].srceffect2 = d;
            break;
        case 17: //fx 0 fxoscwave
            if (d>15) d = 15;
            ins->fx[0].osceffect = d;
            break;
        case 18: //fx 0 effectvar1
            ins->fx[0].effectvar1 = d;
            break;
        case 19: //fx 0 effectvar2
            ins->fx[0].effectvar2 = d;
            break;
        case 20: //fx 0 effectspd
            ins->fx[0].effectspd = d;
            break;
        case 21: //fx 0 oscspd
            ins->fx[0].oscspd = d;
            break;
        case 22: //fx 0 oscflg
            if (d>1) d=1;
            ins->fx[0].oscflg = d;
            break;
        case 23: //fx 0 effecttype
            if (d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
            ins->fx[0].effecttype = d;
            break;
        case 24: //fx 0 reseteffect
            if (d>1) d=1;
            ins->fx[0].reseteffect = d;
            break;
            
        case 25: //fx 1 fxdstwave
            if (d>15) d = 15;
            ins->fx[1].dsteffect = d;
            break;
        case 26: //fx 1 fxsrcwave1
            if (d>15) d = 15;
            ins->fx[1].srceffect1 = d;
            break;
        case 27: //fx 1 fxsrcwave2
            if (d>15) d = 15;
            ins->fx[1].srceffect2 = d;
            break;
        case 28: //fx 1 fxoscwave
            if (d>15) d = 15;
            ins->fx[1].osceffect = d;
            break;
        case 29: //fx 1 effectvar1
            ins->fx[1].effectvar1 = d;
            break;
        case 30: //fx 1 effectvar2
            ins->fx[1].effectvar2 = d;
            break;
        case 31: //fx 1 effectspd
            ins->fx[1].effectspd = d;
            break;
        case 32: //fx 1 oscspd
            ins->fx[1].oscspd = d;
            break;
        case 33: //fx 1 oscflg
            if (d>1) d=1;
            ins->fx[1].oscflg = d;
            break;
        case 34: //fx 1 effecttype
            if (d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
            ins->fx[1].effecttype = d;
            break;
        case 35: //fx 1 reseteffect
            if (d>1) d=1;
            ins->fx[1].reseteffect = d;
            break;
            
        case 36: //fx 2 fxdstwave
            if (d>15) d = 15;
            ins->fx[2].dsteffect = d;
            break;
        case 37: //fx 2 fxsrcwave1
            if (d>15) d = 15;
            ins->fx[2].srceffect1 = d;
            break;
        case 38: //fx 2 fxsrcwave2
            if (d>15) d = 15;
            ins->fx[2].srceffect2 = d;
            break;
        case 39: //fx 2 fxoscwave
            if (d>15) d = 15;
            ins->fx[2].osceffect = d;
            break;
        case 40: //fx 2 effectvar1
            ins->fx[2].effectvar1 = d;
            break;
        case 41: //fx 2 effectvar2
            ins->fx[2].effectvar2 = d;
            break;
        case 42: //fx 2 effectspd
            ins->fx[2].effectspd = d;
            break;
        case 43: //fx 2 oscspd
            ins->fx[2].oscspd = d;
            break;
        case 44: //fx 2 oscflg
            if (d>1) d=1;
            ins->fx[2].oscflg = d;
            break;
        case 45: //fx 2 effecttype
            if (d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
            ins->fx[2].effecttype = d;
            break;
        case 46: //fx 2 reseteffect
            if (d>1) d=1;
            ins->fx[2].reseteffect = d;
            break;
            
        case 47: //fx 3 fxdstwave
            if (d>15) d = 15;
            ins->fx[3].dsteffect = d;
            break;
        case 48: //fx 3 fxsrcwave1
            if (d>15) d = 15;
            ins->fx[3].srceffect1 = d;
            break;
        case 49: //fx 3 fxsrcwave2
            if (d>15) d = 15;
            ins->fx[3].srceffect2 = d;
            break;
        case 50: //fx 3 fxoscwave
            if (d>15) d = 15;
            ins->fx[3].osceffect = d;
            break;
        case 51: //fx 3 effectvar1
            ins->fx[3].effectvar1 = d;
            break;
        case 52: //fx 3 effectvar2
            ins->fx[3].effectvar2 = d;
            break;
        case 53: //fx 3 effectspd
            ins->fx[3].effectspd = d;
            break;
        case 54: //fx 3 oscspd
            ins->fx[3].oscspd = d;
            break;
        case 55: //fx 3 oscflg
            if (d>1) d=1;
            ins->fx[3].oscflg = d;
            break;
        case 56: //fx 3 effecttype
            if (d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
            ins->fx[3].effecttype = d;
            break;
        case 57: //fx 3 reseteffect
            if (d>1) d=1;
            ins->fx[3].reseteffect = d;
            break;
            
        case 58: //resetwave 00
            if (d>1) d=1;
            ins->resetwave[0] = d;
            break;
        case 59: //resetwave 01
            if (d>1) d=1;
            ins->resetwave[1] = d;
            break;
        case 60: //resetwave 02
            if (d>1) d=1;
            ins->resetwave[2] = d;
            break;
        case 61: //resetwave 03
            if (d>1) d=1;
            ins->resetwave[3] = d;
            break;
        case 62: //resetwave 04
            if (d>1) d=1;
            ins->resetwave[4] = d;
            break;
        case 63: //resetwave 05
            if (d>1) d=1;
            ins->resetwave[5] = d;
            break;
        case 64: //resetwave 06
            if (d>1) d=1;
            ins->resetwave[6] = d;
            break;
        case 65: //resetwave 07
            if (d>1) d=1;
            ins->resetwave[7] = d;
            break;
        case 66: //resetwave 08
            if (d>1) d=1;
            ins->resetwave[8] = d;
            break;
        case 67: //resetwave 09
            if (d>1) d=1;
            ins->resetwave[9] = d;
            break;
        case 68: //resetwave 10
            if (d>1) d=1;
            ins->resetwave[10] = d;
            break;
        case 69: //resetwave 11
            if (d>1) d=1;
            ins->resetwave[11] = d;
            break;
        case 70: //resetwave 12
            if (d>1) d=1;
            ins->resetwave[12] = d;
            break;
        case 71: //resetwave 13
            if (d>1) d=1;
            ins->resetwave[13] = d;
            break;
        case 72: //resetwave 14
            if (d>1) d=1;
            ins->resetwave[14] = d;
            break;
        case 73: //resetwave 15
            if (d>1) d=1;
            ins->resetwave[15] = d;
            break;
            
        case 74: //Change bpm
            if (d<=10) d=10;
            if (d>220) d=220;
            subsong->songspd = d;

            float t;
            t=(float)subsong->songspd; //bpm
            t/=60.0;         //bps
            t*=32.0;
            m_TimeSpd=(int32_t)(44100.0/t);
            break;
        case 75: //Change Groove
            if (d>3) d=3;
            subsong->groove = d;
            break;
        case 76: //Fire External Event
            //TODO: add this effect
            break;
	}
}

void handlePattern(JayPlayer* _this, int32_t channr) {
    Subsong* subsong; Voice* vc;
	int32_t pat,off;
	int32_t f,d,s,p;
    
	if (m_PauseFlg) return;
	if (!m_PlayFlg) return; 
	if (m_PatternDelay != m_PlaySpeed) return;
    subsong = song->subsongs[m_CurrentSubsong];
    vc = &m_ChannelData[channr];
    
    if (subsong->mute[channr]==1) return;
    off = vc->patpos;
    pat = subsong->orders[channr][vc->songpos].patnr;

    //init instrument
	f = song->patterns[(pat*64)+off].srcnote;
	if (f) playInstrument(_this, channr, song->patterns[(pat*64)+off].inst, f);

    //handle special effects
	s = song->patterns[(pat*64)+off].script;
	d = song->patterns[(pat*64)+off].dstnote;
	p = song->patterns[(pat*64)+off].param;
	handleScript(_this, f, s, d, p, channr);
}

void advanceSong(JayPlayer* _this) {
	Subsong* subsong;
    int i;
    
    subsong = song->subsongs[m_CurrentSubsong];
	handleSong(_this);
	for (i=0; i < subsong->nrofchans; i++) {
		handlePattern(_this, i);
		if(m_ChannelData[i].instrument!=-1) {// mute?
			handleInstrument(_this, i); //do volume and pitch things
			handleEffects(_this, i);    //do channel effects 
		}
	}
}

//void SoundEngine::PlayPattern(int32_t PatternNr)

//---------------------API

int jaytrax_loadSong(JayPlayer* _this, Song* sng) {
    int version = sng->header.mugiversion;
    if (version >= 3456 && version <= 3457) {
		song = sng;
		return 1;
	}
	printf("Bad song ver: %d\n", version);
	return 0;
}

// This function ensures that the play routine is called properly and everything is initialized in a good way
void jaytrax_playSubSong(JayPlayer* _this, int subsongnr) {
	int maat,pos,t;
	Order* order; Subsong* subsong;

	m_CurrentSubsong = subsongnr;
    subsong = song->subsongs[m_CurrentSubsong];
	clearSoundBuffers(_this);

	for(t=0;t < SE_NROFCHANS;t++) {
		int endpos;
		int lastmaat;
        
		maat = pos = lastmaat = 0;
		order = subsong->orders[t];
		endpos = (subsong->songpos*64)+subsong->songstep -1; //minus 1 because we immediately start with the new note
		while (pos<256) {
			if (maat >= endpos) {
				if(pos != endpos) pos--;
				break;
			}
			lastmaat=maat;
			maat+=order[pos].patlen;
			pos++;
		}
        //oops! starting position too far!
		if (pos == 256) return; //WARN: >= 256?

		endpos-=lastmaat;
        //endpos-=maat;
		endpos &=63;
		
		m_ChannelData[t].songpos = pos;
		m_ChannelData[t].patpos  = endpos;
	}

	m_PatternDelay = 1;
	m_PlayFlg = 1;
	m_PauseFlg = 0;
	m_PlaySpeed = 8+subsong->groove;
    //m_PlaySpeed = 8;

	if (subsong->songspd) {
		float t;
		t=(float)subsong->songspd; //bpm
		t/=60.0;                   //bps
		t*=32.0;
		m_TimeSpd=(int)(44100.0/t);
		m_TimeCnt = m_TimeSpd;
	}

	if(subsong->songstep == 0) {
		m_PlayPosition=subsong->songpos-1;
	} else {
		m_PlayPosition=subsong->songpos;
	}
	m_PlayStep=subsong->songstep-1;
	m_PlayStep &= 63;
}

void jaytrax_stopSong(JayPlayer* _this) {
    Subsong* subsong;
    
    subsong = song->subsongs[m_CurrentSubsong];
	m_PlayFlg = 0;
	m_PauseFlg = 0;
	if(song) {
		m_PlayPosition = subsong->songpos;
		m_PlayStep     = subsong->songstep;
	}
}

void jaytrax_pauseSong(JayPlayer* _this) {
	m_PauseFlg = 1;
}

void jaytrax_continueSong(JayPlayer* _this) {
	m_PauseFlg = 0;
}

JayPlayer* jaytrax_init() {
    JayPlayer* _this = NULL;
    if (!isStaticInit) {
        int32_t i, j;
        double f,y;
        //freq and finetune table
        y=2;
        for (j=0;j<SE_NROFFINETUNESTEPS;j++) {
            for (i=0;i<128;i++) {
                // formule = wortel(x/12)
                f=((i+3)*16)-j;   // we beginnen met de a want die is 440hz
                f=f/192;
                f=pow(y,f);
                f=f*220;
                m_frequencyTable[j][i]=(int32_t)(f+0.5);
            }
        }
        
        //sine table
        for (i=0;i<256;i++) {
            m_sineTab[i] = (int16_t)(sin((M_PI*i)/128)*32760);
        }
        //empty wave
        memset(&m_emptyWave, 0, sizeof(int16_t)*256);
        
        isStaticInit = 1;
    }
    
    //------------------
	m_OverlapCnt = 0;
	
	m_PlayPosition = 0;	 // waar is de song nu?
	m_PlayStep = 0;
	m_CurrentSubsong = 0; 
	m_PlayFlg = 0; 
	m_PauseFlg = 0;

	m_PatternDelay = 0;
	m_PlaySpeed = 0;
	m_MasterVolume = 0;

	song = NULL;

	//initialize renderingcounters and speed
	m_TimeCnt = 2200;
	m_TimeSpd = 2200;
    
	clearSoundBuffers(_this);
	m_DelayCnt=0;
    return NULL;
}

//internal rendering structure to speed up innerloop
typedef struct ChannelVar ChannelVar;
struct ChannelVar {
	int8_t   issample;
	int16_t* wavepnt;
	uint16_t localwavepos;
	int16_t  panl;
	int16_t  panr;
	int16_t  echovolumel;
	int16_t  echovolumer;
	int16_t  freqoffset;
	int16_t  wavelength;
	int32_t  synthPos; //WARN: long
	int32_t  samplepos; //WARN: long
	int16_t  curdirecflg;
	int32_t  waveend;
	int16_t  loopflg;
	int16_t  bidirecflg;
	int32_t  waveloop;
	int16_t  volume;
	int16_t  echovolume;
};

// Optimized stereo renderer... No high quality overlapmixbuf
void jaytrax_renderChunk(JayPlayer* _this, int16_t* outbuf, int32_t nrofsamples, int32_t frequency) {
    Subsong* subsong; Voice* vc;
	ChannelVar channelvars[SE_NROFCHANS];
	ChannelVar* curvars;
	int16_t i,p,c;
	int32_t r;
	int16_t nos;										//number of samples
	int32_t lsample,rsample,echosamplel,echosampler;	//current state of samples
	int16_t amplification;							// amount to amplify afterwards
	uint16_t echodelaytime;					//delaytime for echo (differs for MIDI or songplayback)
    int16_t m_NrOfChannels;

    subsong = song->subsongs[m_CurrentSubsong];
	m_NrOfChannels = subsong->nrofchans;
	// we calc nrofsamples samples in blocks of 'm_TimeCnt' big (de songspd)

	r=0;
	while(nrofsamples>0) {
        // 2 possibilities:  we calc a complete m_TimeCnt block...or the last part of a m_TimeCnt block...
		if(m_TimeCnt<nrofsamples) {
			nos = m_TimeCnt;   //Complete block
			m_TimeCnt=(m_TimeSpd*frequency)/44100;
		} else {
			nos = nrofsamples;  //Last piece
			m_TimeCnt=m_TimeCnt-nos; 
		}
		nrofsamples-=nos;

		// preparation of wave pointers and freq offset
		for(i=0; i<m_NrOfChannels; i++) {
			int wavenr, instnr;
            
            vc = &m_ChannelData[i];
			instnr = vc->instrument;
			if (instnr == -1) { // mute?
				channelvars[i].wavepnt=&m_emptyWave[0];
				channelvars[i].issample=0;
			} else {
				if(vc->sampledata) {
					channelvars[i].samplepos   = vc->samplepos;
					channelvars[i].wavepnt     = (short *)vc->sampledata;
					channelvars[i].waveloop    = vc->looppoint; 
					channelvars[i].waveend     = vc->endpoint; 
					channelvars[i].loopflg     = vc->loopflg;
					channelvars[i].bidirecflg  = vc->bidirecflg;
					channelvars[i].curdirecflg = vc->curdirecflg;
					channelvars[i].issample=1;
				} else {
                    Inst* inst = song->instruments[instnr];
					wavenr = inst->waveform;
					channelvars[i].wavepnt=&vc->waves[256*wavenr];
					channelvars[i].wavelength=((inst->wavelength-1)<<8)+255;  //fixed point 8 bit (last 8 bits should be set)
					channelvars[i].issample=0;
				}
			}

			channelvars[i].localwavepos = vc->synthPos;

            //calcen frequency
			if(vc->curfreq<10)vc->curfreq=10;
			channelvars[i].freqoffset = (256*vc->curfreq)/frequency;
			channelvars[i].volume = (vc->curvol+10000)/39;
			if(channelvars[i].volume>256) channelvars[i].volume=256;

			if(vc->curpan==0) { //panning?
				channelvars[i].panl=256; //geen panning
				channelvars[i].panr=256;
			} else {
				if(vc->curpan>0) {
					channelvars[i].panl = 256-(vc->curpan);
					channelvars[i].panr = 256;
				} else {
					channelvars[i].panr = 256+(vc->curpan);
					channelvars[i].panl=256;
				}
			}

			channelvars[i].echovolume = subsong->delayamount[i];

			//stereo optimimalisation 1... precalc panning multiplied with mainvolume
			channelvars[i].panl = (channelvars[i].panl * channelvars[i].volume)>>8;
			channelvars[i].panr = (channelvars[i].panr * channelvars[i].volume)>>8;

			//stereo optimimalisation 1... precalc panning multiplied with echo
			channelvars[i].echovolumel = (channelvars[i].echovolume * channelvars[i].panl)>>8;
			channelvars[i].echovolumer = (channelvars[i].echovolume * channelvars[i].panr)>>8;
		}
		amplification = subsong->amplification;
		echodelaytime = subsong->delaytime;

		if(outbuf) { // dry rendering? or actual rendering?
			if(m_NrOfChannels==0) {
				for(i=0;i<nrofsamples;i++) { //c lean renderbuffer
					outbuf[r++] = 0;
					outbuf[r++] = 0;
				}
			} else {
				for(p=0;p<nos;p++) {
					lsample=0;
					rsample=0;
					echosamplel=0;
					echosampler=0;
					for(c=0;c<m_NrOfChannels;c++) {
						curvars = &channelvars[c];
						int b;
						if (!curvars->issample) {
							b=curvars->wavepnt[curvars->localwavepos>>8];
							lsample += (b*curvars->panl)>>8;
							rsample += (b*curvars->panr)>>8;
							echosamplel += (b*curvars->echovolumel)>>8;
							echosampler += (b*curvars->echovolumer)>>8;
							curvars->localwavepos+=curvars->freqoffset;
							curvars->localwavepos&=curvars->wavelength;
						} else {
							if(curvars->samplepos!=0xffffffff) {
								b=curvars->wavepnt[curvars->samplepos>>8];
								lsample += (b*curvars->panl)>>8;
								rsample += (b*curvars->panr)>>8;
								echosamplel += (b*curvars->echovolumel)>>8;
								echosampler += (b*curvars->echovolumer)>>8;
								
								if(curvars->curdirecflg==0) { //richting end?
									curvars->samplepos+=(int)curvars->freqoffset;
									
									if(curvars->samplepos>=curvars->waveend) {
										if(curvars->loopflg==0) { //one shot?
											curvars->samplepos=0xffffffff;
										} else { // looping
											if(curvars->bidirecflg==0) { //1 richting??
												curvars->samplepos-=(curvars->waveend-curvars->waveloop);
											} else {
												curvars->samplepos-=(int)curvars->freqoffset;
												curvars->curdirecflg=1; //richting omdraaien
											}
										}
									}
								} else { // weer bidirectioneel terug naar looppoint
									curvars->samplepos-=(int)curvars->freqoffset;
									
									if(curvars->samplepos<=curvars->waveloop) {
										curvars->samplepos+=(int)curvars->freqoffset;
										curvars->curdirecflg=0; //richting omdraaien
									}
								}
							}
						}
					}
					
					lsample = ((lsample/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
					rsample = ((rsample/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;
					
					// if we are still in the bottom SE_OVERLAP part then we have to mix the previous wave through (to avoid clicks)
					
					lsample *= amplification;
					lsample /= 100;
					short smpl;
					if(lsample>32760) lsample=32760;
					if(lsample<-32760) lsample=-32760;
					
					rsample *= amplification;
					rsample /= 100;
					if(rsample>32760) rsample=32760;
					if(rsample<-32760) rsample=-32760;
					
					smpl=lsample;
					outbuf[r++]=smpl;
					
					smpl=rsample;
					outbuf[r++]=smpl;  //stereo
					
					
					m_LeftDelayBuffer[m_DelayCnt]=((echosamplel/m_NrOfChannels)+m_LeftDelayBuffer[m_DelayCnt])/2;
					m_RightDelayBuffer[m_DelayCnt]=((echosampler/m_NrOfChannels)+m_RightDelayBuffer[m_DelayCnt])/2;
					m_DelayCnt++;
					m_DelayCnt%=echodelaytime/(44100/frequency);
					
				}
			}
		} else { // was this a dry render?  then we should increment r anyway.
			r += (nos*2);   // times 2 since we must think stereo
		}

		for(i=0; i<m_NrOfChannels; i++) {
            Voice* vc = &m_ChannelData[i];
            vc->synthPos = channelvars[i].localwavepos;
			if(channelvars[i].issample) {
				vc->samplepos   = channelvars[i].samplepos;
				vc->curdirecflg = channelvars[i].curdirecflg;
			}
		}

		if(m_TimeCnt==((m_TimeSpd*frequency)/44100)) {
			unsigned short tempwavepos[SE_NROFCHANS];		// last position per channel in the waveform(8bit fixed point)(temporarily storage)
			unsigned short tempdelaycnt;		// ditto

			m_OverlapCnt=0; //reset overlap counter
			tempdelaycnt=m_DelayCnt;
			for(c=0;c<SE_NROFCHANS;c++)
			{
				tempwavepos[c] = channelvars[c].localwavepos;
			}

			m_DelayCnt=tempdelaycnt;

			for(c=0;c<SE_NROFCHANS;c++) {
                m_ChannelData[c].synthPos = tempwavepos[c];
			}
			
			//Update song pointers
			advanceSong(_this);
		}
	}
}
