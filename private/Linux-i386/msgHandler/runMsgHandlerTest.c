/* runMsgHandlerTest.c */

#include "msgHandler/msgHandlerTest.h"
	

int main() {

    int i;

    i=msgHandlerTest();

    printf("runMsgHandlerTest: return status= %s\n",
	msgHandlerTest_status());
}
