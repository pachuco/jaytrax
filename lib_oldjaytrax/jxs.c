#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jaytrax.h"
#include "jxs.h"
#include "ioutil.h"

//---------------------JXS3457

static int struct_readHeader(Header* dest, size_t len, FILE* fin) {
    uint32_t i;
    J3457Header t;
    
    for (i=0; i < len; i++) {
        fread(&t, sizeof(J3457Header), 1, fin);
        dest[i].mugiversion = t.mugiversion;
        dest[i].nrofpats    = t.nrofpats;
        dest[i].nrofsongs   = t.nrofsongs;
        dest[i].nrofinst    = t.nrofinst;
    }
    return ferror(fin);
}

static int struct_readSubsong(Subsong* dest, size_t len, FILE* fin) {
    uint32_t i, j, k;
    J3457Subsong t;
    
    for (i=0; i < len; i++) {
        fread(&t, sizeof(J3457Subsong), 1, fin);
        for (j=0; j < J3457_CHANS_SUBSONG; j++) dest[i].mute[j] = t.mute[j];
        dest[i].songspd         = t.songspd;
        dest[i].groove          = t.groove;
        dest[i].songpos         = t.songpos;
        dest[i].songstep        = t.songstep;
        dest[i].endpos          = t.endpos;
        dest[i].endstep         = t.endstep;
        dest[i].looppos         = t.looppos;
        dest[i].loopstep        = t.loopstep;
        dest[i].songloop        = t.songloop;
        memcpy(&dest[i].name, &t.name, 32);
        dest[i].nrofchans       = t.nrofchans;
        dest[i].delaytime       = t.delaytime;
        for (j=0; j < J3457_CHANS_SUBSONG; j++) {
            dest[i].delayamount[j] = t.delayamount[j];
        }
        dest[i].amplification   = t.amplification;
        for (j=0; j < J3457_CHANS_SUBSONG; j++) {
            for (k=0; k < J3457_ORDERS_SUBSONG; k++) {
                dest[i].orders[j][k].patnr  = t.orders[j][k].patnr;
                dest[i].orders[j][k].patlen = t.orders[j][k].patlen;
            }
        }
    }
    return ferror(fin);
}

static int struct_readPat(Row* dest, size_t len, FILE* fin) {
    uint32_t i, j;
    J3457Row t[J3457_ROWS_PAT];
    
    for (i=0; i < len; i++) {
        fread(&t, sizeof(J3457Row)*J3457_ROWS_PAT, 1, fin);
        for (j=0; j < J3457_ROWS_PAT; j++) {
            uint32_t off = i*J3457_ROWS_PAT + j;
            dest[off].srcnote = t[j].srcnote;
            dest[off].dstnote = t[j].dstnote;
            dest[off].inst    = t[j].inst;
            dest[off].param   = t[j].param;
            dest[off].script  = t[j].script;
        }
    }
    return ferror(fin);
}

static int struct_readInst(Inst* dest, size_t len, FILE* fin) {
    uint32_t i, j;
    J3457Inst t;
    for (i=0; i < len; i++) {
        fread(&t, sizeof(J3457Inst), 1, fin);
        dest[i].mugiversion     = t.mugiversion;
        memcpy(&dest[i].instname, &t.instname, 32);
        dest[i].waveform        = t.waveform;
        dest[i].wavelength      = t.wavelength;
        dest[i].mastervol       = t.mastervol;
        dest[i].amwave          = t.amwave;
        dest[i].amspd           = t.amspd;
        dest[i].amlooppoint     = t.amlooppoint;
        dest[i].finetune        = t.finetune;
        dest[i].fmwave          = t.fmwave;
        dest[i].fmspd           = t.fmspd;
        dest[i].fmlooppoint     = t.fmlooppoint;
        dest[i].fmdelay         = t.fmdelay;
        dest[i].arpeggio        = t.arpeggio;
        for (j=0; j < J3457_WAVES_INST; j++) {
            dest[i].resetwave[j] = t.resetwave[j];
        }
        dest[i].panwave         = t.panwave;
        dest[i].panspd          = t.panspd;
        dest[i].panlooppoint    = t.panlooppoint;
        for (j=0; j < J3457_EFF_INST; j++) {
            dest[i].fx[j].dsteffect     = t.fx[j].dsteffect;
            dest[i].fx[j].srceffect1    = t.fx[j].srceffect1;
            dest[i].fx[j].srceffect2    = t.fx[j].srceffect2;
            dest[i].fx[j].osceffect     = t.fx[j].osceffect;
            dest[i].fx[j].effectvar1    = t.fx[j].effectvar1;
            dest[i].fx[j].effectvar2    = t.fx[j].effectvar2;
            dest[i].fx[j].effectspd     = t.fx[j].effectspd;
            dest[i].fx[j].oscspd        = t.fx[j].oscspd;
            dest[i].fx[j].effecttype    = t.fx[j].effecttype;
            dest[i].fx[j].oscflg        = t.fx[j].oscflg;
            dest[i].fx[j].reseteffect   = t.fx[j].reseteffect;
        }
        memcpy(&dest[i].samplename, &t.samplename, 192);
        //exFnameFromPath(&dest[i].samplename, &t.samplename, SE_NAMELEN);
        dest[i].sharing       = t.sharing;
        dest[i].loopflg       = t.loopflg;
        dest[i].bidirecflg    = t.bidirecflg;
        dest[i].startpoint    = t.startpoint;
        dest[i].looppoint     = t.looppoint;
        dest[i].endpoint      = t.endpoint;
		dest[i].hasSampData   = t.hasSampData ? 1 : 0; //this was a sampdata pointer in original jaytrax
        dest[i].samplelength  = t.samplelength;
		//memcpy(&dest[i].waves, &t.waves, J3457_WAVES_INST * J3457_SAMPS_WAVE * sizeof(int16_t));
        fread(&dest->waves, 2, J3457_WAVES_INST * J3457_SAMPS_WAVE, fin);
    }
    return ferror(fin);
}

