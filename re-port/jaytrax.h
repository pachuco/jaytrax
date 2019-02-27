#ifndef JAYTRAX_H
#define JAYTRAX_H

//in case of changing any of the below
//please change jxs loader to account for changes
#define SE_ORDERS_SUBSONG (256)
#define SE_ROWS_PAT (64)
#define SE_EFF_INST (4)
#define SE_WAVES_INST (16)
#define SE_SAMPS_WAVE (256)
#define SE_ARPS_SONG (16)
#define SE_STEPS_ARP (16)
#define SE_NAMELEN (32)


#define SE_NROFCHANS (16)           // number of chans replayer can take
#define SE_NROFFINETUNESTEPS (16)	// number of finetune scales
#define SE_NROFEFFECTS (18)			// number of available wave effects

typedef struct Order Order;
struct Order {
	int16_t	    patnr;		// welk pattern spelen...
	int16_t	    patlen;		// 0/16/32/48
};

typedef struct Row Row;
struct Row {
	uint8_t	srcnote;
	uint8_t dstnote;
	uint8_t inst;
	int8_t  param;
	uint8_t script;
};

typedef struct Header Header;
struct Header {
	int16_t		mugiversion;//version of mugician this song was saved with
	int32_t		nrofpats;	//aantal patterns beschikbaar
	int32_t 	nrofsongs;	//aantal beschikbare subsongs
	int32_t 	nrofinst;	//aantal gebruikte instruments
};

typedef struct Subsong Subsong;
struct Subsong {
	uint8_t		mute[SE_NROFCHANS];   // which channels are muted? (1=muted)
	int32_t 	songspd;	// delay tussen de pattern-stepjes
	int32_t 	groove;		// groove value... 0=nothing, 1 = swing, 2=shuffle
	int32_t 	songpos;	// waar start song? (welke maat?)
	int32_t 	songstep;	// welke patternpos offset? (1/64 van maat)
	int32_t 	endpos;		// waar stopt song? (welke maat?)
	int32_t 	endstep;	// welke patternpos offset? (1/64 van maat)
	int32_t 	looppos;	// waar looped song? (welke maat?)
	int32_t 	loopstep;	// welke patternpos offset? (1/64 van maat)
	int16_t		songloop;	// if true, the song loops inbetween looppos and endpos
	char		name[SE_NAMELEN];   // name of subsong
	int16_t		nrofchans;	//nr of channels used
	uint16_t    delaytime; // the delaytime (for the echo effect)
	uint8_t     delayamount[SE_NROFCHANS]; // amount per channel for the echo-effect
	int16_t		amplification; //extra amplification factor (20 to 1000)
    Order       orders[SE_NROFCHANS][SE_ORDERS_SUBSONG];
};

typedef struct Effect Effect;
struct Effect {
	int32_t		dsteffect;
	int32_t		srceffect1;
	int32_t		srceffect2;
	int32_t		osceffect;
	int32_t		effectvar1;
	int32_t		effectvar2;
	int32_t		effectspd;
	int32_t		oscspd;
	int32_t		effecttype;
	int8_t		oscflg;
	int8_t		reseteffect;
};

// inst is the structure which has the entire instrument definition.
typedef struct Inst Inst;
struct Inst {
	int16_t		mugiversion;
	char		instname[SE_NAMELEN];
	int16_t		waveform;
	int16_t		wavelength;
	int16_t		mastervol;
	int16_t		amwave;
	int16_t		amspd;
	int16_t		amlooppoint;
	int16_t		finetune;
	int16_t		fmwave;
	int16_t		fmspd;
	int16_t		fmlooppoint;
	int16_t		fmdelay;
	int16_t		arpeggio;
	int8_t		resetwave[SE_WAVES_INST];
	int16_t		panwave;  
	int16_t		panspd;
	int16_t		panlooppoint;
	Effect		fx[SE_EFF_INST];
	char		samplename[SE_NAMELEN];
    //ugly. Move samples into their own spot
	int16_t		sharing;	// sample sharing! sharing contains instr nr of shared sanpledata (0=no sharing)
	int16_t		loopflg;    //does the sample loop or play one/shot? (0=1shot)
	int16_t		bidirecflg; // does the sample loop birdirectional? (0=no)
	int32_t		startpoint;
	int32_t		looppoint;
	int32_t		endpoint;
	uint8_t     hasSampData;     // pointer naar de sample (mag 0 zijn)
	int32_t		samplelength;	   // length of sample
	int16_t		waves[SE_WAVES_INST * SE_SAMPS_WAVE];
};

typedef struct Song Song;
struct Song {
    Header    header;
    Subsong** subsongs;
    Row*      patterns;
    char**    patNames;
    Inst**    instruments;
    uint8_t** samples;
    int8_t    arpTable[SE_ARPS_SONG * SE_STEPS_ARP];
    
};

//---------------------internal structs

// Chanfx is an internal structure which keeps track of the current effect parameters per active channel
typedef struct VoiceEffect VoiceEffect;
struct VoiceEffect {
	int			fxcnt1;
	int			fxcnt2;
	int			osccnt;
	double		a0;
	double		b1;
	double		b2;
	double		y1;
	double		y2;
	int			Vhp;
	int			Vbp;
	int			Vlp;
};

// chandat is an internal structure which keeps track of the current instruemnts current variables per active channel
typedef struct Voice Voice;
struct Voice {
	int32_t	    songpos;
	int32_t	    patpos;
	int32_t	    instrument;
	int32_t	    volcnt;
	int32_t	    pancnt;
	int32_t	    arpcnt;
	int32_t	    curnote;
	int32_t	    curfreq;
	int32_t	    curvol;
	int32_t	    curpan;
	int32_t	    bendadd;	// for the pitchbend
	int32_t	    destfreq;	// ...
	int32_t	    bendspd;	// ...
	int32_t	    bendtonote;
	int32_t	    freqcnt;
	int32_t	    freqdel;
	uint8_t*    sampledata;
	int32_t		looppoint;
	int32_t		endpoint;
	int16_t		loopflg;
	int16_t		bidirecflg;
	int16_t		curdirecflg;
	int32_t		synthPos;
	int32_t		samplepos;
	int32_t		lastplaypos;
	
	//immediate render vars
	uint8_t		isSample;
	int16_t*	wavePtr;
	int16_t		waveLength;
	int32_t		freqOffset;
	int16_t		gainMainL;
	int16_t		gainMainR;
	int16_t		gainEchoL;
	int16_t		gainEchoR;
	
	VoiceEffect fx[SE_WAVES_INST];
	int16_t		waves[SE_WAVES_INST * SE_SAMPS_WAVE + 1];
};

//TODO: make code re-entrant
typedef struct JayPlayer JayPlayer;
struct JayPlayer {
};

//---------------------API

int   jaytrax_loadSong(JayPlayer* _this, Song* sng);
void  jaytrax_playSubSong(JayPlayer* _this, int subsongnr);
void  jaytrax_stopSong(JayPlayer* _this);
void  jaytrax_pauseSong(JayPlayer* _this);
void  jaytrax_continueSong(JayPlayer* _this);
JayPlayer* jaytrax_init();
void  jaytrax_renderChunk(JayPlayer* _this, int16_t* renderbuf, int32_t nrofsamples, int32_t frequency);
#endif