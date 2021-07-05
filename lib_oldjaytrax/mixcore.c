#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#include "mixcore.h"
#include "jaytrax.h"

// ITP_[number of taps]_[input sample width]_[fractional precision]_[readable name]
#define ITP_T02_S16_I08_LINEAR(P, F) ((P[0]*(0x100-F) + P[1] * F)>>8)
#define ITP_T03_S16_I15_QUADRA(P, F) (((((((((P[0] + P[2]) >> 1) - P[1]) * F) >> 16) + P[1]) - ((P[2] + P[0] + (P[0] << 1)) >> 2)) * F) >> 14) + P[0]
#define ITP_T04_SXX_F01_CUBIC(P, F)  (P[1] + 0.5 * F*(P[2] - P[0] + F*(2.0 * P[0] - 5.0 * P[1] + 4.0 * P[2] - P[3] + F * (3.0 * (P[1] - P[2]) + P[3] - P[0]))))


//---------------------interpolators
#define GET_PT(x) buf[((pos + ((x)<<8)) & sizeMask)>>8]

static int32_t itpNone(int16_t* buf, int32_t pos, int32_t sizeMask) {
    return 0;(void)buf;(void)pos;(void)sizeMask;
}

static int32_t itpNearest(int16_t* buf, int32_t pos, int32_t sizeMask) {
    return GET_PT(0);
}

static int32_t itpLinear(int16_t* buf, int32_t pos, int32_t sizeMask) {
    int32_t p[2];
    int32_t frac = pos & 0xFF;
    
    p[0] = GET_PT(0);
    p[1] = GET_PT(1);
    
    return ITP_T02_S16_I08_LINEAR(p, frac);
}

static int32_t itpQuad(int16_t* buf, int32_t pos, int32_t sizeMask) {
    int32_t p[3];
    int32_t frac = (pos & 0xFF)<<7;
    
    p[0] = GET_PT(0);
    p[1] = GET_PT(1);
    p[2] = GET_PT(2);
    
    return ITP_T03_S16_I15_QUADRA(p, frac);
}

static int32_t itpCubic(int16_t* buf, int32_t pos, int32_t sizeMask) {
    int32_t p[4];
    double frac = (double)(pos & 0xFF)/255;
    
    p[0] = GET_PT(0);
    p[1] = GET_PT(1);
    p[2] = GET_PT(2);
    p[3] = GET_PT(3);
    
    return ITP_T04_SXX_F01_CUBIC(p, frac);
}

Interpolator interps[INTERP_COUNT] = {
    {ITP_NONE,      0, &itpNone,    "None"},
    {ITP_NEAREST,   1, &itpNearest, "Nearest"},
    {ITP_LINEAR,    2, &itpLinear,  "Linear"},
    {ITP_QUADRATIC, 3, &itpQuad,    "Quadratic"},
    {ITP_CUBIC,     4, &itpCubic,   "Cubic"},
    //{ITP_BLEP,     -1, &mixSynthNearest, &mixSampNearest, "BLEP"} //BLEP needs variable amount of taps
};

static void smpCpyFrw(int16_t* destination, const int16_t* source, int32_t num) {
    memcpy(destination, source, num * sizeof(int16_t));
}

static void smpCpyRev(int16_t* destination, const int16_t* source, int32_t num) {
    for (int i=0; i < num; i++) {
        destination[i] = source[num-1 - i];
    }
}

//---------------------API

uint8_t jaymix_setInterp(Interpolator** out, uint8_t id) {
    for (int8_t i=0; i<INTERP_COUNT; i++) {
        if (interps[i].id == id) {
            *out = &interps[i];
            return 1;
        }
    }
    return 0;
}