//---------------------JXS3458

/* Soon! */

//---------------------

int jxsfile_loadSong(char* path, Song** sngOut) {
    #define FAIL(x) {error=(x); goto _ERR;}
    char buf[BUFSIZ];
    FILE *fin;
    Song* song;
    int i;
    int error;
    
    if (!(fin = fopen(path, "rb"))) FAIL(ERR_FILEIO);
    setbuf(fin, buf);
	
    //song
    if((song = (Song*)calloc(1, sizeof(Song)))) {
        int version;
        
        if (struct_readHeader(&song->header, 1, fin)) FAIL(ERR_BADSONG);
        //version magic
        version = song->header.mugiversion;
        if (version >= 3456 && version <= 3457) {
            int nrSubsongs = song->header.nrofsongs;
            int nrPats     = song->header.nrofpats;
            int nrRows     = J3457_ROWS_PAT * nrPats;
            int nrInst     = song->header.nrofinst;
            
            //subsongs
            if ((song->subsongs = (Subsong**)calloc(nrSubsongs, sizeof(Subsong*)))) {
                for (i=0; i < nrSubsongs; i++) {
                    if ((song->subsongs[i] = (Subsong*)calloc(1, sizeof(Subsong)))) {
                        if (struct_readSubsong(song->subsongs[i], 1, fin)) FAIL(ERR_BADSONG);
                    } else FAIL(ERR_MALLOC);
                }
            } else FAIL(ERR_MALLOC);
            
            //patterns
            if ((song->patterns = (Row*)calloc(nrRows, sizeof(Row)))) {
                if (struct_readPat(song->patterns, nrPats, fin)) FAIL(ERR_BADSONG);
            } else FAIL(ERR_MALLOC);
            
            //pattern names. Length includes \0
            if ((song->patNames = (char**)calloc(nrPats, sizeof(char*)))) {
                for (i=0; i < nrPats; i++) {
                    int32_t nameLen = 0;
                    
                    fread(&nameLen, 4, 1, fin);
                    if ((song->patNames[i] = (char*)calloc(nameLen, sizeof(char)))) {
                        fread(song->patNames[i], nameLen, 1, fin);
                    } else FAIL(ERR_MALLOC);
                }
                
                if (ferror(fin)) FAIL(ERR_BADSONG);
            } else FAIL(ERR_MALLOC);
			
            //instruments
            if ((song->instruments = (Inst**)calloc(nrInst, sizeof(Inst*)))) {
                if (!(song->samples = (uint8_t**)calloc(nrInst, sizeof(uint8_t*)))) FAIL(ERR_MALLOC);
                for (i=0; i < nrInst; i++) {
                    if ((song->instruments[i] = (Inst*)calloc(1, sizeof(Inst)))) {
                        Inst* inst = song->instruments[i];
                        if (struct_readInst(inst, 1, fin)) FAIL(ERR_BADSONG);
                        
                        //patch old instrument to new
						if (version == 3456) {
                            inst->sharing = 0;
                            inst->loopflg = 0;
                            inst->bidirecflg = 0;
                            inst->startpoint = 0;
                            inst->looppoint = 0;
                            inst->endpoint = 0;
                            //silly place to put a pointer
							if (inst->hasSampData) {
								inst->startpoint=0;
								inst->endpoint=(inst->samplelength/2);
								inst->looppoint=0;
							}
						}
                        
                        //sample data
                        if (inst->hasSampData) {
							//inst->samplelength is in bytes, not samples
                            if(!(song->samples[i] = (uint8_t*)calloc(inst->samplelength, sizeof(uint8_t)))) FAIL(ERR_MALLOC);
                            fread(song->samples[i], 1, inst->samplelength, fin);
                            if (ferror(fin)) FAIL(ERR_BADSONG);
                        } else {
                            song->samples[i] = NULL;
                        }
                    } else FAIL(ERR_MALLOC);
                }
            } else FAIL(ERR_MALLOC);
            
            //arpeggio table
            fread(&song->arpTable, J3457_STEPS_ARP, J3457_ARPS_SONG, fin);
        } else if (version == 3458) {
            //Soon enough!
            FAIL(ERR_BADSONG);
        } else FAIL(ERR_BADSONG);
    } else FAIL(ERR_MALLOC);
    
    if (fin) fclose(fin);
    *sngOut = song;
    return ERR_OK;
    #undef FAIL
    _ERR:
        if (fin) fclose(fin);
        //freesong(song);
        *sngOut = NULL;
        return error;
}