/*
 * compress.c
 * 
 * Standalone test wrapper for compressEvent.
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "hal/DOM_MB_types.h"
#include "engFormat.h"
#include "compressEvent.h"

/* Pedestal waveforms */
unsigned short atwdpedavg[2][4][128];
unsigned short fadcpedavg[256];

/*****************************************************************/
/* 
 * getAvgPedestals
 *
 * Collect average pedestals from a hit record file.  
 */
int getAvgPedestals(void) {

    int atwd, ch, cnt;

    /* FIX ME */
    /* Just initialize to zero for now */
    for (atwd = 0; atwd < 2; atwd++)
        for (ch = 0; ch < 4; ch++)
            for (cnt = 0; cnt < 128; cnt++)
                atwdpedavg[atwd][ch][cnt] = 0;

    for (cnt = 0; cnt < 256; cnt++)
        fadcpedavg[ch] = 0;

}
/*****************************************************************/
/* 
 * Read in a binary hit record file and compress the events into
 * compressed format.
 *
 */
int main(int argc, char *argv[]) {

    USHORT val;
    const UBYTE *buf_in;
    ULONG  buf_out[1000];
    USHORT len_in = 0;
    USHORT len_out;
    int i;

    FILE *fout;
    int fin;
    struct stat st;

    /* Get the command line arguments */
    if (argc != 3) {
        printf("Usage: compress <input file> <output file>\n");
        return 1;
    }

    if ((fin = open(argv[1], O_RDONLY)) < 0) {
        printf("Error opening file %s for reading.\n", argv[1]);
        return 1;
    }
    
    if ((fout = fopen(argv[2], "wb")) == NULL) {
        printf("Error opening file %s for writing.\n", argv[2]);
        return 1;
    }

    if ((fstat(fin, &st))<0) {
        printf("Error in fstat\n");
        return 1;
    }
    
    /* Memory map the file */
    if ((buf_in=(const UBYTE *) mmap(0, st.st_size, 
                                     PROT_READ, MAP_PRIVATE, 
                                     fin, 0))==MAP_FAILED) {
        printf("Error calling mmap\n");
        return 1;
    }

    /* Get the pedestals from a hit record file */
    /* FIX ME -- this just fills with zeros right now */
    getAvgPedestals();

    /* Set the road grader thresholds */
    /* FIX ME -- parameterize or something */
    int atwd, ch;
    for (atwd = 0; atwd < 2; atwd++)
        for (ch = 0; ch < 4; ch++)
            setATWDRoadGradeThreshold(3, atwd, ch);
    setFADCRoadGradeThreshold(3);
    
    off_t idx = 0;
    USHORT eventLen;
    int cnt = 0;

    /* Process each event */
    while (idx<st.st_size) {

        if (st.st_size-idx < 32) {
            printf("Partial event at end of file\n");
            break;
        }
        else {
            /* Skip DOMId, etc. in hit record file */
            idx += 32;
            eventLen = getEngEventLength(buf_in+idx);        
            if (st.st_size-idx < eventLen) {
                printf("Partial event at end of file\n");
                break;
            }
        }
 
        len_out = compressEvent(buf_in+idx, buf_out);

        /* Write the output file */
        fwrite(buf_out, sizeof(ULONG), len_out, fout);

        idx += eventLen;
        cnt++;
    }

    printf("Compressed %d events!\n", cnt);

    close(fin);
    fclose(fout);

    return 0;
}
