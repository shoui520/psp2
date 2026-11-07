/*
 * The near background service's methods, each with one output and no input: the method's result, or
 * the call's error.
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../../lowlayer/src/ipmi/client.h"
#include "near_checkin_lib.h"

int NearBGServiceInvokeGetItemDownloadStatus(IPMI::Client *client,
						    NearItemDownloadStatus *status)
{
	IPMI::BufferInfo out;
	int result = 0;
	int ret;

	out.pBuffer = status;
	out.bufferSize = sizeof(*status);
	ret = client->invokeSyncMethod(NEAR_BG_SERVICE_METHOD_GET_ITEM_DOWNLOAD_STATUS, NULL, 0,
				       &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int NearBGServiceInvokeCancelItemDownload(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(NEAR_BG_SERVICE_METHOD_CANCEL_ITEM_DOWNLOAD,
				       (const IPMI::DataInfo *)NULL, 0, &result,
				       (IPMI::BufferInfo *)NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int NearBGServiceInvokeGetCheckinStatus(IPMI::Client *client, NearCheckinStatus *status)
{
	IPMI::BufferInfo out;
	int result = 0;
	int ret;

	out.pBuffer = status;
	out.bufferSize = sizeof(*status);
	ret = client->invokeSyncMethod(NEAR_BG_SERVICE_METHOD_GET_CHECKIN_STATUS, NULL, 0,
				       &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int NearBGServiceInvokeCancelCheckin(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(NEAR_BG_SERVICE_METHOD_CANCEL_CHECKIN,
				       (const IPMI::DataInfo *)NULL, 0, &result,
				       (IPMI::BufferInfo *)NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int NearBGServiceInvokeGetMode(IPMI::Client *client, SceInt32 *mode)
{
	IPMI::BufferInfo out;
	int result = 0;
	int ret;

	out.pBuffer = mode;
	out.bufferSize = sizeof(*mode);
	ret = client->invokeSyncMethod(NEAR_BG_SERVICE_METHOD_GET_MODE, NULL, 0, &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int NearBGServiceInvokeIsBusy(IPMI::Client *client, bool *busy)
{
	IPMI::BufferInfo out;
	int result = 0;
	int ret;

	out.pBuffer = busy;
	out.bufferSize = sizeof(*busy);
	ret = client->invokeSyncMethod(NEAR_BG_SERVICE_METHOD_IS_BUSY, NULL, 0, &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int NearBGServiceInvokeGetReady(IPMI::Client *client, NearBGServiceReady *ready)
{
	IPMI::BufferInfo out;
	int result = 0;
	int ret;

	out.pBuffer = ready;
	out.bufferSize = sizeof(*ready);
	ret = client->invokeSyncMethod(NEAR_BG_SERVICE_METHOD_GET_READY, NULL, 0, &result, &out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}
