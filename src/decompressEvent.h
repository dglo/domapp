/*
 * decompressEvent
 *
 * Header file for software decompression.
 */

/* Number of bits per value in the output buffer */
#define COMPRESS_NUMBITS     10

/* Prototypes */
int decompressEvent(const ULONG *buf_in, int max_len, int *len_in_p, 
                    UBYTE *buf_out, int *len_out_p);
