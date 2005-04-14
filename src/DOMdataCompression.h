// DOMdataCompression.h //

#define COMPRESSED_BUF_MAX 1024 /* For SW data compression */

// DOM-related includes
#include "DOMtypes.h"

void initDOMdataCompression(int bufLen);

UBYTE *startDOMdataBuffer(UBYTE *bufferLoc);

UBYTE *addAndCompressEvent(UBYTE *incomingData, UBYTE *bufferLoc, int SW_compression);
