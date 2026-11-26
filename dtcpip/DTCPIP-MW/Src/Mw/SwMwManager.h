/*
 * The DTCP manager object (SwMwManager.cpp): the session lifecycle the manager dispatch
 * drives, the keybin and record setup, and the checked status helper.
 */

#pragma once

#include <scetypes.h>

/* the device's two 35-entry session tables (the manager keeps its sinks and move sinks) */
struct SwMwManagerSinkSlots {
	int sinks[35];		/* 0x000: the sinks (0x81005060) */
	int reserved;		/* 0x08C */
	int moveSinks[35];	/* 0x090: the move sinks (0x810050FC/0x8100514A) */
};

extern "C" {
int SwMwManagerMapStatus(int status);
int SwMwManagerCancel(void);
int SwMwManagerRegisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink);
int SwMwManagerUnregisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink);
}