void jaymix_mixCore(JT1Player* SELF, int32_t numSamples) {
    int32_t  tempBuf[MIXBUF_LEN];
    int32_t  ic, is, doneSmp;
    int32_t* outBuf = &SELF->tempBuf[0];
    int32_t chanNr = SELF->subsong->nrofchans;
    
    assert(numSamples <= MIXBUF_LEN);
    memset(&outBuf[0], 0, numSamples * MIXBUF_NR * sizeof(int32_t));
    
    
    for (ic=0; ic < chanNr; ic++) {
        JT1Voice* vc = &SELF->voices[ic];
        int32_t (*fItp) (int16_t* buf, int pos, int sizeMask);
        int32_t loopLen = vc->endpoint - vc->looppoint;
        
        doneSmp = 0;
        if (1 && vc->sampledata) { //sample render mark I-A
            
            if (!vc->wavePtr) continue;
            if (vc->samplepos < 0) continue;
            fItp = SELF->itp->fItp;
            
            while (doneSmp < numSamples) {
                int32_t delta, dif, nos;
                int32_t maxSmp = numSamples - doneSmp;
                
                if (vc->curdirecflg) {
                    dif   = vc->samplepos - vc->looppoint;
                    delta = vc->freqOffset * -1;
                } else {
                    dif = (vc->endpoint - 1) - vc->samplepos;
                    delta = vc->freqOffset * 1;
                }
                
                nos = ((dif ^ 0xFF) / vc->freqOffset) + 1;
                if (nos > maxSmp) nos = maxSmp;
                if (!nos) printf("a");
                
                //playback of sample
                for (is=0; is < nos; is++) {
                    //assert(CHKPOS(vc));
                    //this is wrong because it reads outside loops instead of wrapping taps
                    tempBuf[doneSmp++] = vc->wavePtr[vc->samplepos>>8]; //fItp(vc->wavePtr, vc->samplepos, 0xFFFFFFFF);
                    vc->samplepos += delta;
                }
                vc->samplepos += nos <= 0 ? delta : 0; //this happens at the edge of a sample
                
                if (vc->curdirecflg) { //backwards
                    if (vc->samplepos < vc->looppoint) {
                        vc->samplepos  += vc->freqOffset;
                        vc->curdirecflg = 0;
                    }
                } else { // forwards
                    if (vc->samplepos >= vc->endpoint) {
                        if (vc->loopflg) { //has loop
                            if(vc->bidirecflg) { //bidi
                                vc->samplepos -= vc->freqOffset;
                                vc->curdirecflg  = 1;
                            } else { //straight
                                vc->samplepos -= loopLen;
                            }
                        } else { //oneshot
                            vc->samplepos = -1;
                            break;
                        }
                    }
                }
            }
        } else { //synth render
            int32_t nos;
            
            if (!vc->wavePtr) {
                //original replayer plays through an empty wave
                vc->synthPos += vc->freqOffset * numSamples;
                vc->synthPos &= vc->waveLength;
                continue;
            }
            fItp = SELF->itp->fItp;
            
            //loop unroll optimization
            nos = numSamples;
            if (nos&1) {
                tempBuf[doneSmp++] = fItp(vc->wavePtr, vc->synthPos, vc->waveLength);
                vc->synthPos += vc->freqOffset;
                vc->synthPos &= vc->waveLength;
                nos--;
            }
            for(is=0; is < nos; is+=2) {
                tempBuf[doneSmp++] = fItp(vc->wavePtr, vc->synthPos, vc->waveLength);
                vc->synthPos += vc->freqOffset;
                vc->synthPos &= vc->waveLength;
                tempBuf[doneSmp++] = fItp(vc->wavePtr, vc->synthPos, vc->waveLength);
                vc->synthPos += vc->freqOffset;
                vc->synthPos &= vc->waveLength;
            }
        }
        
        for(is=0; is < doneSmp; is++) {
            int32_t samp, off;
            
            samp = tempBuf[is];
            off  = is * MIXBUF_NR;
            outBuf[off + BUF_MAINL] += (samp * vc->gainMainL)>>8;
            outBuf[off + BUF_MAINR] += (samp * vc->gainMainR)>>8;
            outBuf[off + BUF_ECHOL] += (samp * vc->gainEchoL)>>8;
            outBuf[off + BUF_ECHOR] += (samp * vc->gainEchoR)>>8;
        }
    }
}