/* runMessageTest.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/PacketFormatInfo.h"
#include "domapp_common/MessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffersTest.h"
	

int main() {

    int i;

    i=messageTest();

    printf("runMessageTest: return status= %s\n",
	messageTest_status());
}
