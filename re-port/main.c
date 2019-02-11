#include <stdio.h>
#include "jxs.h"
#include "syntrax.h"

int main() {
    Song song;
    int retcode;
    retcode = jxsfile_loadSong("090301.jxs", &song);
    printf("retcode: %d\n", retcode);
    return retcode;
}