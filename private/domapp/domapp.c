/**
 * @mainpage domapp socket based simulation program
 * @author McP, with mods by Jacobsen
 * Based on original code by mcp.
 *
 * $Date: 2003-04-16 00:16:23 $
 *
 * @section ration Rationale
 *
 * This code provides low level initialization and communications
 * code that allow execution of the DOM application within the 
 * either the Linux or Cygwin environment.  This particular verstion
 * uses stdin and stdout to perform all message passing communications.
 * It is intended to be run by the simboot program and, therefore, will
 * have these file descriptors mapped to a pre-established IP socket.
 *
 * @section details Implementation details
 *
 * This implementation uses the standard ipcmsg messaging package to
 * simulate the use of shared message queues between the various threads
 * of the DOM application.  This facility will be replaced by a similar
 * mechanism native to the DOM MB operating system (Nucleus).
 *
 * @subsection lang Language
 *
 * For DOM application compatibility, this code is written in C. 
 *
 */

/**
 * @file domappFile.c
 * 
 * This file contains low level initialization routines used to
 * settup and manage the environment used in simulating execution of
 * the DOM application on other platforms.
 *
 * $Revision: 1.17 $
 * $Author: jacobsen $
 * Based on original code by Chuck McParland
 * $Date: 2003-04-16 00:16:23 $
*/

/** system include files */
#include <pthread.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>   /* for time() */
#include <signal.h> /* For signal() */

/* JEJ Included these files to pick up O_RDWR flags for open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* JEJ */
#include <errno.h> /** For errno, on open */

/** project include files */
#include "msgHandler/msgHandlerTest.h"
#include "domapp_common/DOMtypes.h"
#include "domapp_common/packetFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
#include "message/genericMsgSendRecv.h"	
#include "msgHandler/msgHandler.h"
#include "msgHandler/MSGHANDLERmessageAPIstatus.h"
#include "expControl/expControl.h"
#include "dataAccess/dataAccess.h"
#include "slowControl/domSControl.h"
#include "hal/DOM_MB_hal_simul.h"

/** fds index for stdin, stdout */
#define STDIN 0
#define STDOUT 1

/** Where we get messages from : either STDIN or an open device file descriptor */
int dom_input_file;      
/** Where we send messages to : either STDOUT or an open device file descriptor */
int dom_output_file;

/** error reporting flags, mostly just for information */
#define ERROR -1
#define COM_ERROR -2
#define COM_EOF   -3

/** maximum values for some MESSAGE_STRUCT fields */
#define MAX_TYPE 255
#define MAX_SUBTYPE 255
#define MAX_STATUS 255

/** timeout values for select timer struct */
#define TIMEOUT_10MSEC 10000
#define TIMEOUT_100MSEC 100000

/** ipcmsg key values used in creating DOM application msg queues */
#define RD_QUEUE 0
#define SD_QUEUE 1
#define SC_QUEUE 2
#define EC_QUEUE 3
#define DA_QUEUE 4
#define TM_QUEUE 5

/** thread variable for msgHandler */
void *msgHandlerThread(void *arg);
/** thread variable for expControl */
void *expControlThread(void *arg);
/** thread variable for  dataAccess */
void *dataAccessThread(void *arg);
/** thread variable for  slowControl */
void *domSControl(void *arg);

/** routines to handle send and receive of messages through stdin/out */
int recvMsg(void);
int sendMsg(void);

/** message queue ids for DOM application thread use */
int RD;
int SD;
int SC;
int EC;
int DA;
int TM;

/** packet driver counters, etc. For compatibility purposes only */
ULONG PKTrecv;
ULONG PKTsent;
ULONG NoStorage;
ULONG FreeListCorrupt;
ULONG PKTbufOvr;
ULONG PKTbadFmt;
ULONG PKTspare;

ULONG MSGrecv;
ULONG MSGsent;
ULONG tooMuchData;
ULONG IDMismatch;
ULONG CRCproblem;

/* initialize a mutex for the msgHandler */
pthread_mutex_t msgHandlerMutex=PTHREAD_MUTEX_INITIALIZER;
/* initialize a mutex for manipulating DOM state */
pthread_mutex_t DOMstateMutex=PTHREAD_MUTEX_INITIALIZER;
/* initialize a mutex for use with slowControl */
pthread_mutex_t SlowControlMutex=PTHREAD_MUTEX_INITIALIZER;

/**
 * Main entry that starts the DOM application off.  Should
 * never return unless error encountered.
 *
 * @param integer ID to be used as DOM ID for this simulation.
 *	If not present default value of 1234 is used.
 *
 * @return ERROR or COM_ERROR, no non-error returns possible.
 *
 * @see domappFile.c for file based version of same program.
*/

static FILE *log;

#define pfprintf(...) /* makes it easier to comment out, 
			 when you want less verbose messages */

volatile sig_atomic_t fatal_error_in_progress = 0;
void cleanup_exit(int);

