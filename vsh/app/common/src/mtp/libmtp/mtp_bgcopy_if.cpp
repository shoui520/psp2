/*
 * The background copy service's calls: a copy's progress, its cancel, whether a copy may
 * start, and the copy itself, run while the service waits (SceLibMtp_BF311CC8).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>
#include "mtp_internal.h"
extern "C" {
#include <appmgr/appmgr.h>
}

/* a copy may not start: a host's operation runs, or the connection is in use */
#define SCE_MTP_ERROR_BGCOPY_BUSY		((int)0x80546610)
/* the launcher thread did not start */
#define SCE_MTP_ERROR_BGCOPY_LAUNCH		((int)0x80546600)
/* the service refused the copy */
#define SCE_MTP_ERROR_BGCOPY_REFUSED		((int)0x80546620)
/* the status the service reports once the copy is complete */
#define SCE_MTP_BGCOPY_STATUS_COMPLETE		((int)0x80546622)

/* how a progress over 100 percent, an unknown total and an unknown time are reported */
#define MTP_BGCOPY_PROGRESS_UNKNOWN	50
#define MTP_BGCOPY_PROGRESS_COMPLETE	100
#define MTP_BGCOPY_TOTAL_UNKNOWN	2
#define MTP_BGCOPY_REMAINING_UNKNOWN	20

/* the service is waited for 200 ms at a time, at most ten seconds once the copy ends */
#define MTP_BGCOPY_POLL_INTERVAL	200
#define MTP_BGCOPY_END_WAIT_COUNT	50

BgCopyMtpLauncher g_bgCopyMtpLauncher;

extern "C" {

/* the current copy's progress */
int SceLibMtp_E3EBABB4(SceUInt32 id, MtpBgCopyProgress *progress)
{
	MtpBgCopyTaskStatus status;

	mtpBgCopyGetTaskStatus(&status);
	progress->name = status.name;
	progress->progress = status.progress <= 100 ? status.progress :
		MTP_BGCOPY_PROGRESS_UNKNOWN;
	progress->number = status.index + 1;
	progress->total = status.total > 0 ? status.total : MTP_BGCOPY_TOTAL_UNKNOWN;
	progress->remaining = status.remaining >= 0 ? status.remaining :
		MTP_BGCOPY_REMAINING_UNKNOWN;
	progress->id = id;
	progress->reserved = 0;
	return 0;
}

int SceLibMtp_981E220E(SceUInt32 id)
{
	mtpBgCopyCancelTask(id);
	return 0;
}

/* a copy may start: no host's operation runs and the connection has been idle */
int SceLibMtp_FC6D4772(void)
{
	return !g_bgCopyMtpLauncher.isMtpTaskInProgress() && mtpBgCopyIsIdle() != 0;
}

/* runs the copy: the progress goes to the service until the copy completes or fails */
int SceLibMtp_BF311CC8(const MtpBgCopyRunParam *param)
{
	SceAppMgrAppStatus serviceStatus;
	bool requested;
	bool complete;
	int count;
	int ret;

	if (g_bgCopyMtpLauncher.isMtpTaskInProgress() || mtpBgCopyIsIdle() == 0)
		return SCE_MTP_ERROR_BGCOPY_BUSY;
	mtpBgCopySetCurrentTask(param->id);
	mtpBgCopySetTaskPath(&param->path);
	ret = g_bgCopyMtpLauncher.start();
	if (ret < 0)
		return SCE_MTP_ERROR_BGCOPY_LAUNCH;
	mtpBgCopySetRestart(1);
	requested = false;
	count = 0;
	complete = false;
	for (;;) {
		paf::thread::Sleep(MTP_BGCOPY_POLL_INTERVAL);
		if (!requested && param->host->isRequested() != 0) {
			requested = true;
			if (param->accept() == 0) {
				mtpCallbackInvokeEvent(1);
				ret = SCE_MTP_ERROR_BGCOPY_REFUSED;
				break;
			}
			mtpCallbackInvokeEvent(2);
		}

		{
			MtpBgCopyProgress progress;
			MtpBgCopyTaskStatus status;

			mtpBgCopyGetTaskStatus(&status);
			progress.name = status.name;
			progress.progress = status.progress <= 100 ? status.progress :
				MTP_BGCOPY_PROGRESS_UNKNOWN;
			progress.number = status.index + 1;
			progress.total = status.total > 0 ? status.total : MTP_BGCOPY_TOTAL_UNKNOWN;
			progress.remaining = status.remaining >= 0 ? status.remaining :
				MTP_BGCOPY_REMAINING_UNKNOWN;
			progress.reserved = 0;
			progress.id = param->id;
			if (status.result == SCE_MTP_BGCOPY_STATUS_COMPLETE) {
				progress.progress = MTP_BGCOPY_PROGRESS_COMPLETE;
				progress.number = progress.total;
				complete = true;
				ret = count;
			} else if (status.result != 0) {
				ret = status.result;
				break;
			}
			param->notify(param->arg, &progress);
		}
		if (complete)
			break;
	}
	do {
		paf::thread::Sleep(MTP_BGCOPY_POLL_INTERVAL);
		count++;
	} while (mtpBgCopyGetServiceStatus(&serviceStatus) == 0 &&
		 serviceStatus.processRunRequested != 0 && count < MTP_BGCOPY_END_WAIT_COUNT);
	if (ret == SCE_MTP_ERROR_BGCOPY_REFUSED)
		mtpBgCopySetRestart(0);
	else
		mtpBgCopySetCurrentTask(0);
	{
		paf::string empty("");

		mtpBgCopySetTaskPath(&empty);
	}
	mtpBgCopyClearTaskStatus();
	return ret;
}

}
