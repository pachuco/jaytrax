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
// ITP_[number of taps]_[input sample width]_[fractional precision]_[readable name]
#define ITP_T02_S16_I08_LINEAR(P, F) ((P[0]*(0x100-F) + P[1] * F)>>8)
#define ITP_T03_S16_I15_QUADRA(P, F) (((((((((P[0] + P[2]) >> 1) - P[1]) * F) >> 16) + P[1]) - ((P[2] + P[0] + (P[0] << 1)) >> 2)) * F) >> 14) + P[0]
#define ITP_T04_SXX_F01_CUBIC(P, F)  (P[1] + 0.5 * F*(P[2] - P[0] + F*(2.0 * P[0] - 5.0 * P[1] + 4.0 * P[2] - P[3] + F * (3.0 * (P[1] - P[2]) + P[3] - P[0]))))
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
#define SYN_GETPT(x) vc->wavePtr[((vc->synthPos + ((x)<<8)) & vc->waveLength)>>8]

int32_t mixSynthNone(Voice* vc, int32_t* p) {
    return 0;
}

int32_t mixSynthNearest(Voice* vc, int32_t* p) {
    int32_t x;
    
    x = SYN_GETPT(0);
    return x;
}

int32_t mixSynthLinear(Voice* vc, int32_t* p) {
    int32_t x;
    int32_t frac = vc->synthPos & 0xFF;
    p[0] = SYN_GETPT(0);
    p[1] = SYN_GETPT(1);
    
    x = ITP_T02_S16_I08_LINEAR(p, frac);
    return x;
}

int32_t mixSynthQuad(Voice* vc, int32_t* p) {
    int32_t x;
    int32_t frac = (vc->synthPos & 0xFF)<<7;
    p[0] = SYN_GETPT(0);
    p[1] = SYN_GETPT(1);
    p[2] = SYN_GETPT(2);
    
    x  = ITP_T03_S16_I15_QUADRA(p, frac);
    return x;
}

int32_t mixSynthCubic(Voice* vc, int32_t* p) {
    int32_t x;
    double frac = (double)(vc->synthPos & 0xFF)/255;
    
    p[0] = SYN_GETPT(0);
    p[1] = SYN_GETPT(1);
    p[2] = SYN_GETPT(2);
    p[3] = SYN_GETPT(3);
    
    x = ITP_T04_SXX_F01_CUBIC(p, frac);
    return x;
}

//---------------------API

void jaymix_mixCore(JayPlayer* THIS, int16_t numSamples) {
    int32_t  tempBuf[MIXBUF_LEN];
    int16_t  ic, is, actuallyRendered;
    int32_t* outBuf = &THIS->tempBuf[0];
    int16_t  chanNr = THIS->m_subsong->nrofchans;
    int32_t  pBuf[MAX_TAPS];
    
    assert(numSamples <= MIXBUF_LEN);
    memset(&outBuf[0], 0, numSamples * MIXBUF_NR * sizeof(int32_t));
    
    is = 0;
    for (ic=0; ic < chanNr; ic++) {
        Voice* vc = &THIS->m_ChannelData[ic];
        int32_t (*f_interp) (Voice* vc, int32_t* pBuf);
        int32_t* pos;
        
        if (vc->isSample) { //sample render
            if (!vc->wavePtr) continue;
            if (vc->samplepos < 0) continue;
            f_interp = &mixSampNearest;
            
            for(is=0; is < numSamples; is++) {
                int32_t samp;
                
                samp = f_interp(vc, &pBuf);
                tempBuf[is] = samp;
                
                if (!doPosSamp(vc)) break;
            }
        } else { //synth render
            if (!vc->wavePtr) {
                //original replayer plays through an empty wave
                vc->synthPos += vc->freqOffset * numSamples;
                vc->synthPos &= vc->waveLength;
                continue;
            }
            f_interp = &mixSynthCubic;
            
            for(is=0; is < numSamples; is++) {
                int32_t samp;
                
                samp = f_interp(vc, &pBuf);
                tempBuf[is] = samp;
                
                vc->synthPos += vc->freqOffset;
                vc->synthPos &= vc->waveLength;
            }
        }
        
        actuallyRendered = is;
        for(is=0; is < actuallyRendered; is++) {
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