int main(int argc, char* argv[]) {

    /** storage */
    char *errorMsg;
#define DA_MAX_LOGNUM 6
    char id6dig[DA_MAX_LOGNUM+1];
    char logname[] = "dom_log_XXXXXX.txt"; /* Replace Xs w/ low bits of DOM ID */

    int i;
    int j;
    int k;
    int dataLen;
    int send;
    int receive;
    int status;
    int port;
    int nready;
    int dom_simulation_file; 
    pthread_t msgHandlerID;
    pthread_t expControlID;
    pthread_t dataAccessID;
    pthread_t domSControlID;
    fd_set fds;
    struct timeval timeout;
    MESSAGE_STRUCT *recvBuffer;
    MESSAGE_STRUCT *sendBuffer;


    setvbuf(stdout, (char *)0, _IONBF, 0);

    /* read args and configure */
    if (argc >= 3) {
      /* insert ID and name into hal simulation for later use.
       ID length is important, so make sure its 6 chars */

      if(strlen(argv[1])!=12) {
	halSetBoardID("123456123456");
      } else {
        halSetBoardID(argv[1]);
      }
      halSetBoardName(argv[2]);
    } else {
      /* must have id and name to run, make something up */
      halSetBoardID("000000000000");
      halSetBoardName("none");
    }

    strncpy(id6dig,halGetBoardID()+6, DA_MAX_LOGNUM);
    id6dig[DA_MAX_LOGNUM] = '\0';

    sprintf(logname,"dom_log_%s.txt", id6dig);
    
    log = fopen(logname, "w"); 
    //log = stderr; /* Do this if you want messages to stderr instead
    //		       of log file */

    fprintf(log, "domapp: opened log file.\n");

    /* standard hello msg, will remove eventually */
    fprintf(log,"domapp: argc=%d, argv[0]=%s\n\r", argc, argv[0]);

    /* show what dom we're running as */
    fprintf(log,"domapp: executing as DOM #%s, %s\n\r",
      halGetBoardID(), halGetBoardName());

    dom_input_file  = STDIN;
    dom_output_file = STDOUT;

    /* Install signal handler so that socket is closed when
       domapp is killed. JEJ */
    signal(SIGHUP, &cleanup_exit);
    signal(SIGINT, &cleanup_exit);
    signal(SIGTERM, &cleanup_exit);

    /* init messageBuffers */
    messageBuffers_init();

    /* create message queues for msgHandler */
    RD = Message_createQueue(RD_QUEUE); 
    if (RD < 0) {
	errorMsg="domapp: cannot create message queue RD";
	fprintf(log,"%s (error %d: %s)\n\r",errorMsg, errno, strerror(errno));
	return ERROR;
    }

    SD = Message_createQueue(SD_QUEUE);
    if (SD < 0) {
	errorMsg="domapp: cannot create message queue SD";
	fprintf(log,"%s (error %d: %s)\n\r",errorMsg, errno, strerror(errno));
	return ERROR;
    }

    SC = Message_createQueue(SC_QUEUE);
    if (SC < 0) {
	errorMsg="domapp: cannot create message queue SC";
	fprintf(log,"%s (error %d: %s)\n\r",errorMsg, errno, strerror(errno));
	return ERROR;
    }

    EC = Message_createQueue(EC_QUEUE);
    if (EC < 0) {
	errorMsg="domapp: cannot create message queue EC";
	fprintf(log,"%s (error %d: %s)\n\r",errorMsg, errno, strerror(errno));
	return ERROR;
    }

    DA = Message_createQueue(DA_QUEUE);
    if (DA < 0) {
	errorMsg="domapp: cannot create message queue DA";
	fprintf(log,"%s (error %d: %s)\n\r",errorMsg, errno, strerror(errno));
	return ERROR;
    }

    TM = Message_createQueue(TM_QUEUE);
    if (TM < 0) {
	errorMsg="domapp: cannot create message queue TM";
	fprintf(log,"%s (error %d: %s)\n\r",errorMsg, errno, strerror(errno));
	return ERROR;
    }

    fprintf(log, "domapp: Getting ready to go\n");

    /* start off the domapp threads */
    i = pthread_create(&msgHandlerID, NULL, msgHandler, 0);
    i = pthread_create(&expControlID, NULL, expControl, 0);
    i = pthread_create(&dataAccessID, NULL, dataAccess, 0);
    i = pthread_create(&domSControlID, NULL, dataAccess, 0);

    fprintf(log, "domapp: Ready to go\n");

    for (;;) {

      /* Flush log file out, each loop */
      fflush(log);
      //fclose(log);
      //log = fopen(logname, "a"); 

      /* always reset timeout value to larger value */
      timeout.tv_sec  = 0;
      timeout.tv_usec = 1000;

      /* set up the select so that we can do read timeouts. 
	 For some reason it seems you have to do this every time: JEJ */
      FD_ZERO(&fds);
      FD_SET(dom_input_file, &fds);

      //fprintf(log, "About to select...\n");
      nready = select(FD_SETSIZE, &fds, (fd_set *)0, (fd_set *)0, 
		      &timeout);
      //if(nready > 0) 
      //  fprintf(log, "domapp: Select returned %d.\n",nready);
      
      if (nready < 0) {
	fprintf(log, "domapp: com error on read\n");
	return COM_ERROR;
      }
      
      /* see if we have anything to read */
      if (nready > 0) {
	if (FD_ISSET(dom_input_file, &fds)) {
	  //fprintf(log, "domapp: performing read\n");
	  if ((status = recvMsg()) <= 0) {
	    /* error reported from receive, or EOF */
	    fprintf(log, "domapp: Error or EOF from recvMsg() (%d).\n",
		    status);
	    return COM_ERROR;
	  } else {
	    //fprintf(log, "domapp: Got a message.\n");
	  }
	}
      }
      
      /* see if msgHandler has something to send out */
      else {
	//fprintf(log,"domapp: Nothing to read.\n");
	if ((status = sendMsg()) < 0) {
	  fprintf(log,"domapp: problem on send: status %d.\n",status);
	  /* error reported from send */
	  /* try a reconnect */
	  break;
	}
      }
    }

    /* this is really bad, nothing left to do but bail */
    errorMsg="domapp: lost socket connection";
    fprintf(log,"%s\n\r",errorMsg);
    return 0;
}

