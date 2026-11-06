/*
 * SceLibLocation's exports: sceLocation* (Sony's SDK API, liblocation.h) check their arguments and
 * call the handle list (location_client.cpp); the NID-named exports are what the library's other
 * modules (liblocation_internal, liblocation_extension, liblocation_factory) use. The file's
 * name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <liblocation.h>
#include "location_client.h"

/* sceLocationSetGpsEmulationFile: the only folder an emulation file may come from */
#define LOCATION_GPS_EMULATION_DIR	"ux0:/data/gpsdata/"

static inline bool isValidLocationMethod(SceUInt32 lmethod)
{
	SceUInt32 method = lmethod & SCE_LOCATION_LMETHOD_MASK;
	SceUInt32 timing = lmethod & SCE_LOCATION_LOCATION_TIMING_MASK;
	SceUInt32 system = lmethod & LOCATION_LMETHOD_SYSTEM;

	if ((lmethod & ~(SCE_LOCATION_LMETHOD_MASK | SCE_LOCATION_LOCATION_TIMING_MASK) &
	     ~LOCATION_LMETHOD_SYSTEM) != 0)
		return false;
	if ((method > SCE_LOCATION_LMETHOD_GPS && method < SCE_LOCATION_LMETHOD_GPS_EMULATION) ||
	    method > SCE_LOCATION_LMETHOD_GPS_EMULATION)
		return false;
	if (timing > SCE_LOCATION_LOCATION_TIMING_10MIN)
		return false;
	if (system != 0 && system != LOCATION_LMETHOD_SYSTEM)
		return false;
	return true;
}

static inline bool isValidHeadingMethod(SceUInt32 hmethod)
{
	SceUInt32 method = hmethod & SCE_LOCATION_HMETHOD_MASK;
	SceUInt32 timing = hmethod & SCE_LOCATION_HEADING_TIMING_MASK;
	SceUInt32 resolution = hmethod & SCE_LOCATION_HEADING_RESOLUTION_MASK;
	SceUInt32 north = hmethod & SCE_LOCATION_HEADING_NORTH_MASK;

	if ((hmethod & ~(SCE_LOCATION_HMETHOD_MASK | SCE_LOCATION_HEADING_TIMING_MASK |
			 SCE_LOCATION_HEADING_RESOLUTION_MASK |
			 SCE_LOCATION_HEADING_FILTER_AVERAGE_MASK |
			 SCE_LOCATION_HEADING_FILTER_HYSTERESIS_MASK |
			 SCE_LOCATION_HEADING_NORTH_MASK)) != 0)
		return false;
	if (method > SCE_LOCATION_HMETHOD_CAMERA)
		return false;
	if (timing > SCE_LOCATION_HEADING_TIMING_1SEC)
		return false;
	if (resolution > SCE_LOCATION_HEADING_RESOLUTION_DIV4)
		return false;
	if (north != SCE_LOCATION_HEADING_NORTH_TRUE && north != SCE_LOCATION_HEADING_NORTH_MAGNETIC)
		return false;
	return true;
}

SceInt32 sceLocationInit(SceUInt32 open, SceUInt32 mode)
{
	SceUInt32 confirm = mode & LOCATION_MODE_CONFIRM_MASK;

	if ((SceUInt8)mode > SCE_LOCATION_MODE_V3 ||
	    (confirm != SCE_LOCATION_MODE_CONFIRM_MANUAL && confirm != SCE_LOCATION_MODE_CONFIRM_AUTO))
		return SCE_LOCATION_ERROR_INVALID_VALUE;
	return IpcClientList::init(open, mode);
}

SceInt32 sceLocationTerm(void)
{
	return IpcClientList::term();
}

SceInt32 sceLocationOpen(SceLocationHandle *handle, SceUInt32 lmethod, SceUInt32 hmethod)
{
	IpcClientInfo *info = NULL;
	SceInt32 ret;

	if (handle == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	if ((lmethod & LOCATION_LMETHOD_SYSTEM) != 0 && !IpcClientList::isSystemInitialized()) {
		*handle = SCE_LOCATION_HANDLE_INVALID;
		return SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
	}
	if (!isValidLocationMethod(lmethod)) {
		*handle = SCE_LOCATION_HANDLE_INVALID;
		return SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
	}
	if (!isValidHeadingMethod(hmethod)) {
		*handle = SCE_LOCATION_HANDLE_INVALID;
		return SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
	}

	ret = IpcClientList::getInstance()->allocClient(&info);
	if (ret != 0) {
		*handle = SCE_LOCATION_HANDLE_INVALID;
		return ret;
	}
	ret = IpcClientList::getInstance()->open(handle, info, lmethod, hmethod);
	if (ret != 0) {
		IpcClientList::removeClient(*handle);
		*handle = SCE_LOCATION_HANDLE_INVALID;
	}
	return ret;
}

SceInt32 sceLocationClose(SceLocationHandle handle)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->close(info);
}

