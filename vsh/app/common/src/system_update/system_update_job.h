/*
 * system_update_job.h: the system update's jobs (system_update_job.cpp, Sony's file by 0.990's
 * records; "SceSystemUpdateJob" is the jobs' name). Every type name is ours (PLACEHOLDER) until
 * a record names them.
 */
#ifndef _SYSTEM_UPDATE_JOB_H
#define _SYSTEM_UPDATE_JOB_H

#include <stddef.h>
#include <scetypes.h>
#include <job/job.h>
#include <common/job_slots.h>

/* what the system update's calls give back (PLACEHOLDER names) */
struct SystemUpdateResult {
	int error;
	int reserved;
};

/* what a job tells when it is done (PLACEHOLDER names) */
class SystemUpdateListener {
public:
	virtual ~SystemUpdateListener()
	{
	}

	virtual int Bgdl_8112CFD8(SystemUpdateResult result) = 0;
};

/* a system update job (PLACEHOLDER name) */
class SystemUpdateJob : public paf::common::SlotJobItem {
public:
	SystemUpdateJob(SystemUpdateListener *listener);	/* (0x811329B0) */

	virtual ~SystemUpdateJob()
	{
	}

	virtual void Cancel();					/* (0x81132A24) */

	void SetResult(SystemUpdateResult result);		/* (0x81132A00) */
	SystemUpdateResult GetResult(void);			/* (0x81132A1A) */

	SystemUpdateListener *m_listener;
	bool m_cancelled;
	SystemUpdateResult m_result;
};

int Bgdl_81132A2C(int type, SceUInt32 *version);
bool Bgdl_81132AA2(int *status);

#endif
