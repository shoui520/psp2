/*
 * The location and heading services' methods SceLibLocationInternal calls, one synchronous
 * IPMI call each, as SceLibLocation's and liblocation_extension's location_service_ipc_client.cpp
 * make them (Sony's linker kept only the methods a module calls).
 */
#include <stddef.h>
#include <scetypes.h>
#include "location_internal.h"

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

int locationServiceGetLocationWifiHistory(IPMI::Client *client, SceLocationHandle handle,
					  bool query, bool option, SceRtcTick timestamp,
					  SceUInt32 timeout, SceLocationLocationInfo *linfo)
{
	int result = 0;
	GetLocationWifiHistoryParam param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	param.handle = handle;
	param.query = query;
	param.option = option;
	param.timestamp = timestamp;
	param.timeout = timeout;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = linfo;
	out[0].bufferSize = sizeof(*linfo);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_LOCATION_WIFI_HISTORY, in, 1, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int locationServiceGetLocationWifiHistoryResult(IPMI::Client *client, SceLocationHandle handle,
						bool query, bool option,
						SceLocationLocationInfo *linfo)
{
	int result = 0;
	GetLocationWifiHistoryResultParam param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	param.handle = handle;
	param.query = query;
	param.option = option;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = linfo;
	out[0].bufferSize = sizeof(*linfo);
	ret = client->invokeSyncMethod(LOCATION_METHOD_GET_LOCATION_WIFI_HISTORY_RESULT, in, 1,
				       &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}
