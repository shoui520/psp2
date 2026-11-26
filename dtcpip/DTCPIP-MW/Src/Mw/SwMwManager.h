/*
 * The DTCP manager object (SwMwManager.cpp): the session lifecycle the manager dispatch
 * drives, the keybin and record setup, and the checked status helper.
 */

#pragma once

#include <scetypes.h>

extern "C" {
int SwMwManagerMapStatus(int status);
int SwMwManagerCancel(void);
}
