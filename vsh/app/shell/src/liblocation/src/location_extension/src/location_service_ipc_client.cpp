/*
 * The location service's Wi-Fi history methods, one synchronous IPMI call each, like
 * SceLibLocation's location_service_ipc_client.cpp (the file name is Sony's there, ours here).
 */
#include <stddef.h>
#include <scetypes.h>
#include "location_extension.h"

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

int locationServiceCancelQueryLocationWifiHistory(IPMI::Client *client, SceLocationHandle handle)
{
	int result = 0;
	SceLocationHandle param = handle;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(LOCATION_METHOD_CANCEL_QUERY_LOCATION_WIFI_HISTORY, in, 1,
				       &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