SceInt32 sceLocationReopen(SceLocationHandle handle, SceUInt32 lmethod, SceUInt32 hmethod)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	if ((lmethod & LOCATION_LMETHOD_SYSTEM) != 0 && !IpcClientList::isSystemInitialized())
		return SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
	if (!isValidLocationMethod(lmethod))
		return SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
	if (!isValidHeadingMethod(hmethod))
		return SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
	return IpcClientList::getInstance()->reopen(info, lmethod, hmethod);
}

SceInt32 sceLocationGetMethod(SceLocationHandle handle, SceUInt32 *lmethod, SceUInt32 *hmethod)
{
	IpcClientInfo *info = IpcClientList::find(handle);
	SceUInt32 locationMethod;
	SceUInt32 headingMethod;
	SceInt32 ret;

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	if (lmethod == NULL && hmethod == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;

	ret = IpcClientList::getInstance()->getMethod(info, &locationMethod, &headingMethod);
	if (ret == 0) {
		if (lmethod != NULL)
			*lmethod = locationMethod;
		if (hmethod != NULL)
			*hmethod = headingMethod;
	}
	return ret;
}

SceInt32 sceLocationGetLocation(SceLocationHandle handle, SceLocationLocationInfo *linfo)
{
	IpcClientInfo *info;

	if (linfo == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	info = IpcClientList::find(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->getLocation(info, LOCATION_GET_LOCATION_TIMEOUT, linfo);
}

SceInt32 sceLocationGetLocationWithTimeout(SceLocationHandle handle, SceUInt32 timeout,
					   SceLocationLocationInfo *linfo)
{
	IpcClientInfo *info;

	if (linfo == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	info = IpcClientList::find(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->getLocation(info, timeout, linfo);
}

SceInt32 sceLocationCancelGetLocation(SceLocationHandle handle)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->cancelGetLocation(info);
}

SceInt32 sceLocationStartLocationCallback(SceLocationHandle handle, SceUInt32 distance,
					  SceLocationLocationInfoCallback callback, void *userdata)
{
	IpcClientInfo *info;

	if (callback == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	info = IpcClientList::find(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->startLocationCallback(info, distance, callback,
								   userdata);
}

SceInt32 sceLocationStopLocationCallback(SceLocationHandle handle)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->stopLocationCallback(info);
}

SceInt32 sceLocationGetHeading(SceLocationHandle handle, SceLocationHeadingInfo *hinfo)
{
	IpcClientInfo *info;

	if (hinfo == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	info = IpcClientList::find(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->getHeading(info, hinfo);
}

SceInt32 sceLocationStartHeadingCallback(SceLocationHandle handle, SceFloat32 difference,
					 SceLocationHeadingInfoCallback callback, void *userdata)
{
	IpcClientInfo *info;

	if (callback == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	info = IpcClientList::find(handle);
	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	if (difference < 0.0f || difference > 180.0f)
		return SCE_LOCATION_ERROR_INVALID_VALUE;
	return IpcClientList::getInstance()->startHeadingCallback(info, difference, callback,
								  userdata);
}

SceInt32 sceLocationStopHeadingCallback(SceLocationHandle handle)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->stopHeadingCallback(info);
}

SceInt32 sceLocationConfirm(SceLocationHandle handle)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->confirm(info);
}

SceInt32 sceLocationConfirmGetStatus(SceLocationHandle handle, SceLocationDialogStatus *status)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	if (status == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	return IpcClientList::getInstance()->confirmGetStatus(info, status);
}

SceInt32 sceLocationConfirmGetResult(SceLocationHandle handle, SceLocationDialogResult *result)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	if (result == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	return IpcClientList::getInstance()->confirmGetResult(info, result);
}

SceInt32 sceLocationConfirmAbort(SceLocationHandle handle)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->confirmAbort(info);
}

SceInt32 sceLocationGetPermission(SceLocationHandle handle, SceLocationPermissionInfo *info)
{
	SceLocationPermissionInfo permission;
	IpcClientInfo *client = IpcClientList::find(handle);
	SceInt32 ret;

	if (client == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	sceClibMemset(&permission, 0, sizeof(permission));
	ret = IpcClientList::getInstance()->getPermission(client, &permission);
	if (info != NULL)
		sceClibMemcpy_safe(info, &permission, sizeof(permission));
	return ret;
}

SceInt32 sceLocationDenyApplication(SceLocationHandle handle)
{
	IpcClientInfo *info = IpcClientList::find(handle);

	if (info == NULL)
		return SCE_LOCATION_ERROR_INVALID_HANDLE;
	return IpcClientList::getInstance()->denyApplication(info);
}

SceInt32 sceLocationSetThreadParameter(SceInt32 intPriority, SceInt32 cpuAffinityMask)
{
	return IpcClientList::setThreadParameter(intPriority, cpuAffinityMask);
}

SceInt32 sceLocationSetGpsEmulationFile(unsigned char *filename)
{
	IpcClientInfo *info;
	char path[256];
	SceInt32 result;
	SceInt32 ret;

	if (filename == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	sceClibMemset(path, 0, sizeof(path));
	sceClibMemcpy_safe(path, filename, sizeof(path) - 1);
	if (sceClibStrncasecmp(LOCATION_GPS_EMULATION_DIR, path,
			       sizeof(LOCATION_GPS_EMULATION_DIR) - 1) != 0)
		return SCE_LOCATION_DBGERROR_EMULATION_FILE_ERROR;

	ret = IpcClientList::createTemporaryClient(&info);
	if (ret != 0)
		return ret;
	result = IpcClientList::getInstance()->setGpsEmulationFile(info, path);
	ret = IpcClientList::destroyTemporaryClient(&info);
	if (result != 0)
		ret = result;
	return ret;
}

/* a value of the location service's own, read over a connection of its own */
SceInt32 SceLibLocation_C1CFEC92(SceUInt32 *value)
{
	IpcClientInfo *info;
	SceInt32 result;
	SceInt32 ret;

	if (value == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	if (!IpcClientList::isInitialized())
		return SCE_LOCATION_ERROR_UNINITIALIZED;

	ret = IpcClientList::createTemporaryClient(&info);
	if (ret != 0)
		return ret;
	result = IpcClientList::getInstance()->getServiceValue(info, value);
	ret = IpcClientList::destroyTemporaryClient(&info);
	if (result != 0)
		ret = result;
	return ret;
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	IpcClientList::closeAll();
	IpcClientList::term();
	return 0;
}

IpcClientList *SceLibLocation_EFC9BD25(void)
{
	return IpcClientList::getInstance();
}

void SceLibLocation_7F74AA94(IpcClientList *list)
{
	IpcClientList::setInstance(list);
}

LocationAllocator *SceLibLocation_0C0BB66A(void)
{
	return IpcClientList::getAllocator();
}

SceInt32 SceLibLocation_822BBF06(const LocationAllocator *allocator, SceUInt32 useHeap)
{
	return IpcClientList::setAllocator(allocator, (SceUInt8)useHeap);
}

void SceLibLocation_991AF2D0(SceInt32 *pPriority, SceInt32 *pCpuAffinityMask)
{
	IpcClientList::getThreadParameter(pPriority, pCpuAffinityMask);
}

/* the handle's client, while the library runs as the mode the caller expects */
IpcClientInfo *SceLibLocation_4C8B6F21(SceLocationHandle handle)
{
	SceUInt32 mode = IpcClientList::getInstance()->getMode();

	if (((SceUInt8)mode == SCE_LOCATION_MODE_V3 &&
	     IpcClientList::s_initState == LOCATION_STATE_INITIALIZED) ||
	    (mode == LOCATION_MODE_SYSTEM && IpcClientList::s_initState == LOCATION_STATE_SYSTEM))
		return IpcClientList::find(handle);
	return NULL;
}

SceUInt32 SceLibLocation_0341758B(void)
{
	return IpcClientList::getInstance()->getMode();
}

void SceLibLocation_5C7185D2(void)
{
	IpcClientList::s_extensionLoaded = true;
}

bool SceLibLocation_2311B24A(void)
{
	return IpcClientList::s_extensionLoaded;
}

void SceLibLocation_41C1E68F(int pending)
{
	IpcClientList::getInstance()->setConfirmPending(pending);
}
