/*
 * SceLibLocationFactory_9E020010: one of the service's factory commands, on a connection made
 * for it and closed after it. The heap is made on the first connection and destroyed with the
 * last, as SceLibLocation's sceLocationInit does.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "../../../../../../../lowlayer/src/ipmi/ipmi_error.h"
#include "location_factory.h"

bool LocationFactoryClient::s_useHeap = true;

static const LocationAllocator s_defaultAllocator = { locationHeapAlloc, locationHeapFree };
LocationAllocator LocationFactoryClient::s_allocator = s_defaultAllocator;
int LocationFactoryClient::s_heapRefCount;

extern "C" SceInt32 SceLibLocationFactory_9E020010(SceUInt32 command, const void *in, void *out)
{
	LocationFactoryClient *client = NULL;
	int result;
	int ret;

	switch (command) {
	case LOCATION_FACTORY_COMMAND_4:
	case LOCATION_FACTORY_COMMAND_5:
	case LOCATION_FACTORY_COMMAND_13:
	case LOCATION_FACTORY_COMMAND_14:
	case LOCATION_FACTORY_COMMAND_16:
	case LOCATION_FACTORY_COMMAND_19:
		break;
	default:
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
	}

	ret = LocationFactoryClient::create(&client);
	if (ret != 0)
		return ret;
	result = client->invoke(command, in, out);
	ret = LocationFactoryClient::destroy(&client);
	if (result != 0)
		return result;
	return ret;
}

__attribute__((noinline)) int LocationFactoryClient::connect()
{
	IPMI::Client::Config config;
	void *memory;
	int result;
	int error;
	int ret;

	sceClibStrncpy(config.serviceName, LOCATION_SERVICE_NAME_LOCATION,
		       sizeof(config.serviceName));
	config.budget = IPMI::USER_BUDGET;
	memory = s_allocator.alloc(IPMI::Client::getBufferSize(&config));
	if (memory == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;

	ret = IPMI::Client::create(&client, &config, this, memory);
	if (ret != 0) {
		/* the default allocator's free, whichever allocator gave the memory */
		s_defaultAllocator.free(memory);
		return ret;
	}
	ret = client->connect(NULL, 0, &result);
	if (ret != 0) {
		error = SCE_LOCATION_ERROR_FATAL_ERROR;
		switch (ret) {
		case IPMI_ERROR_CONNECTION_REFUSED:
			error = ret;
			break;
		}
		client->destroy();
		s_allocator.free(memory);
		return error;
	}
	return 0;
}

inline int LocationFactoryClient::acquireHeap(SceSize size)
{
	int ret;

	if (s_useHeap) {
		ret = 0;
		if (s_heapRefCount == 0)
			ret = locationHeapCreate(size);
		if (ret == 0)
			s_heapRefCount++;
		if (ret != 0)
			return ret;
	}
	return 0;
}

inline void LocationFactoryClient::releaseHeap()
{
	if (s_useHeap) {
		if (s_heapRefCount == 1)
			locationHeapDestroy();
		else if (s_heapRefCount == 0)
			return;
		s_heapRefCount--;
	}
}

__attribute__((noinline)) int LocationFactoryClient::create(LocationFactoryClient **ppClient)
{
	LocationFactoryClient *p;
	int ret;

	ret = acquireHeap(LOCATION_HEAP_SIZE);
	if (ret != 0)
		return ret;

	p = new LocationFactoryClient;
	if (p == NULL) {
		releaseHeap();
		return SCE_LOCATION_ERROR_NO_MEMORY;
	}
	ret = p->connect();
	if (ret != 0) {
		delete p;
		*ppClient = NULL;
		releaseHeap();
		return ret;
	}
	*ppClient = p;
	return ret;
}

__attribute__((noinline)) int LocationFactoryClient::destroy(LocationFactoryClient **ppClient)
{
	LocationFactoryClient *p = *ppClient;
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;

	if (p != NULL) {
		if (p->client != NULL) {
			if (p->client->disconnect() != IPMI_ERROR_BUSY) {
				p->client->destroy();
				s_allocator.free(p->client);
				ret = 0;
				p->client = NULL;
			}
			p = *ppClient;
		}
		delete p;
		*ppClient = NULL;
		releaseHeap();
	}
	return ret;
}

/* a command without an answer to keep still gets one written somewhere */
__attribute__((noinline)) int LocationFactoryClient::invoke(SceUInt32 command, const void *in,
							   void *out)
{
	SceUInt8 answer[LOCATION_FACTORY_DATA_SIZE];

	if (out == NULL)
		out = answer;
	return locationServiceFactoryCommand(client, command, in, out);
}
