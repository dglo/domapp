/* commonServices.c */


/* Common code for DOM Services */
/* like Experimental Control, Data Access, etc. */

#include "domapp_common/DOMtypes.h"
#include "message/message.h"
#include "domapp_common/commonServices.h"

/* make a big-endian long from a little-endian one */
void formatLong(ULONG value, UBYTE *buf) {
	*buf++=(UBYTE)((value>>24)&0xff);
	*buf++=(UBYTE)((value>>16)&0xff);
	*buf++=(UBYTE)((value>>8)&0xff);
	*buf++=(UBYTE)(value&0xff);
}

/* make a big-endian short from a little-endian one */
void formatShort(USHORT value, UBYTE *buf) {
	*buf++=(UBYTE)((value>>8)&0xff);
	*buf++=(UBYTE)(value&0xff);
}

/* make a little-endian long from a big-endian one */
ULONG unformatLong(UBYTE *buf) {
	ULONG temp;
	temp=(ULONG)(*buf++);
	temp=temp<<8;
	temp|=(ULONG)(*buf++);
	temp=temp<<8;
	temp|=(ULONG)(*buf++);
	temp=temp<<8;
	temp|=(ULONG)(*buf++);
	return temp;
}

/* make a little-endian short from a big-endian one */
USHORT unformatShort(UBYTE *buf) {
	USHORT temp;
	temp=(USHORT)(*buf++);
	temp=temp<<8;
	temp|=(USHORT)(*buf++);
	return temp;
}
