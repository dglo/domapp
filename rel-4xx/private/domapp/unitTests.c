#include "unitTests.h"
#include "hal/DOM_MB_hal.h"
#include "moniDataAccess.h"
#include "message.h"
#include "dataAccess.h"

int unit_tests(void) {
  /* Add 'subsystem' unit tests here... */
  int failures = 0;
  if(!data_access_unit_tests()) {
    failures++;
  }

  if(failures) {
    mprintf("DOMApp unit tests failed");
  } else {
    mprintf("DOMApp unit tests passed");
  }
  return failures ? 0 : 1;
}
