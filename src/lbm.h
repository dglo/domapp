/* lbm.h: routines for getting in and out of lookback memory.

   Start with malloc version of LBM

   Jacobsen 8/10/04 jacobsen@npxdesigns.com
   $Id: lbm.h,v 1.2 2005-04-14 18:12:12 jacobsen Exp $

*/
#ifndef __LBM_H__
#define __LBM_H__

#define LBM_REC_SIZ   2*1024
#define NLBMBITS      12
#define MAX_LBM_RECS (1<<NLBMBITS) /* 2**12 = 4096 */
#define LBMMASK      (MAX_LBM_RECS-1)
#define LBM_SIZE     (MAX_LBM_RECS * sizeof(struct lbm_rec))

typedef struct lbm_rec_s {
  unsigned char data[LBM_REC_SIZ];
} lbmRec;

typedef struct lbm_queue_struct {
  lbmRec lbmrecs[MAX_LBM_RECS];
  unsigned head, tail;
} LBMQ;

int lbm_ok(void);
int lbm_init(void);


void emptyLBMQ(void);
int nLBMQEntries(void);
int nLBMQSlots(void);
int LBMQisFull(void);
int getLBMQHead(void);
int getLBMQTail(void);
int _getLBMQHead(void);
int _getLBMQTail(void);
int LBMQisEmpty(void);
void LBMResetOverflow(void);
int LBMQisBlown(void);
void putLBMQ(lbmRec *l);
void getLBMQ(lbmRec *l);
void getLBMQ(lbmRec *l);

void maybeGetLBMQ(lbmRec *l);
void acceptLBMRec(void);

#endif
