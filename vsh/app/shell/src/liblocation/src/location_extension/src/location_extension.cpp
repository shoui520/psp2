/*
 * SceLibLocationExtension's exports: the location from the stored Wi-Fi access points
 * (sceLocationGetLocationWifiHistory), a query of an earlier one by its timestamp
 * (sceLocationQueryLocationWifiHistory) and its cancellation, over the handles SceLibLocation
 * opened (SDK liblocation Reference). While the service waits on the user's permission, the
 * library follows the confirmation dialog in SceLibLocation's list, as sceLocationGetLocation
 * does there.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/modulemgr.h>
#include <modulemgr/module_info.h>
#include "../../../../../../lowlayer/src/tinylibc/tinylibc.h"
#include "location_extension.h"

/* what the service sets on the client's event flag 0 for a history request */
#define LOCATION_EVENTS_WIFI_HISTORY	(LOCATION_EVENT_LOCATION | \
					 LOCATION_EVENT_LOCATION_CANCELED | \
					 LOCATION_EVENTS_CONFIRM)
#define LOCATION_EVENTS_WIFI_HISTORY_QUERY \
					(LOCATION_EVENT_QUERY_LOCATION | \
					 LOCATION_EVENT_QUERY_CANCELED | \
					 LOCATION_EVENTS_CONFIRM)
#define LOCATION_EVENTS_CONFIRM		(LOCATION_EVENT_CONFIRM_ALLOWED | \
					 LOCATION_EVENT_CONFIRM_RUNNING | \
					 LOCATION_EVENT_CONFIRM_DENIED | \
					 LOCATION_EVENT_CONFIRM_CLOSED)

/* the list the last request ran on */
static IpcClientList *s_clientList = NULL;

extern const SceModuleThreadParam sce_module_start_thread_parameter = {
	4, 0, 0x1000, 0, 0
};
extern const SceModuleThreadParam sce_module_stop_thread_parameter = {
	4, 0, 0x1000, 0, 0
};

extern "C" SceInt32 sceLocationGetLocationWifiHistory(SceLocationHandle handle, SceUInt32 timeout,
						      SceLocationLocationInfo *linfo)
{
	IpcClientList *list;
	IpcClientInfo *info;
	SceRtcTick timestamp;

	if (linfo == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	list = SceLibLocation_EFC9BD25();
	if (list == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	setClientList(list);
	info = SceLibLocation_4C8B6F21(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	timestamp.tick = 0;
	return getLocationWifiHistory(list, info, false, timestamp, timeout, linfo);
}

extern "C" SceInt32 sceLocationQueryLocationWifiHistory(SceLocationHandle handle,
							SceRtcTick timestamp, SceUInt32 timeout,
							SceLocationLocationInfo *linfo)
{
	IpcClientList *list;
	IpcClientInfo *info;

	if (linfo == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	list = SceLibLocation_EFC9BD25();
	if (list == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	setClientList(list);
	info = SceLibLocation_4C8B6F21(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return getLocationWifiHistory(list, info, true, timestamp, timeout, linfo);
}

extern "C" SceInt32 sceLocationCancelQueryLocationWifiHistory(SceLocationHandle handle)
{
	IpcClientList *list;
	IpcClientInfo *info;

	list = SceLibLocation_EFC9BD25();
	if (list == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	info = SceLibLocation_4C8B6F21(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return cancelQueryLocationWifiHistory(list, info);
}

__attribute__((noinline)) void setClientList(IpcClientList *list)
{
	s_clientList = list;
}

/*
 * The request, then, when the service needs the user's permission first (or its history file
 * is not ready), the events until the location comes: the dialog's progress is written to the
 * list. The word at 0x28 is cleared where SceLibLocation clears its pending flag.
 */
int getLocationWifiHistory(IpcClientList *list, IpcClientInfo *info, bool query,
			   SceRtcTick timestamp, SceUInt32 timeout, SceLocationLocationInfo *linfo)
{
	SceUInt32 pattern;
	SceUInt32 events;
	int ret;

	ret = locationServiceGetLocationWifiHistory(info->client, info->handle, query, false,
						    timestamp, timeout, linfo);
	if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID ||
	    ((SceLibLocation_0341758B() & LOCATION_MODE_CONFIRM_MASK) ==
		     SCE_LOCATION_MODE_CONFIRM_AUTO &&
	     ret == SCE_LOCATION_INFO_DENIED_BY_USER)) {
		events = query ? LOCATION_EVENTS_WIFI_HISTORY_QUERY : LOCATION_EVENTS_WIFI_HISTORY;
		for (;;) {
			if (info->client->waitEventFlag(0, events,
							SCE_KERNEL_EVF_WAITMODE_OR |
							SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
							&pattern, NULL) != 0)
				continue;

			if ((pattern & (LOCATION_EVENT_LOCATION |
					LOCATION_EVENT_QUERY_LOCATION)) != 0) {
				ret = locationServiceGetLocationWifiHistoryResult(info->client,
										  info->handle,
										  query, false,
										  linfo);
				break;
			}
			if ((pattern & (LOCATION_EVENT_LOCATION_CANCELED |
					LOCATION_EVENT_QUERY_CANCELED)) != 0) {
				if ((SceLibLocation_0341758B() & LOCATION_MODE_CONFIRM_MASK) ==
					    SCE_LOCATION_MODE_CONFIRM_AUTO &&
				    list->confirmStatus == SCE_LOCATION_DIALOG_STATUS_RUNNING) {
					list->confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
					list->confirmResult = SCE_LOCATION_DIALOG_STATUS_ENABLE;
				}
				ret = SCE_LOCATION_INFO_GET_LOCATION_CANCELED;
				break;
			}
			if (pattern == LOCATION_EVENT_CONFIRM_ALLOWED) {
				list->confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
				list->confirmResult = SCE_LOCATION_DIALOG_STATUS_ENABLE;
				list->confirmArg = 0;
				SceLibLocation_41C1E68F(0);
			} else if (pattern == LOCATION_EVENT_CONFIRM_RUNNING) {
				list->confirmStatus = SCE_LOCATION_DIALOG_STATUS_RUNNING;
				list->confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
				list->confirmArg = 0;
				SceLibLocation_41C1E68F(1);
			} else if (pattern == LOCATION_EVENT_CONFIRM_DENIED) {
				ret = SCE_LOCATION_INFO_DENIED_BY_USER;
				list->confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
				list->confirmResult = SCE_LOCATION_DIALOG_RESULT_DISABLE;
				list->confirmArg = 0;
				SceLibLocation_41C1E68F(0);
				break;
			} else if (pattern == LOCATION_EVENT_CONFIRM_CLOSED) {
				ret = SCE_LOCATION_INFO_DENIED_BY_USER;
				list->confirmArg = 0;
				SceLibLocation_41C1E68F(0);
				break;
			} else {
				ret = SCE_LOCATION_ERROR_FATAL_ERROR;
				break;
			}
		}
	}
	return ret;
}

__attribute__((noinline)) int cancelQueryLocationWifiHistory(IpcClientList *list,
							  IpcClientInfo *info)
{
	int ret;

	ret = locationServiceCancelQueryLocationWifiHistory(info->client, info->handle);
	if (ret == 0 && list->confirmArg != 0) {
		list->confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
		list->confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
		list->confirmArg = 0;
		SceLibLocation_41C1E68F(0);
	}
	return ret;
}

/* SceLibLocation answers sceLocationInit's V3 mode only once the extension is loaded */
extern "C" int module_start(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	tinyLibcInit();
	SceLibLocation_5C7185D2();
	return SCE_KERNEL_START_SUCCESS;
}
