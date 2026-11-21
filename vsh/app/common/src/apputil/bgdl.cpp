/*
 * The background downloads queued for the application: its additional content and its license
 * (sceAppUtilBgdlGetStatus), and its own updates (SceAppUtil_B2FE3BB1).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <apputil.h>
#include <appmgr/appmgr.h>
#include "apputil_private.h"

/* the queue sceAppMgrBgdlGetQueueStatus copies out */
#define BGDL_QUEUE_ITEMS		32

int bgdlGetStatus(SceAppUtilBgdlStatus *stat)
{
	AppmgrBgdlQueueItem queue[BGDL_QUEUE_ITEMS];
	SceUInt32 ready, notReady, i;
	int ret;

	if (checkBgdlStatus(stat) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(queue, 0, sizeof(queue));
	ret = sceAppMgrBgdlGetQueueStatus(queue);
	if (ret != 0)
		return ret;
	ready = 0;
	stat->addcontNumReady = 0;
	notReady = 0;
	stat->addcontNumNotReady = 0;
	stat->licenseReady = 0;
	for (i = 0; i < BGDL_QUEUE_ITEMS; i++) {
		if (queue[i].contentId[0] == '\0')
			break;
		if ((stat->type == SCE_APPUTIL_BGDL_STATUS_TYPE_ADDCONT ||
		     stat->type == SCE_APPUTIL_BGDL_STATUS_TYPE_ADDCONT_AND_LICENSE) &&
		    queue[i].type == APPMGR_BGDL_TYPE_ADDCONT) {
			switch (queue[i].state) {
			case APPMGR_BGDL_STATE_READY:
				stat->addcontNumReady = ++ready;
				break;
			case APPMGR_BGDL_STATE_NOT_READY:
				stat->addcontNumNotReady = ++notReady;
				break;
			}
		}
		if (stat->type == SCE_APPUTIL_BGDL_STATUS_TYPE_ADDCONT_AND_LICENSE &&
		    queue[i].type == APPMGR_BGDL_TYPE_LICENSE && queue[i].state == APPMGR_BGDL_STATE_READY)
			stat->licenseReady = 1;
	}
	return 0;
}

int bgdlGetAppStatus(SceInt32 *notReady, SceInt32 *ready)
{
	AppmgrBgdlQueueItem queue[BGDL_QUEUE_ITEMS];
	SceInt32 numNotReady, numReady;
	SceUInt32 i;
	int ret;

	sceClibMemset(queue, 0, sizeof(queue));
	ret = sceAppMgrBgdlGetQueueStatus(queue);
	if (ret != 0)
		return ret;
	numNotReady = 0;
	numReady = 0;
	i = 0;
	do {
		if (queue[i].contentId[0] == '\0')
			break;
		if (queue[i].type == APPMGR_BGDL_TYPE_APP) {
			switch (queue[i].state) {
			case APPMGR_BGDL_STATE_READY:
				numReady++;
				break;
			case APPMGR_BGDL_STATE_NOT_READY:
				numNotReady++;
				break;
			}
		}
		i++;
	} while (i < BGDL_QUEUE_ITEMS);
	if (notReady != NULL)
		*notReady = numNotReady;
	if (ready != NULL)
		*ready = numReady;
	return 0;
}

int checkBgdlStatus(const SceAppUtilBgdlStatus *stat)
{
	SceUInt32 i;

	if (stat == NULL) {
		paramError(210);
		return 0;
	}
	if (stat->type != SCE_APPUTIL_BGDL_STATUS_TYPE_ADDCONT &&
	    stat->type != SCE_APPUTIL_BGDL_STATUS_TYPE_ADDCONT_AND_LICENSE) {
		paramError(212);
		return 0;
	}
	for (i = 0; i < sizeof(stat->reserved); i++) {
		if (stat->reserved[i] != 0) {
			paramError(211);
			return 0;
		}
	}
	return 1;
}
