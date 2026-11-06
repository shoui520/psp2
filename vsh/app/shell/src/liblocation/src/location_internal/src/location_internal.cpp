/*
 * SceLibLocationInternal's exports: sceLocationInit in the system mode, the Wi-Fi history
 * requests the system's client makes with its permission (the service is asked with the option
 * set), the allocator, and the data the confirmation dialog shows.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/modulemgr.h>
#include "location_internal.h"

/* the system's sceLocationInit: SceLibLocation runs on the list it makes */
extern "C" SceInt32 SceLibLocationInternal_3500A98C(SceUInt32 open, SceUInt32 mode)
{
	int ret;

	if (mode != LOCATION_MODE_SYSTEM)
		return SCE_LOCATION_ERROR_INVALID_VALUE;
	ret = IpcClientList::init(open, mode);
	if (ret == 0) {
		SceLibLocation_7F74AA94(IpcClientList::getInstance());
		if (IpcClientList::getUseHeap() == 1)
			ret = SceLibLocation_822BBF06(IpcClientList::getAllocator(),
						      IpcClientList::getUseHeap());
	}
	return ret;
}

extern "C" SceInt32 SceLibLocationInternal_A8531ABF(SceLocationHandle handle, SceUInt32 timeout,
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
	IpcClientList::setInstance(list);
	info = IpcClientList::find(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	timestamp.tick = 0;
	return IpcClientList::getInstance()->getLocationWifiHistory(info, false, timestamp, timeout,
								    linfo);
}

extern "C" SceInt32 SceLibLocationInternal_29267421(SceLocationHandle handle, SceRtcTick timestamp,
						    SceUInt32 timeout, SceLocationLocationInfo *linfo)
{
	IpcClientList *list;
	IpcClientInfo *info;

	if (linfo == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	list = SceLibLocation_EFC9BD25();
	if (list == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	IpcClientList::setInstance(list);
	info = IpcClientList::find(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->getLocationWifiHistory(info, true, timestamp, timeout,
								    linfo);
}

/* the system's allocator, here and in SceLibLocation */
extern "C" SceInt32 SceLibLocationInternal_EEF11F68(const LocationAllocator *allocator)
{
	int ret;

	ret = IpcClientList::setAllocator(allocator, 0);
	if (ret == 0)
		ret = SceLibLocation_822BBF06(allocator, 0);
	return ret;
}

/* what the confirmation dialog of the system's request shows */
extern "C" SceInt32 SceLibLocationInternal_209228A3(SceLocationHandle handle,
						    const SceWChar16 *firstText,
						    const SceWChar16 *secondText, SceUInt32 arg)
{
	IpcClientList *list;
	LocationAllocator *allocator;

	list = SceLibLocation_EFC9BD25();
	if (list == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	IpcClientList::setInstance(list);
	if (IpcClientList::find(handle) == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	allocator = SceLibLocation_0C0BB66A();
	return IpcClientList::getInstance()->setConfirmData(handle, allocator, arg, firstText,
							    secondText);
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	IpcClientList::closeAll();
	IpcClientList::term();
	return SCE_KERNEL_STOP_SUCCESS;
}
