/* commonServices.h */

#ifndef _COMMONSERVICES_
#define _COMMONSERVICES_

/* Defines, etc. that are common for DOM Services
   like Experimental Control, Data Access, etc.
   If a data struct or definition is common to all
   services, it should be defined here so that
   other programs, like the message handler, can
   understand and access the appropriate structs. */

#define MAX_ERROR_STR_LEN 80
#define NULL_ERROR_STR ""

typedef struct {	
	UBYTE state;
	UBYTE lastErrorID;
	UBYTE lastErrorSeverity;
	UBYTE majorVersion;
	UBYTE minorVersion;
	UBYTE spare;
	char lastErrorStr[MAX_ERROR_STR_LEN];
	ULONG msgReceived;
	ULONG msgRefused;
	ULONG msgProcessingErr;
} COMMON_SERVICE_INFO;

void formatLong(ULONG value, UBYTE * buf);
void formatShort(USHORT value, UBYTE *buf);
void formatTime(unsigned long long time, UBYTE *buf);
ULONG unformatLong(UBYTE *buf);
USHORT unformatShort(UBYTE *buf);

/* Timing stuff added by JEJ Oct. 2004 */
typedef struct bench_struct {
  unsigned long t1, t2;
  unsigned long long sumdt;
  unsigned long ndt;
  unsigned long mindt;
  unsigned long maxdt;
  unsigned long lastdt;
  int started;
} bench_rec_t;

#define FPGA_CLOCK_SPEED 40000000
#define FPGA_CLOCKS_TO_NSEC 25

#define bench_start(b) do { b.t1 = FPGA(TEST_LOCAL_CLOCK_LOW); } while(0)
inline void bench_end(bench_rec_t *b);
void bench_init(bench_rec_t *b);
void bench_show(bench_rec_t *b, char * name);

#endif
