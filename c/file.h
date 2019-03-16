#ifndef FILE_H
#define FILE_H

#include "syntrax.h"

Song* File_loadSong(const char *path);
Song* File_loadSongMem(const uint8_t *buffer, size_t size);

void File_freeSong(Song *synSong);

#endif
