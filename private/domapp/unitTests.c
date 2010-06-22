#include "unitTests.h"
#include "hal/DOM_MB_hal.h"
#include "moniDataAccess.h"
#include "message.h"
#include "dataAccess.h"

int unit_tests(void) {
  /* Add 'subsystem' unit tests here... */
  if(!data_access_unit_tests()) {
    return 0;
  }

  mprintf("DOMApp unit tests passed");
  return 1;
}