void cleanup_exit(int sig) {
  /* Close input/output files/sockets.
     Since this handler is established for more than one kind of signal, 
     it might still get invoked recursively by delivery of some other kind
     of signal.  Use a static variable to keep track of that. */
  if(fatal_error_in_progress) 
    raise(sig);
  fatal_error_in_progress = 1;

  fprintf(log,"domapp: Caught signal--closing log and socket or device file.\n");
  fclose(log);
  close(dom_input_file);
  close(dom_output_file);

  /* Now reraise the signal.  We reactivate the signal's
     default handling, which is to terminate the process.
     We could just call exit or abort,
     but reraising the signal sets the return status
     from the process correctly. */
  signal(sig, SIG_DFL);
  raise(sig);
  //exit(0);
}

/**
 * receive a complete message, place it into a message buffer
 * and queue it up to the msgHandler for processing.
 *
 * @return 1 if message properly handled, other wise error from
 *	communications routine.
 *
 * @see sendMsg() complimentary function.
*/

int recvMsg() {
  /** length of entire message to be received */
  long msgLength;
  /** number of bytes to be received for a given msg part */
  int targetLength;
  /** recv sts */
  int sts;
  /** pointer to message header struct */
  MESSAGE_STRUCT *recvBuffer_p;
  /* pointer to message data buffer */
  UBYTE *dataBuffer_p;
  /* misc buffer pointer */
  UBYTE *buffer_p;
  
  /* receive the message header */
  recvBuffer_p = messageBuffers_allocate();
  if (recvBuffer_p == NULL) {
    return ERROR;
  }
  
  /* save data buffer address for  this message buffer */
  dataBuffer_p = Message_getData(recvBuffer_p);

  //fprintf(log, "domapp: about to gmsr_recvMessageGeneric.\n");

  /* Read in the whole message, now using generic functions */
  sts = gmsr_recvMessageGeneric(dom_input_file, recvBuffer_p, dataBuffer_p, 0);
  if(sts <= 0) {
    /* patch up the message buffer and release it */
    Message_setData(recvBuffer_p, dataBuffer_p, MAXDATA_VALUE);
    messageBuffers_release(recvBuffer_p);
    fprintf(log, "domapp: got sts %d in recv\n",sts);
    return sts == 0 ? COM_EOF : sts;
  } 
  
  /* send it off to the msgHandler */
  Message_send(recvBuffer_p, RD);
  
  return 1;
}

/**
 * Check msgHandler message queue for outgoing messages. If
 * one is present, assemble message and send it.
 *
 * @return 0  no errors, otherwise return value of low level 
 * 	communications routine.
 *
 * @see recvMsg()
*/


int sendMsg() {
  int sts;
  long sendLen;
  MESSAGE_STRUCT *sendBuffer_p;
  
  //fprintf(log, "domapp: About to Message_receive_nonblock().\n");

  if(Message_receive_nonblock(&sendBuffer_p, SD) == MEM_SUCCESS) {
    
    //fprintf(log, "domapp: have a message to send (sendBuffer_p == %p)\n",
    //	    sendBuffer_p);

    /* Send out the message using generic function */
    sts = gmsr_sendMessageGeneric(dom_output_file, sendBuffer_p);

    /* always delete the message buffer-even if there was a com error */
    messageBuffers_release(sendBuffer_p);
    
    //fprintf(log, "domapp: Send status was %d.\n",sts);
    
    if(sts < 0) {
      return sts;
    }
  }
  return 0;
}

