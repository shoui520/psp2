/*
 * The check-in library's disconnection (SceNearCheckinLibTermJob).
 */
#include <stddef.h>
#include <scetypes.h>
#include <thread/thread.h>
#include "near_checkin_lib.h"

using namespace paf;

NearCheckinLibTermJob::NearCheckinLibTermJob()
	: NearCheckinLibJob("SceNearCheckinLibTermJob")
{
}

NearCheckinLibTermJob::~NearCheckinLibTermJob()
{
}

int NearCheckinLibTermJob::Run()
{
	int ret;

	for (int time = 0; time < NEAR_CHECKIN_LIB_TERM_TIMEOUT;
	     time += NEAR_CHECKIN_LIB_POLL_INTERVAL) {
		ret = NearBGServiceDisconnect();
		if (ret == 0)
			break;
		thread::Sleep(NEAR_CHECKIN_LIB_POLL_INTERVAL);
	}
	return ret;
}
