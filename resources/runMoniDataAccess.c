/* Test of moniDataAccess.c */
#include <stdio.h>
#include "hal/DOM_MB_hal.h"
#include "dataAccess/moniDataAccess.h"


#define PROG "runMoniDataAccess: "

int main(int argc, char *argv[]) {
  int to_alloc, i, irec;
  UBYTE *basebuf;
  char  *msg = "This is a diagnostic message to be put in the circular buffer.\n";
  struct moniRec myMoniRec;
  MONI_STATUS ms;
  long long time;

  printf(PROG "moniGetElementLen is %d.\n", moniGetElementLen());

  to_alloc = MONI_CIRCBUF_RECS * MONI_REC_SIZE;

  printf(PROG "Will allocate buffer of %d bytes.\n", to_alloc);

  basebuf = (UBYTE *) malloc(to_alloc);
  
  if(!basebuf) {
    printf(PROG "Malloc failed.\n");
    exit(-1);
  }

  /* initialize monitoring */
  moniInit(basebuf, MONI_MASK);

  printf(PROG "Inserting 1023 monitoring messages...\n");

  /* try inserting a message */
  for(irec=0; irec < 1023; irec++) {
    //time=hal_FPGA_getClock();
    time = hal_FPGA_TEST_get_local_clock();
    moniInsertDiagnosticMessage(msg,&time,strlen(msg));
  }

  /* try to get it back */
  for(irec=0; irec < 1023; irec++) {
    ms = moniFetchRec(&myMoniRec);
    moniAcceptRec();

    if(ms != MONI_OK) {
      printf(PROG "Couldn't fetch record: status was %d.\n", (int) ms);
      exit(-1);
    }

    if(myMoniRec.dataLen != strlen(msg)) {
      printf(PROG "Record corrupted: length mismatch.\n");
      exit(-1);
    }

    if(strncmp(myMoniRec.data, msg, strlen(msg))) { 
      printf(PROG "Record corrupted: bytes recv'd follow.\n");
      for(i=0; i < myMoniRec.dataLen; i++) {
	printf(PROG "Byte %04d is %c %d 0x%x.\n",
	       i,
	       (char) myMoniRec.data[i],
	       (int) myMoniRec.data[i],
	       (int) myMoniRec.data[i]);
      }
    }
  }

  printf(PROG "Read back a good number of records (1023).\n");

  printf(PROG "Trying again... should return MONI_NODATA... ");

  ms = moniFetchRec(&myMoniRec);
  moniAcceptRec();

  if(ms != MONI_NODATA) {
    printf(PROG "\nNo joy.  Return value was %d.\n", (int) ms);
    exit(-1);
  } else {
    printf("looks good.\n");
  }

  printf(PROG "Overflowing buffer just for fun, then reading back.\n");
  /* blow buffer, just for fun */
  for(irec=0; irec < 1028; irec++) {
    //time=hal_FPGA_getClock();
    time = hal_FPGA_TEST_get_local_clock();
    moniInsertDiagnosticMessage(msg,&time,strlen(msg));
  }

  ms = moniFetchRec(&myMoniRec);
  moniAcceptRec();


  if(ms != MONI_OVERFLOW) {
    printf(PROG "Expected MONI_OVERFLOW, got %d.\n", (int) ms);
    exit(-1);
  } else {
    printf(PROG "Got MONI_OVERFLOW while trying to read, as expected.  Looks good.\n");
  }
  
  printf(PROG "Trying one more insert and two reads.\n");
  //time=hal_FPGA_getClock();
  time = hal_FPGA_TEST_get_local_clock();
  moniInsertDiagnosticMessage(msg,&time,strlen(msg));
  ms = moniFetchRec(&myMoniRec);
  moniAcceptRec();
  if(ms != MONI_OK) {
    printf(PROG "Expected MONI_OK, got %d.\n", (int) ms);
    exit(-1);
  } else {
    printf(PROG "Got MONI_OK...\n");
  }

  ms = moniFetchRec(&myMoniRec);
  moniAcceptRec();
  if(ms != MONI_NODATA) {
    printf(PROG "Expected MONI_NODATA, got %d.\n", (int) ms);
    exit(-1);
  } else {
    printf(PROG "Got MONI_NODATA.  Alles gute.\n");
  }

  return 0;
}
