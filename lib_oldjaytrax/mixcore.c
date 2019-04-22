#include <stdint.h>
#include <string.h>
#include <assert.h>
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

void jaymix_mixCore(JT1Player* THIS, int32_t numSamples) {
    int32_t  tempBuf[MIXBUF_LEN];
    int32_t  ic, is, doneSmp;
    int32_t* outBuf = &THIS->tempBuf[0];
    int32_t chanNr = THIS->subsong->nrofchans;
    
    assert(numSamples <= MIXBUF_LEN);
    memset(&outBuf[0], 0, numSamples * MIXBUF_NR * sizeof(int32_t));
    
    
    for (ic=0; ic < chanNr; ic++) {
        JT1Voice* vc = &THIS->voices[ic];
        int32_t (*fItp) (int16_t* buf, int pos, int sizeMask);
        
        doneSmp = 0;
        if (vc->isSample) { //sample render mark I
            int32_t nos;
            
            if (!vc->wavePtr) continue;
            if (vc->samplepos < 0) continue;
            fItp = THIS->itp->fItp;
            
            while (doneSmp < numSamples) {
                int32_t delta, fracMask, dif;
                int32_t maxSmp = numSamples - doneSmp;
                
                if (vc->curdirecflg) { //backwards
                    dif   = vc->samplepos - vc->looppoint - 1;
                    delta = vc->freqOffset * -1;
                    fracMask = 0xFF;
                } else { //forwards
                    dif = vc->endpoint - vc->samplepos - 1;
                    delta = vc->freqOffset;
                    fracMask = 0x00;
                }
                dif = dif > 0 ? dif : 1;
                nos = (dif / vc->freqOffset) + 1;
                if (nos > maxSmp) nos = maxSmp;
                
                //loop unroll optimization
                if (nos&1) {
                    tempBuf[doneSmp] = fItp(vc->wavePtr, vc->samplepos ^ fracMask, 0xFFFFFFFF); //vc->wavePtr[vc->samplepos>>8];
                    vc->samplepos += delta;
                    doneSmp++;
                }
                nos >>= 1;
                for (is=0; is < nos; is++) {
                    tempBuf[doneSmp + (is << 1) + 0] = fItp(vc->wavePtr, vc->samplepos ^ fracMask, 0xFFFFFFFF); //vc->wavePtr[vc->samplepos>>8];
                    vc->samplepos += delta;
                    tempBuf[doneSmp + (is << 1) + 1] = fItp(vc->wavePtr, vc->samplepos ^ fracMask, 0xFFFFFFFF); //vc->wavePtr[vc->samplepos>>8];
                    vc->samplepos += delta;
                }
                doneSmp += nos << 1;
                
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
                                vc->samplepos -= (vc->endpoint - vc->looppoint);
                            }
                        } else { //oneshot
                            vc->samplepos = -1;
                            break;
                        }
                    }
                }
            }
        } else if (0 && vc->isSample) { //experimental sample render mark II
            int32_t nos;
            
            if (!vc->wavePtr) continue;
            if (vc->samplepos < 0) continue;
            fItp = THIS->itp->fItp;
            
            while (doneSmp < numSamples) {
                int32_t nosSpool, pos;
                
                nos = numSamples - doneSmp;
                //make sure we are not going outside the sample spool
                while ((nosSpool = (((nos - 1) * (vc->freqOffset))>>8) + THIS->itp->numTaps) >= SAMPSPOOLSIZE) nos--;
                
                //unroll sample into spool
                pos = vc->samplepos;
                for (is=0; is < nosSpool; is++) {
                    THIS->sampleSpool[is] = vc->wavePtr[pos>>8];
                    pos += vc->curdirecflg ? -0x100 : 0x100;
                    
                    //TODO: per-chunkofsample instead of per-sample, like Mark I renderer
                    if (vc->curdirecflg) { //backwards
                        if (pos < vc->looppoint) {
                            pos += 0x100;
                            vc->curdirecflg = 0;
                        }
                    } else { // forwards
                        if (pos >= vc->endpoint) {
                            if (vc->loopflg) { //has loop
                                if(vc->bidirecflg) { //bidi
                                    pos -= 0x100;
                                    vc->curdirecflg = 1;
                                } else { //straight
                                    pos -= (vc->endpoint - vc->looppoint);
                                }
                            } else { //oneshot
                                while (is < nosSpool) THIS->sampleSpool[is++] = 0;
                            }
                        }
                    }
                }
                pos = vc->samplepos;
                
                //playback of sample
                for (is=0; is < nos; is++) {
                    tempBuf[doneSmp] = fItp(THIS->sampleSpool, vc->samplepos - pos, SAMPSPOOLSIZE<<8);
                    vc->samplepos += vc->freqOffset;
                    doneSmp++;
                }
                
                //fix samplepos AND direction after we are done
                if (vc->samplepos >= vc->endpoint) {
                    if (vc->loopflg) { //it loops
                        int32_t loopLen = vc->endpoint - vc->looppoint;
                        
                        if (vc->bidirecflg && (((vc->samplepos - vc->looppoint) / loopLen) & 1)) { //bidi and backwards
                            vc->samplepos = vc->endpoint - ((vc->samplepos - vc->endpoint) % loopLen + 1);
                            vc->curdirecflg = 1;
                        } else { //forwards
                            vc->samplepos = vc->looppoint + ((vc->samplepos - vc->looppoint) % loopLen);
                            vc->curdirecflg = 0;
                        }
                    } else { //oneshot
                        vc->samplepos = -1;
                    }
                }
            }
            
        } else if (0 && vc->isSample) { //experimental sample render mark III
            int32_t nos;
            
            if (!vc->wavePtr) continue;
            if (vc->samplepos < 0) continue;
            fItp = THIS->itp->fItp;
            
            while (doneSmp < numSamples) {
                int32_t nosSpool, pos;
                
                nos = numSamples - doneSmp;
                //make sure we are not going outside the sample spool
                while ((nosSpool = (((nos - 1) * (vc->freqOffset))>>8) + THIS->itp->numTaps) >= SAMPSPOOLSIZE) nos--;
                
                //unroll sample into spool
                pos = vc->samplepos; //& 0xFFFFFF00;
                for (is=0; is < nosSpool;) {
                    int dif;
                    
                    if (vc->curdirecflg) { //backwards
                        dif = pos - vc->looppoint;
                        if (dif >= nosSpool<<8) dif = (nosSpool<<8);
                        
                        smpCpyRev(THIS->sampleSpool+is, vc->wavePtr+((pos-dif)>>8), dif>>8);
                        pos -= dif; is += dif>>8;
                        pos -= 0x100;
                        
                        if (pos < vc->looppoint) {
                            pos  += 0x100;
                            vc->curdirecflg = 0;
                        }
                    } else { // forwards
                        dif = vc->endpoint - pos;
                        if (dif >= nosSpool<<8) dif = (nosSpool<<8);
                        
                        smpCpyFrw(THIS->sampleSpool+is, vc->wavePtr+(pos>>8), dif>>8);
                        pos += dif; is += dif>>8;
                        pos += 0x100;
                        
                        if (pos >= vc->endpoint) {
                            if (vc->loopflg) { //has loop
                                if(vc->bidirecflg) { //bidi
                                    pos -= 0x100;
                                    vc->curdirecflg  = 1;
                                } else { //straight
                                    pos -= (vc->endpoint - vc->looppoint);
                                }
                            } else { //oneshot
                                while (is < nosSpool) THIS->sampleSpool[is++] = 0;
                            }
                        }
                    }
                }
                pos = vc->samplepos;
                
                //playback of sample
                for (is=0; is < nos; is++) {
                    tempBuf[doneSmp] = fItp(THIS->sampleSpool, vc->samplepos - pos, SAMPSPOOLSIZE<<8);
                    vc->samplepos += vc->freqOffset;
                    doneSmp++;
                }
                
                //fix samplepos AND direction after we are done
                if (vc->samplepos >= vc->endpoint) {
                    if (vc->loopflg) { //it loops
                        int32_t loopLen = vc->endpoint  - vc->looppoint;
                        int32_t relPos  = vc->samplepos - vc->looppoint;
                        
                        if (vc->bidirecflg && ((relPos / loopLen) & 1)) { //bidi and backwards
                            vc->samplepos = vc->endpoint  - (relPos % loopLen);
                            vc->curdirecflg = 1;
                        } else { //forwards
                            vc->samplepos = vc->looppoint + (relPos % loopLen);
                            vc->curdirecflg = 0;
                        }
                    } else { //oneshot
                        while (doneSmp < numSamples) tempBuf[doneSmp++] = 0;
                        vc->samplepos = -1;
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
            fItp = THIS->itp->fItp;
            
            //loop unroll optimization
            nos = numSamples;
            if (nos&1) {
                tempBuf[doneSmp] = fItp(vc->wavePtr, vc->synthPos, vc->waveLength);
                vc->synthPos += vc->freqOffset;
                vc->synthPos &= vc->waveLength;
                doneSmp++;
            }
            nos >>= 1;
            for(is=0; is < nos; is++) {
                tempBuf[doneSmp + (is << 1) + 0] = fItp(vc->wavePtr, vc->synthPos, vc->waveLength);
                vc->synthPos += vc->freqOffset;
                vc->synthPos &= vc->waveLength;
                tempBuf[doneSmp + (is << 1) + 1] = fItp(vc->wavePtr, vc->synthPos, vc->waveLength);
                vc->synthPos += vc->freqOffset;
                vc->synthPos &= vc->waveLength;
            }
            doneSmp += nos << 1;
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
