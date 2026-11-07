/*
 * The check-in library's status (SceNearCheckinLibStatusJob): polls the near background service's
 * status until its job has finished.
 */
#include <stddef.h>
#include <scetypes.h>
#include <thread/thread.h>
#include "near_checkin_lib.h"

using namespace paf;

NearCheckinLibStatusJob::NearCheckinLibStatusJob()
	: NearCheckinLibJob("SceNearCheckinLibStatusJob")
{
}

NearCheckinLibStatusJob::~NearCheckinLibStatusJob()
{
}

int NearCheckinLibStatusJob::Run()
{
	bool canceled = false;
	int ret;

	for (;;) {
		bool finished;

		if (!canceled && IsCancelRequested()) {
			canceled = true;
			if (IsAbortRequested()) {
				ret = Abort();
				if (ret != NEAR_CHECKIN_LIB_JOB_CANCELED)
					return ret;
			}
		}
		finished = false;
		ret = UpdateStatus(&finished);
		if (ret != 0)
			return ret;
		if (finished)
			return 0;
		if (canceled && !IsAbortRequested())
			return NEAR_CHECKIN_LIB_JOB_CANCELED;
		thread::Sleep(NEAR_CHECKIN_LIB_POLL_INTERVAL);
	}
}
