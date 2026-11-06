/*
 * The location service's factory method: the command and its input, and the answer, in one
 * synchronous IPMI call, like SceLibLocation's location_service_ipc_client.cpp.
 */
#include <stddef.h>
#include <scetypes.h>
#include "location_factory.h"

#define LOCATION_METHOD_FACTORY_COMMAND		0x12340013

int locationServiceFactoryCommand(IPMI::Client *client, SceUInt32 command, const void *in,
				  void *out)
{
	int result = 0;
	SceUInt32 param = command;
	IPMI::DataInfo inList[2];
	IPMI::BufferInfo outList[1];
	int ret;

	inList[0].pData = &param;
	inList[0].dataSize = sizeof(param);
	inList[1].pData = in;
	inList[1].dataSize = LOCATION_FACTORY_DATA_SIZE;
	outList[0].pBuffer = out;
	outList[0].bufferSize = LOCATION_FACTORY_DATA_SIZE;
	ret = client->invokeSyncMethod(LOCATION_METHOD_FACTORY_COMMAND, inList, 2, &result,
				       outList, 1);
	if (ret != 0)
		return ret;
	return result;
}
