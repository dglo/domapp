
#include <string.h>

// DOMdataCompression.c //

/* For clock: */
#include "hal/DOM_FPGA_regs.h"

// DOM-related includes
#include "DOMtypes.h"
#include "DOMdataCompression.h"
#include "compressEvent.h"
#include "moniDataAccess.h"

// local functions, data
int bufferLen;
int bufferLenRemaining;

void initDOMdataCompression(int bufLen) {
    bufferLen = bufLen;
}

UBYTE *startDOMdataBuffer(UBYTE *bufferLoc) {
    bufferLenRemaining = bufferLen;
    return bufferLoc;
}

UBYTE *addAndCompressEvent(UBYTE *incomingData, UBYTE *bufferLoc, int SW_compression) {
    // assume simulated, well formated event as input with byte
    // length in the first short word

  ULONG compressed[COMPRESSED_BUF_MAX];
  int incomingDataLen;

  if(SW_compression) {
    int length_out = compressEvent(incomingData, compressed);
    incomingDataLen = length_out*sizeof(ULONG);
    if(incomingDataLen > bufferLenRemaining) return NULL;
    memcpy(bufferLoc, compressed, incomingDataLen);
  } else {
    incomingDataLen = *incomingData*256 + *(incomingData+1);
    // verify that we have enough room left
    if(incomingDataLen > bufferLenRemaining) return NULL;    
    memcpy(bufferLoc, incomingData, incomingDataLen);
  }
  bufferLenRemaining -= incomingDataLen;
  return (UBYTE *)(bufferLoc + incomingDataLen);
}
