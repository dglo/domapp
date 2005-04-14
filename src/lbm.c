/* lbm.c: routines for getting in and out of lookback memory.

   Start with malloc version of LBM

   Jacobsen 8/10/04 jacobsen@npxdesigns.com
   $Id: lbm.c,v 1.3 2005-04-14 21:30:45 jacobsen Exp $

*/

#include <stdlib.h>
#include <string.h>
#include <lbm.h>

int lbm_is_initialized  = 0;
LBMQ * lbm              = NULL;

int lbm_ok(void) { return lbm_is_initialized; }

int lbm_init(void) {
  lbm = (LBMQ *) malloc(sizeof(LBMQ));
  if(lbm) {
    emptyLBMQ();
    lbm_is_initialized = 1;
    return 0;
  } 
  return 1; /* Failure */
}

void emptyLBMQ(void) { lbm->head = lbm->tail = 0; }
int nLBMQEntries(void) { return lbm->head - lbm->tail; }
int nLBMQSlots(void) { return MAX_LBM_RECS - nLBMQEntries(); }
int LBMQisFull(void) { return MAX_LBM_RECS == nLBMQEntries(); }
inline int getLBMQHead(void) { return lbm->head&LBMMASK; }
inline int getLBMQTail(void) { return lbm->tail&LBMMASK; }
int _getLBMQHead(void) { return lbm->head; }
int _getLBMQTail(void) { return lbm->tail; }
int LBMQisEmpty(void) { return lbm->head == lbm->tail; }
void LBMResetOverflow(void) { lbm->tail = lbm->head & ~LBMMASK; }
int LBMQisBlown(void) { return nLBMQEntries() > MAX_LBM_RECS; }

inline void putLBMQ(lbmRec *l) {
  memcpy(&(lbm->lbmrecs[getLBMQHead()]), l, sizeof(lbmRec));
  lbm->head++;
}

/* Check that LBMQisEmpty is false first */
inline void getLBMQ(lbmRec *l) {
  memcpy(l, &(lbm->lbmrecs[getLBMQTail()]), sizeof(lbmRec));
  lbm->tail++;
}

void maybeGetLBMQ(lbmRec *l) {
  /* Try getting this record; if destination has room, accept with acceptLBMRec */
  memcpy(l, &(lbm->lbmrecs[getLBMQTail()]), sizeof(lbmRec));
}

void acceptLBMRec(void) {
  lbm->tail++;
}

