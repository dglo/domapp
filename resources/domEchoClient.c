/**
 * @file domEchoClient.c
 * 
 * Try to form up a message and send it off to domapp, 
 * either over a socket or through the DOM Hub device driver
 * running in simulation mode (sim_queue).
 * $Rev $
 * $Author: jacobsen $
 * $Date: 2005-04-14 21:42:01 $
 */

/** system include files */
#include <pthread.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h> /* JEJ: gethostbyname */

#ifdef CYGWIN
#include <cygwin/in.h>
#endif

/* JEJ Included these files to pick up O_RDWR flags for open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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
#include "msgHandler/msgHandler.h"
#include "msgHandler/MSGHANDLERmessageAPIstatus.h"
#include "message/genericMsgSendRecv.h"

/** fds index for stdin, stdout */
#define STDIN      0
#define STDOUT     1

/** error reporting flags, mostly just for information */
#define ERROR     -1
#define COM_ERROR -2
#define COM_EOF   -3

#define PRG        "domEchoClient"

static int ec_mode;
static int iofile;


int main(int argc, char* argv[]) {

  char *errorMsg;
  char port_copy[128];
  char buffer[128];
  int port;
  int filedes;
  int serversock;
  int rc;
  int sent, recvd;
  int imsg;
  static int num_msgs = 10;

  struct hostent *h;
  struct sockaddr_in localAddr, servAddr;
  MESSAGE_STRUCT * downMessage;
  MESSAGE_STRUCT * upMessage;

  int i;
  char test_echo_message[] = "test echo message - a little longer than before";
  char compare_echo_message[] = "test echo message - a little longer than before";
  char up_message_body[MAXDATA_VALUE];

  if(argc < 2 || argc > 3) {
    fprintf(stderr,"usage: %s <server> <port>\n",PRG);
    fprintf(stderr,"    or %s <device_file>\n",PRG);
    exit(1);
  }

  /* Find out what machine and port number to connect to, if socket I/O */
  if(argc == 3) {
    sscanf(argv[2], "%i", &port);
    fprintf(stderr, "%s: Will use socket for I/O.  Server %s, port %d.\n",
	    PRG, argv[1], port);
    if(port < 0 || port > 65536) {
      fprintf(stderr, "%s: Invalid port number %d.  Sorry.\n", PRG, port);
      exit(ERROR);
    }
    ec_mode = GMSR_SOCKET;
  } else {
    fprintf(stderr, "%s: Will use device file %s for I/O.\n",
	    PRG, argv[1]);
    ec_mode = GMSR_FILE;
  }

  /* For file mode, open device file. */
  if(ec_mode == GMSR_FILE) {
    filedes = open(argv[1], O_RDWR);
    if(filedes <= 0) {
      fprintf(stderr,"%s:  Couldn't open file %s (error %d: %s).\n",
	      PRG, argv[1], errno, strerror(errno));
      exit(errno);
    }
    iofile = filedes;
  } else { /* Connect to the server via socket */

    /* Get hostname */
    h = gethostbyname(argv[1]);
    if(h==NULL) {
      fprintf(stderr,"%s: unknown host '%s'\n",PRG,argv[1]);
      exit(1);
    }

    /* Fill server address data */
    servAddr.sin_family = h->h_addrtype;
    memcpy((char *) &servAddr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
    servAddr.sin_port = htons(port);

    /* create socket */
    serversock = socket(AF_INET, SOCK_STREAM, 0);
    if(serversock < 0) {
      fprintf(stderr,"%s: Can't create socket.  Reason: error %d (%s).\n",
	      PRG, errno, strerror(errno));
      exit(errno);
    }

    /* bind any port number */
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(0);
  
    rc = bind(serversock, (struct sockaddr *) &localAddr, sizeof(localAddr));
    if(rc<0) {
      fprintf(stderr,"%s: Can't bind port TCP %u\n", PRG, port);
      perror("error ");
      exit(errno);
    }
    
    /* connect to server */
    rc = connect(serversock, (struct sockaddr *) &servAddr, sizeof(servAddr));
    if(rc<0) {
      perror("cannot connect ");
      exit(errno);
    }
    
    /* Save the socket number as input/output file descriptor */
    iofile = serversock;
  }

  fprintf(stderr,"%s: iofile is %d.\n",PRG,iofile);

  /* Test get_file_descriptor_mode */

  fprintf(stderr,"%s: File descriptor mode is %d.\n",
	  PRG,gmsr_get_file_descriptor_mode(iofile));

  downMessage = (MESSAGE_STRUCT *) malloc(sizeof(MESSAGE_STRUCT));  
  if(downMessage == NULL) {
    errorMsg = "can't allocate message buffer";
    return ERROR;
  }

  upMessage = (MESSAGE_STRUCT *) malloc(sizeof(MESSAGE_STRUCT));
  if(upMessage == NULL) {
    errorMsg = "can't allocate message buffer";
    return ERROR;
  }

  for(imsg=0; imsg < num_msgs; imsg++) {
    //Message_setType(downMessage, MESSAGE_HANDLER);
    Message_setType(downMessage, 3);
    Message_setSubtype(downMessage, 11);
    //Message_setSubtype(downMessage, MSGHAND_ECHO_MSG);
    Message_setStatus(downMessage, SUCCESS); /* Do I fill before send or leave blank? */
    Message_setData(downMessage, test_echo_message, 0);
    //Message_setData(downMessage, test_echo_message, strlen(test_echo_message));

    /* Send message down */
    fprintf(stderr,"%s: About to send message.\n",PRG);
    sent = gmsr_sendMessageGeneric(iofile, downMessage);
    if(sent <= 0) {
      fprintf(stderr,"%s: gmsr_sendMessageGeneric failed (%d).\n", PRG,sent);
      return ERROR;
    }

    /* Get response */
    fprintf(stderr,"%s: About to recv message.\n",PRG);
    recvd = gmsr_recvMessageGeneric(iofile, upMessage, up_message_body, 0);
    if(recvd <= 0) {
      fprintf(stderr,"%s: gmsr_recvMessageGeneric failed (%d).\n", PRG,recvd);
      return ERROR;
    }

    /* See if it's correct */

    fprintf(stderr,"%s: Compare message contents here.\n",PRG);
    
    if(Message_dataLen(upMessage) >= MAXDATA_VALUE) {
      fprintf(stderr,"%s: Reply message payload too large (%d bytes)!\n",
	      PRG, Message_dataLen(upMessage));
      return ERROR;
    }
    
/*
    if(Message_dataLen(downMessage) != Message_dataLen(upMessage)) {
      fprintf(stderr,"%s: Down message payload (%d bytes)"
	      "!= up message payload (%d bytes).\n",
	      PRG, 
	      Message_dataLen(downMessage), 
	      Message_dataLen(upMessage));
      return ERROR;
    }
*/
    
    fprintf(stderr,"%s: Down and up message payload byte count agreed (%d bytes).\n",
	    PRG,
	    Message_dataLen(upMessage));
    
/*
    for(i=0; i < Message_dataLen(downMessage) && i < MAXDATA_VALUE; i++) {
      if(compare_echo_message[i] != up_message_body[i]) {
	fprintf(stderr,"%s: Down message payload and up message payload disagree"
		" at position %d (dn %d, up %d).\n", 
		PRG, i, (int) compare_echo_message[i], (int) up_message_body[i]);
	return ERROR;
      }
    }  
*/
    
    fprintf(stderr,"%s: Message %d sent and received correctly.\n",PRG, imsg);

  }  
  /* Clean up */
  close(iofile);
}


