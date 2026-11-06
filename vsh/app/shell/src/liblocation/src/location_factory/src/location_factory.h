/*
 * SceLibLocationFactory: the factory and debug requests of the location service
 * (SceLibLocationFactory_9E020010), each on a connection of its own made for it. The library
 * keeps SceLibLocation's heap (location_heap.cpp, its block named "SceLocationDebugClient") and
 * allocator. The class and function names are ours.
 */
#ifndef LOCATION_FACTORY_SRC_LOCATION_FACTORY_H
#define LOCATION_FACTORY_SRC_LOCATION_FACTORY_H

#include <stddef.h>
#include <scetypes.h>
#include "../../location_client/src/location_client.h"

/* the size of a request's input and of its answer */
#define LOCATION_FACTORY_DATA_SIZE	0x140

/* the commands the library passes on to the service */
#define LOCATION_FACTORY_COMMAND_4	4
#define LOCATION_FACTORY_COMMAND_5	5
#define LOCATION_FACTORY_COMMAND_13	13
#define LOCATION_FACTORY_COMMAND_14	14
#define LOCATION_FACTORY_COMMAND_16	16
#define LOCATION_FACTORY_COMMAND_19	19

/* one connection to the location service, made for one request */
class LocationFactoryClient {
public:
	IPMI::Client *client;

	LocationFactoryClient()
	{
		client = NULL;
	}

	~LocationFactoryClient()
	{
		if (client != NULL)
			s_allocator.free(client);
		client = NULL;
	}

	static void *operator new(size_t size) throw()
	{
		return s_allocator.alloc(size);
	}

	static void operator delete(void *pointer)
	{
		s_allocator.free(pointer);
	}

	static bool s_useHeap;
	static LocationAllocator s_allocator;
	static int s_heapRefCount;

	static int create(LocationFactoryClient **ppClient);
	static int destroy(LocationFactoryClient **ppClient);

	int connect();
	int invoke(SceUInt32 command, const void *in, void *out);

private:
	static int acquireHeap(SceSize size);
	static void releaseHeap();
};

/* location_service_ipc_client.cpp */
int locationServiceFactoryCommand(IPMI::Client *client, SceUInt32 command, const void *in,
				  void *out);

extern "C" SceInt32 SceLibLocationFactory_9E020010(SceUInt32 command, const void *in, void *out);

#endif
