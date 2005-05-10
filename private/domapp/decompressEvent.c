/*
 * decompressEvent.c
 *
 * Decompress a compressed event back into engineering format.
 *
 * John Kelley
 * UW-Madison
 * September 2004
 *
 */

#include <stdio.h>
#include "hal/DOM_MB_types.h"
#include "engFormat.h"
#include "decompressEvent.h"

/*****************************************************************/
/*
 * getBit
 *
 * Get a single bit from the compressed input buffer.  Keep
 * track of which word and bit of the input we're on.
 */
USHORT getBit(const ULONG *buf_in, int *word_p, int *bit_p) {

    USHORT bitval = 0;

    /* Bits are read out LSB first */
    if ((buf_in[*word_p] & (0x1U << *bit_p)) != 0)
        bitval = 1;

    (*bit_p)++;
    if (*bit_p == 32) {
        *bit_p = 0;
        (*word_p)++;
    }

    return bitval;
}

/*****************************************************************/
/*
 * getVal
 *
 * Get a value from the compressed input buffer and return it
 * as a 10-bit short.  Keep track of which word and bit
 * in the input we're on.
 */
USHORT getVal(const ULONG *buf_in, int *word_p, int *bit_p) {

    USHORT val = 0;
    int i;

    /* FIX ME: deal with bad input, watch word pointer */

    /* Check for non-zero indicator bit */
    if (getBit(buf_in, word_p, bit_p)) {
        /* Read the next bits */
        for (i = 0; i < COMPRESS_NUMBITS; i++)
            val |= (getBit(buf_in, word_p, bit_p) << i);
    }

    /* Initialization to zero used if no non-zero bit */
    return val;
}

/*****************************************************************/
/* 
 * decompressWaveform
 *
 * Decompresses a buffer of ULONGs compressed with
 * K. Helbing's waveform compression back into a buffer of
 * N-bit values.  Monitors both the length of the input buffer, 
 * but since this is not fixed for an event, the number of
 * expected samples to decompress.  Returns non-zero if an error
 * occurred.
 *
 */
int decompressWaveform(const ULONG *buf_in, int max_len, int *len_in_p, 
                       UBYTE *buf_out, int *len_out_p, int cnt) {

    int err = 0;
    int bit = 0;
    int word = 0;
    int val_cnt = 0;
    int i;

    USHORT val;
    USHORT runlen;

    /* FIX ME -- use real error codes */
    if ((buf_in == NULL) || (buf_out == NULL) || (max_len <= 0))
        return 1; 

    while ((word < max_len) && (val_cnt < cnt)) {

        /* FIX ME: could still overrun max_len here */
        /* Get the value and run length */
        val = getVal(buf_in, &word, &bit);

        if (word < max_len) {
            runlen = getVal(buf_in, &word, &bit);
            
            /* Copy the values into the output buffer */
            /* Output values are shorts, output buffer is bytes */
            for (i = 0; i < runlen+1; i++) {
                *((USHORT *)(buf_out+(*len_out_p))) = val;
                *len_out_p += 2;
                val_cnt++;
            }
        }        
        else {
            /* FIX ME: deal with bad input */
        }
    }

    /* Check to make sure we got the right number of samples */
    /* The current word pointer should also match the expected input length */
    /* FIX ME -- use a real error code */
    if ((val_cnt != cnt) || (word != max_len-1))
        err = 1;

    /* Round up if some bits are used in final word */
    if (bit > 0)
        word++;

    *len_in_p += word;

    return err;
}

/*****************************************************************/
/* 
 * decompressHeader 
 *
 * Decompress a compressed event header back into an engineering
 * format header.  Since compressed event header is lossy, some
 * fields in engineering event remain zero-filled.
 *
 * Increment the length of the output buffer in len_out_p, and return the
 * number of samples expected in this event in cnt_p. 
 *
 * Returns non-zero if an error occurred.
 */
int decompressHeader(const ULONG *header_in, int max_len, int *len_in_p, 
                     UBYTE *header_out, int *len_out_p, int *cnt) {

    int err = 0;
    int ch;
    int chCount = 0;
    int fadcCount = 0;

    /* Check to make sure we have a header! */
    /* FIX ME: use real error codes */
    if (max_len < 2)
        return 1;

    /* Important -- zero the output header to start */
    memset(header_out, 0, getEngHeaderLength(header_out));

    /* Set event format field (constant) */
    setEngEventFormat(header_out);

    /* FADC information -- either 0 or 255 samples */
    fadcCount = (header_in[0] & 0x1000) ? 255 : 0;
    setEngFADCCount(header_out, fadcCount);
    
    /* ATWD information */
    /* Decompressed events are always 128 sample shorts */
    if (header_in[0] & 0x2000) {
        /* Get channels present */
        chCount = ((header_in[0] >> 14) & 0x3) + 1;

        for (ch = 0; ch < chCount; ch++) {
            setEngChannelAvailable(header_out, ch);
            setEngDataSize(header_out, ch, 2);
            setEngSampleCount(header_out, ch, 128);
        }
    }
    setEngATWDNumber(header_out, (header_in[0] >> 16) & 0x1);
    
    /* Trigger flag */
    setEngTriggerFlag(header_out, (UBYTE)(header_in[0] >> 19));

    /* Timestamp, low portion */
    setEngTimestampLo(header_out, header_in[1]);

    /* Length information */
    USHORT len = getEngHeaderLength(header_out);
    *cnt = 128*chCount + fadcCount;
    len += (*cnt) * 2;
    setEngEventLength(header_out, len);

    /* Increment output buffer length (in bytes) */
    *len_out_p += getEngHeaderLength(header_out);

    /* Amount of input buffer processed (in words) */
    *len_in_p += 2;

    return err;

}

/*****************************************************************/
/*
 * decompressEvent
 *
 * Decompress a software-compressed event back into 
 * engineering format.  Stores the length of the output buffer
 * used in bytes in len_out_p.  Returns nonzero if an error occurred.
 *
 */
int decompressEvent(const ULONG *buf_in, int max_len, int *len_in_p, 
                    UBYTE *buf_out, int *len_out_p) {

    int err = 0;
    *len_out_p = 0;
    int cnt = 0;

    /* Keep track of length of input buffer read */
    *len_in_p = 0;

    /* Decompress header */
    err = decompressHeader(buf_in, max_len, len_in_p, buf_out, len_out_p, &cnt);

    /* Decompress waveforms, as a block */
    if (!err)
        err = decompressWaveform(buf_in+(*len_in_p), max_len-(*len_in_p), len_in_p, 
                                 buf_out, len_out_p, cnt);
    return err;

}
