/* runMessageCygwinTest.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/packetFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffersTest.h"
	

int main() {

    int i;

    i=messageCygwinTest();

    printf("runMessageCygwinTest: return status= %s\n",
	messageCygwinTest_status());
}
