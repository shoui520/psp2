/*
 * The location and heading services' methods, one synchronous IPMI call each: the method's
 * arguments in, its outputs back, and the service's result. Sony's file name (the records of
 * liblocation_extension's build name location_service_ipc_client.cpp); the functions are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "location_service_ipc_client.h"

#define LOCATION_METHOD_OPEN			0x12340000
#define LOCATION_METHOD_CLOSE			0x12340001
#define LOCATION_METHOD_REOPEN			0x12340002
#define LOCATION_METHOD_GET_METHOD		0x12340003
#define LOCATION_METHOD_GET_LOCATION_RESULT	0x12340005
#define LOCATION_METHOD_CANCEL_GET_LOCATION	0x12340006
#define LOCATION_METHOD_START_LOCATION_CALLBACK	0x12340007
#define LOCATION_METHOD_STOP_LOCATION_CALLBACK	0x12340008
#define LOCATION_METHOD_GET_HEADING		0x12340009
#define LOCATION_METHOD_START_HEADING_CALLBACK	0x1234000A
#define LOCATION_METHOD_STOP_HEADING_CALLBACK	0x1234000B
#define LOCATION_METHOD_CONFIRM			0x1234000C
#define LOCATION_METHOD_GET_CONFIRM_RESULT	0x1234000E
#define LOCATION_METHOD_CONFIRM_ABORT		0x1234000F
#define LOCATION_METHOD_GET_PERMISSION		0x12340010
#define LOCATION_METHOD_DENY_APPLICATION	0x12340011
#define LOCATION_METHOD_SET_GPS_EMULATION_FILE	0x12340012
#define LOCATION_METHOD_GET_LOCATION		0x12340014
#define LOCATION_METHOD_GET_VALUE		0x12340017

/* the confirmation dialog's data the service reads */
#define LOCATION_CONFIRM_DATA_SIZE		2048
/* the emulation file's path */
#define LOCATION_GPS_EMULATION_PATH_SIZE	256

struct OpenParam {
	SceUInt32 lmethod;
	SceUInt32 hmethod;
	SceUInt32 mode;
};

struct ReopenParam {
	SceLocationHandle handle;
	SceUInt32 lmethod;
	SceUInt32 hmethod;
};

struct StartLocationCallbackParam {
	SceLocationHandle handle;
	SceUInt32 distance;
};

struct StartHeadingCallbackParam {
	SceLocationHandle handle;
	SceFloat32 difference;
};

struct ConfirmParam {
	SceLocationHandle handle;
	SceUInt32 arg;
	SceUInt32 dataArg;
};

struct GetLocationParam {
	SceLocationHandle handle;
	SceUInt32 timeout;
};

int locationServiceOpen(IPMI::Client *client, SceLocationHandle *handle, SceUInt32 lmethod,
			SceUInt32 hmethod, SceUInt32 mode, int *pStatus)
{
	int result = 0;
	OpenParam param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[2];
	int ret;

	param.lmethod = lmethod;
	param.hmethod = hmethod;
	param.mode = mode;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = handle;
	out[0].bufferSize = sizeof(*handle);
	out[1].pBuffer = pStatus;
	out[1].bufferSize = sizeof(*pStatus);
	ret = client->invokeSyncMethod(LOCATION_METHOD_OPEN, in, 1, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceClose(IPMI::Client *client, SceLocationHandle handle)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_CLOSE, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceReopen(IPMI::Client *client, SceLocationHandle handle, SceUInt32 lmethod,
			  SceUInt32 hmethod)
{
	int result = 0;
	ReopenParam param;
	IPMI::DataInfo in[1];
	int ret;

	param.handle = handle;
	param.lmethod = lmethod;
	param.hmethod = hmethod;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_REOPEN, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetMethod(IPMI::Client *client, SceLocationHandle handle, SceUInt32 *lmethod,
			     SceUInt32 *hmethod)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[2];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = lmethod;
	out[0].bufferSize = sizeof(*lmethod);
	out[1].pBuffer = hmethod;
	out[1].bufferSize = sizeof(*hmethod);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_METHOD, in, 1, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetLocationResult(IPMI::Client *client, SceLocationHandle handle,
				     SceLocationLocationInfo *linfo)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = linfo;
	out[0].bufferSize = sizeof(*linfo);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_LOCATION_RESULT, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceCancelGetLocation(IPMI::Client *client, SceLocationHandle handle)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_CANCEL_GET_LOCATION, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceStartLocationCallback(IPMI::Client *client, SceLocationHandle handle,
					 SceUInt32 distance)
{
	int result = 0;
	StartLocationCallbackParam param;
	IPMI::DataInfo in[1];
	int ret;

	param.handle = handle;
	param.distance = distance;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_START_LOCATION_CALLBACK, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceStopLocationCallback(IPMI::Client *client, SceLocationHandle handle)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_STOP_LOCATION_CALLBACK, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetHeading(IPMI::Client *client, SceLocationHandle handle,
			      SceLocationHeadingInfo *hinfo)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = hinfo;
	out[0].bufferSize = sizeof(*hinfo);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_HEADING, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceStartHeadingCallback(IPMI::Client *client, SceLocationHandle handle,
					SceFloat32 difference)
{
	int result = 0;
	StartHeadingCallbackParam param;
	IPMI::DataInfo in[1];
	int ret;

	param.handle = handle;
	param.difference = difference;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_START_HEADING_CALLBACK, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceStopHeadingCallback(IPMI::Client *client, SceLocationHandle handle)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_STOP_HEADING_CALLBACK, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceConfirm(IPMI::Client *client, SceLocationHandle handle, SceUInt32 arg,
			   SceUInt32 dataArg, const void *data)
{
	int result = 0;
	ConfirmParam param;
	IPMI::DataInfo in[2];
	int ret;

	param.handle = handle;
	param.arg = arg;
	param.dataArg = dataArg;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = data;
	in[1].dataSize = LOCATION_CONFIRM_DATA_SIZE;
	ret = client->invokeSyncMethod(LOCATION_METHOD_CONFIRM, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetConfirmResult(IPMI::Client *client, SceLocationHandle handle,
				    SceLocationDialogResult *dialogResult)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = dialogResult;
	out[0].bufferSize = sizeof(*dialogResult);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_CONFIRM_RESULT, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceConfirmAbort(IPMI::Client *client, SceLocationHandle handle)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_CONFIRM_ABORT, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetPermission(IPMI::Client *client, SceLocationHandle handle,
				 SceLocationPermissionInfo *permission)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = permission;
	out[0].bufferSize = sizeof(*permission);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_PERMISSION, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceDenyApplication(IPMI::Client *client, SceLocationHandle handle)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_DENY_APPLICATION, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceSetGpsEmulationFile(IPMI::Client *client, const char *path)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = path;
	in[0].dataSize = LOCATION_GPS_EMULATION_PATH_SIZE;
	ret = client->invokeSyncMethod(LOCATION_METHOD_SET_GPS_EMULATION_FILE, in, 1, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetLocation(IPMI::Client *client, SceLocationHandle handle, SceUInt32 timeout,
			       SceLocationLocationInfo *linfo)
{
	int result = 0;
	GetLocationParam param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	param.handle = handle;
	param.timeout = timeout;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = linfo;
	out[0].bufferSize = sizeof(*linfo);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_LOCATION, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetValue(IPMI::Client *client, SceUInt32 *value)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = value;
	out[0].bufferSize = sizeof(*value);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_VALUE, NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}
