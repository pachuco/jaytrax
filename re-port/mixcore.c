#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "mixcore.h"
#include "jaytrax.h"

uint8_t doPosSamp(Voice* vc) {
    if (vc->curdirecflg == 0) { //going forwards
        vc->samplepos += vc->freqOffset;
        
        if (vc->samplepos >= vc->endpoint) {
            if (vc->loopflg == 0) { //one shot
                vc->samplepos = -1;
                return 0;
            } else { // looping
                if(vc->bidirecflg == 0) { //straight loop
                    vc->samplepos   -= (vc->endpoint - vc->looppoint);
                } else { //bidi loop
                    vc->samplepos   -= vc->freqOffset;
                    vc->curdirecflg  = 1;
                }
            }
        }
    } else { //going backwards
        vc->samplepos -= vc->freqOffset;
        
        if (vc->samplepos <= vc->looppoint) {
            vc->samplepos  += vc->freqOffset;
            vc->curdirecflg = 0;
        }
    }
    return 1;
}

uint8_t doPosSynt(Voice* vc) {
    vc->synthPos += vc->freqOffset;
    vc->synthPos &= vc->waveLength;
    return 1;
}

//---------------------interpolators for sample

int32_t mixSampNone(Voice* vc, int32_t* p) {
    return 0;
}

int32_t mixSampNearest(Voice* vc, int32_t* p) {
    return vc->wavePtr[vc->samplepos>>8];
}

int32_t mixSampLinear(Voice* vc, int32_t* p) {
    return 0;
}

int32_t mixSampQuad(Voice* vc, int32_t* p) {
    return 0;
}

int32_t mixSampCubic(Voice* vc, int32_t* p) {
    return 0;
}

//---------------------interpolators for synth
#define SYN_GETPT(x) vc->wavePtr[((vc->synthPos+(x)) & vc->waveLength)>>8]
#define INTERPOLATE16(s1, s2, s3, f) /* in: int32_t s1,s2,s3 = -32768..32767 | f = 0..65535 (frac) | out: s1 = -32768..32767 */ \
{ \
    int32_t frac, s4; \
    \
    frac = (f) >> 1; \
    s4 = (s1 + s3) >> 1; \
    s4 -= s2; \
    s4 = (s4 * frac) >> 16; \
    s3 += s1; \
    s1 += s1; \
    s3 = (s3 + s1) >> 2; \
    s1 >>= 1; \
    s4 += s2; \
    s4 -= s3; \
    s4 = (s4 * frac) >> 14; \
    s1 += s4; \
} \

int32_t mixSyntNone(Voice* vc, int32_t* p) {
    return 0;
}

int32_t mixSyntNearest(Voice* vc, int32_t* p) {
    return SYN_GETPT(0);
}

int32_t mixSyntLinear(Voice* vc, int32_t* p) {
    int32_t frac = vc->synthPos & 0xFF;
    p[0] = SYN_GETPT(0);
    p[1] = SYN_GETPT(1);
    return (p[0] * (0x100-frac) + p[1] * frac)>>8;
}

int32_t mixSyntQuad(Voice* vc, int32_t* p) {
    p[0] = SYN_GETPT(-1);
    p[1] = SYN_GETPT(0);
    p[2] = SYN_GETPT(1);
    
    
    int32_t frac, x;
    frac = ((vc->samplepos<<8) & 0xFFFF) >> 1;
    INTERPOLATE16(p[0], p[1], p[2], frac); x = p[0];
    //x = ((((((((p[0] + p[2]) >> 1) - p[1]) * frac) >> 16) + p[1] - (p[2] + p[0] * 3) >> 2) * frac) >> 14) + ((p[0] * 2) >> 1);
    return x;
}

int32_t mixSyntCubic(Voice* vc, int32_t* p) {
    double frac = (double)(vc->synthPos & 0xFF)/255;
    
    p[0] = SYN_GETPT(-1);
    p[1] = SYN_GETPT(0);
    p[2] = SYN_GETPT(1);
    p[3] = SYN_GETPT(2);
    
    return p[1] + 0.5 * frac*(p[2] - p[0] + frac*(2.0*p[0] - 5.0*p[1] + 4.0*p[2] - p[3] + frac*(3.0*(p[1] - p[2]) + p[3] - p[0])));
}

//---------------------API

void jaymix_mixCore(JayPlayer* THIS, int16_t numSamples) {
    int16_t  ic, is;
    int32_t* tempBuf = &THIS->tempBuf[0];
    int16_t  chanNr = THIS->m_subsong->nrofchans;
    int32_t  pBuf[MAX_TAPS];
    
    assert(numSamples <= MIXBUF_LEN);
    memset(&tempBuf[0], 0, numSamples * MIXBUF_NR * sizeof(int32_t));
    
    for (ic=0; ic < chanNr; ic++) {
        Voice* vc = &THIS->m_ChannelData[ic];
        int32_t (*f_interp) (Voice* vc, int32_t* pBuf);
        uint8_t (*f_doPos)  (Voice* vc);
        int32_t* pos;
        
        if (!vc->wavePtr) continue;
        if (vc->isSample) {
            if (vc->samplepos < 0) continue;
            f_interp = &mixSampNearest;
            f_doPos = &doPosSamp;
        } else {
            f_interp = &mixSyntQuad;
            f_doPos = &doPosSynt;
        }
        
        for(is=0; is < numSamples; is++) {
            int32_t samp, off;
            
            samp = f_interp(vc, &pBuf);
            off  = MIXBUF_NR * is;
            tempBuf[off + 0] += (samp * vc->gainMainL)>>8;
            tempBuf[off + 1] += (samp * vc->gainMainR)>>8;
            tempBuf[off + 2] += (samp * vc->gainEchoL)>>8;
            tempBuf[off + 3] += (samp * vc->gainEchoR)>>8;
            
            if (!f_doPos(vc)) break;
        }
    }
}