/*
 * decompress.c
 *
 * Standalone wrapper for decompressEvent.
 *
 * John Kelley
 * UW-Madison
 * September 2004
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
#include "decompressEvent.h"


/*****************************************************************/
/* 
 * Read a binary file as a sequence of compressed events and decompress
 * them back into engineering format.
 */
int main(int argc, char *argv[]) {

    const ULONG *buf_in;
    UBYTE buf_out[2000];
    int len_in = 0;
    int len_out, i;
    int err;

    FILE *fout;
    int fin;
    struct stat st;

    /* Get the command line arguments */
    if (argc != 3) {
        printf("Usage: decompress <input file> <output file>\n");
        return 1;
    }

    if ((fin = open(argv[1], O_RDONLY)) < 0) {
        printf("Error opening file %s for reading.\n", argv[1]);
        return 1;
    }
    
    if ((fout = fopen(argv[2], "w")) == NULL) {
        printf("Error opening file %s for writing.\n", argv[2]);
        return 1;
    }

    if ((fstat(fin, &st))<0) {
        printf("Error in fstat\n");
        return 1;
    }

    /* Memory map the file */
    if ((buf_in=(const ULONG *) mmap(0, st.st_size,
                                    PROT_READ, MAP_PRIVATE, 
                                    fin, 0))==MAP_FAILED) {
        printf("Error calling mmap\n");
        return 1;
    }
    
    off_t idx = 0;
    USHORT eventLen;
    int cnt = 0;

    /* Process each event */
    while (idx< (st.st_size>>2)) {

        /* Get size of event */
        /* Make sure we have a header */
        if (st.st_size-idx*4 < 8) {
            printf("Partial event at end of file (size=%d idx=%d)\n", st.st_size, idx);
            break;
        }

        /* Decompress the event */
        /* FIX ME -- deal with error return */
        err = decompressEvent(buf_in+idx, (st.st_size-idx), &len_in, buf_out, &len_out);
	//printf("Event=%d size=%d idx=%d err=%d\n",cnt, st.st_size, idx, err);
        /* Write the output file */
        fwrite(buf_out, sizeof(UBYTE), len_out, fout);

        idx += len_in;
        cnt++;
    }

    printf("Decompressed %d events!\n", cnt);

    close(fin);
    fclose(fout);

    return 0;
}
