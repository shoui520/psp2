/*
 * The shell's telephony methods over SceShellSvc's IPMI connection, each a synchronous call that
 * returns the call's error or the method's result. Sony's build keeps them out of the module's
 * file (after the exports, not inlined); the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "telephony_util_ipc_client.h"

#define TELEPHONY_UTIL_METHOD_IS_AVAILABLE	0x50051
#define TELEPHONY_UTIL_METHOD_50052		0x50052
#define TELEPHONY_UTIL_METHOD_50053		0x50053

int telephonyUtilIsAvailable(IPMI::Client *client, const TelephonyUtilReserved *pReserved,
			     SceUInt32 serviceType)
{
	int result = 0;
	SceUInt32 type = serviceType;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &type;
	in[0].dataSize = sizeof(type);
	in[1].pData = pReserved;
	in[1].dataSize = sizeof(*pReserved);
	ret = client->invokeSyncMethod(TELEPHONY_UTIL_METHOD_IS_AVAILABLE, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int telephonyUtilRequest50052(IPMI::Client *client, const TelephonyUtilReserved *pReserved,
			      SceUInt32 serviceType, SceUInt64 *pValue)
{
	int result = 0;
	SceUInt32 type = serviceType;
	IPMI::BufferInfo out[1];
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &type;
	in[0].dataSize = sizeof(type);
	in[1].pData = pReserved;
	in[1].dataSize = sizeof(*pReserved);
	out[0].pBuffer = pValue;
	out[0].bufferSize = sizeof(*pValue);
	ret = client->invokeSyncMethod(TELEPHONY_UTIL_METHOD_50052, in, 2, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int telephonyUtilRequest50053(IPMI::Client *client, const TelephonyUtilReserved *pReserved,
			      SceUInt32 serviceType)
{
	int result = 0;
	SceUInt32 type = serviceType;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &type;
	in[0].dataSize = sizeof(type);
	in[1].pData = pReserved;
	in[1].dataSize = sizeof(*pReserved);
	ret = client->invokeSyncMethod(TELEPHONY_UTIL_METHOD_50053, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
