/*
 * patch_chk_cache_enqueue.cpp: game update patch-check cache write job enqueuer.
 * 3.65-CEX: 0x813C9916 - 0x813C9A4F
 */
#include <stddef.h>
#include <scetypes.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <ipmi/session.h>

#include "../livespace_db/lsdb_keys.h"

struct PatchCheckCacheData {
	SceUInt64 updDate;
	int resStatus;
	int signOff;
};

struct GameUpdateCacheEntry {
	SceUInt64 networkTick;
	SceInt32 status;
	SceUInt32 flags;
};

extern paf::job::JobItem *patchCheckSetJobCtor(void *mem, IPMI::Session *session,
					       const char *titleId,
					       const PatchCheckCacheData *cacheData,
					       int updateMask);

static const int kPatchCheckNoMemory = -2146430974; /* 0x80101002 */

/* 0x813C9916 */
int shellGameUpdateEnqueueCacheWrite(const char *titleId, const GameUpdateCacheEntry *entry,
				     int updateMask)
{
	PatchCheckCacheData cacheData;
	cacheData.updDate = entry->networkTick;
	cacheData.resStatus = entry->status;
	cacheData.signOff = (int)entry->flags;

	paf::job::JobItem *jobItem = NULL;
	void *jobMemory = operator new(72);
	if (jobMemory != NULL)
		jobItem = patchCheckSetJobCtor(jobMemory, NULL, titleId, &cacheData, updateMask);

	paf::common::SharedPtr<paf::job::JobItem> jobHandle(jobItem);
	if (jobHandle.get() == NULL)
		return kPatchCheckNoMemory;

	paf::job::JobQueue::default_queue->Enqueue(jobHandle);
	return 0;
}
