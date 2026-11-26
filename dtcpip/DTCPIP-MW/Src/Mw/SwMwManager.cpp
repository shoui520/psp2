/*
 * The DTCP manager object (SwMwManager.cpp): its session lifecycle, the keybin and record
 * setup behind the manager dispatch's commands, and the checked status helper they share.
 * This file starts with the cancel path and the checked status; the session start/stop and
 * the record setup follow.
 */

#include <stddef.h>
#include <stdlib.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwManager.h"

#define SWMW_MANAGER_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwManager.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* the activation and download cancels (dtcpip_activate.c / dtcpip_download.c) */
int _sceDtcpIpCancelActivation(void);
int _sceDtcpIpCancelDownload(void);
}

/* the manager's error translation: -7..0 map to the manager statuses, anything else to
 * 12001 (0x81005776, the jump table at 0x8100578e) */
extern "C" int SwMwManagerMapStatus(int status)
{
	switch (status) {
	case 0:
		return 0;
	case -1:
		return 12015;
	case -2:
		return 12003;
	case -3:
		return 13017;
	case -4:
		return 12002;
	case -5:
		return 13003;
	case -6:
		return 13002;
	case -7:
		return 13019;
	default:
		return 12001;
	}
}

/* cancel the activation and the download in flight (0x8100458c) */
extern "C" int SwMwManagerCancel(void)
{
	int status;

	_sceDtcpIpCancelDownload();
	status = _sceDtcpIpCancelActivation();
	if (status != 0) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MANAGER_SOURCE, 2367);
		return SwMwManagerMapStatus(status);
	}
	return 0;
}

/* give a move sink a slot in the device's move table (0x810050fc) */
extern "C" int SwMwManagerRegisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink)
{
	int i;

	for (i = 0; i < 35; ++i) {
		if (slots->moveSinks[i] == 0) {
			slots->moveSinks[i] = sink;
			return 0;
		}
	}
	return 12008;
}

/* free a move sink's slot in the device's move table (0x8100514a) */
extern "C" int SwMwManagerUnregisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink)
{
	int i;

	for (i = 0; i < 35; ++i) {
		if (slots->moveSinks[i] == sink) {
			slots->moveSinks[i] = 0;
			return 0;
		}
	}
	return 12008;
}

/* start a session: the support singleton, the content and the manager (0x81004e40) */
extern "C" int SwMwManagerSessionStart(struct SwMwManagerSinkSlots *slots, int mode)
{
	int status;
	int failed;
	int i;

	if (slots->starting != 0)
		return 12014;
	slots->starting = mode;
	for (i = 0; i < 35; ++i) {
		slots->sinks[i] = 0;
		slots->moveSinks[i] = 0;
	}
	status = SwMwContentSupportCreate((struct SwMwContentSupport **)&slots->support);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 284);
		failed = 1;
	}
	if (failed != 0)
		goto fail;
	status = ((struct SwMwContentSupport *)slots->support)->createContent(&slots->content);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 291);
		failed = 1;
	}
	if (failed != 0)
		goto fail;
	status = ((struct SwMwContentSupport *)slots->support)->createManager(&slots->manager);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MANAGER_SOURCE, 298);
		failed = 1;
	}
	if (failed == 0)
		return 0;
	status = SwMwManagerMapStatus(status);
	((struct SwMwContentSupport *)slots->support)->release();
	slots->content = NULL;
	slots->starting = 0;
	return status;
fail:
	status = SwMwManagerMapStatus(status);
	slots->starting = 0;
	return status;
}
