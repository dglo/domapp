/* runMessageBuffersTest.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/PacketFormatInfo.h"
#include "domapp_common/MessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffersTest.h"
	

int main() {

    int i;

    i=messageBuffersTest();

    printf("runMessageBuffersTest: return status= %s\n",
	messageBuffersTest_status());
}
