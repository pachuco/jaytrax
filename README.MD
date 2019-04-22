# JayTrax Library

This is a port from C++ to C of Jaytrax replayer by Rhino https://github.com/rhinoid

Current status: everything works except sample interpolation.


Documentation can be found here: <doc_link>


---

## Lib jaytax API

The following functions are exposed.

### Init / Pre-processing

```c

void jaytrax_loadSong(
JT1Player* THIS,
JT1Song* sng

);
```

### Track Player commands.

```c
void jaytrax_playSubSong(
JT1Player* THIS,
int subsongnr
);
```

```c
void jaytrax_stopSong(
JT1Player* THIS
);
```

```c
void jaytrax_pauseSong(
JT1Player* THIS
);
```

```c
//rename to resume?
void  jaytrax_continueSong(
JT1Player* THIS
); 
```

### Internal (for now)

```c
void  jaytrax_setInterpolation(JT1Player* THIS, uint8_t id);
```

```c
JT1Player* jaytrax_init();
```

```c
void  jaytrax_renderChunk(JT1Player* THIS, int16_t* renderbuf, int32_t nrofsamples, int32_t frequency);
```

---