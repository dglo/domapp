/* commonServices.c */


/* Common code for DOM Services */
/* like Experimental Control, Data Access, etc. */

#include <string.h> /* For bzero */
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_FPGA_regs.h"
#include "DOMtypes.h"
#include "message.h"
#include "commonServices.h"
#include "moniDataAccess.h"


/* make a big-endian long from a little-endian one */
void formatLong(ULONG value, UBYTE *buf) {
	*buf++=(UBYTE)((value>>24)&0xff);
	*buf++=(UBYTE)((value>>16)&0xff);
	*buf++=(UBYTE)((value>>8)&0xff);
	*buf++=(UBYTE)(value&0xff);
}

void formatULL(unsigned long long val, UBYTE *buf) {
  *buf++ = (UBYTE)((val>>54) & 0xff);
  *buf++ = (UBYTE)((val>>48) & 0xff);
  *buf++ = (UBYTE)((val>>40) & 0xff);
  *buf++ = (UBYTE)((val>>32) & 0xff);
  *buf++ = (UBYTE)((val>>24) & 0xff);
  *buf++ = (UBYTE)((val>>16) & 0xff);
  *buf++ = (UBYTE)((val>>8) & 0xff);
  *buf++ = (UBYTE)(val & 0xff);
}

void formatTime(unsigned long long time, UBYTE *buf) {
  *buf++ = (UBYTE)((time>>40) & 0xFF);
  *buf++ = (UBYTE)((time>>32) & 0xFF);
  *buf++ = (UBYTE)((time>>24) & 0xFF);
  *buf++ = (UBYTE)((time>>16) & 0xFF);
  *buf++ = (UBYTE)((time>>8) & 0xFF);
  *buf++ = (UBYTE)(time & 0xFF);
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


inline void bench_end(bench_rec_t *b) {
  b->t2 = FPGA(TEST_LOCAL_CLOCK_LOW);
  if(b->t1 > b->t2) return; /* Bail on rollovers */
  b->lastdt = b->t2 - b->t1;

  if(b->lastdt < b->mindt || b->mindt == 0) {
    b->mindt = b->lastdt;
  }
  if(b->lastdt > b->maxdt || b->maxdt == 0) {
    b->maxdt = b->lastdt;
  }

  b->ndt++;
  b->sumdt += b->lastdt;
}

void bench_init(bench_rec_t *b) { bzero(b, sizeof(bench_rec_t)); }

void bench_show(bench_rec_t *b, char * name) {
  if(b->ndt>0)
    mprintf("%s: n=%lu min=%lu max=%lu last=%lu avg(ticks=%ld,nsec=%ld)", 
	    name, b->ndt, b->mindt, b->maxdt, 
	    b->lastdt, (long) (b->sumdt/b->ndt), (long) (b->sumdt/b->ndt)*FPGA_CLOCKS_TO_NSEC);
} 
