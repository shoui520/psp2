/* SCE module startup; original source filename is unavailable. */

#include "net_ps_internal.h"
#include <net_ps/netps_gateway.h>
#include <string.h>

extern struct NetDriver *gNetPsDriverHead;
extern SceNetPsProcessHooks gNetPsProcessHooks;

int sceNetPsInitializeGateway(SceNetPsGatewayState *slot, struct NetDriver *driver);
int sceNetPsInitializeDebugInterface(void *slot, struct NetDriver *driver);
int sceNetPsInitializeHeap(void *base, int size);
int sceNetPsStartStack(const SceNetPsThreadParam *param);
SceUID sceNetPsGetSoftThread(void);
struct NetDriverPacket *sceNetPsAllocatePacket(struct NetDriver *driver,
		unsigned int length, unsigned int alignment);
void sceNetPsFreePacket(struct NetDriverPacket *packet);

SceUID gNetPsMemoryId;
SceUID gNetPsSystemEventId;
SceUID gNetPsThreadEventId;
SceUID gNetPsProcessEventId;
void *gNetPsMemoryBase;
int gNetPsMemorySize;
SceNetPsGatewayState gNetPsGatewayState;
void *gNetPsDebugInterface;
struct SceNetPsDriverContext *gNetPsAllocatedDriverContext;

/* Name unknown; initialize platform services and prime the packet cache. */
int sceNetPsStartModule(SceUID memoryId, void *base, int size,
		SceNetPsThreadParam *thread, SceUID systemId,
		SceUID threadId, SceUID processId)
{
	struct NetDriverPacket *packets[32];
	unsigned int index;

	gNetPsThreadEventId = threadId;
	gNetPsMemoryId = memoryId;
	gNetPsSystemEventId = systemId;
	gNetPsProcessEventId = processId;
	gNetPsMemoryBase = base;
	gNetPsMemorySize = size;
	memset(&gNetPsGatewayState.interface, 0,
	       sizeof(gNetPsGatewayState.interface));
	sceNetPsInitializeGateway(&gNetPsGatewayState, gNetPsDriverHead);
	memset(&gNetPsDebugInterface, 0, sizeof(gNetPsDebugInterface));
	sceNetPsInitializeDebugInterface(&gNetPsDebugInterface, gNetPsDriverHead);
	sceNetPsInitializeHeap(base, size);
	sceNetPsStartStack(thread);
	gNetPsProcessHooks.softThread = sceNetPsGetSoftThread();
	for (index = 0; index < 32; index++)
		packets[index] = sceNetPsAllocatePacket(NULL, 1600, 128);
	for (index = 0; index < 32; index++)
		sceNetPsFreePacket(packets[index]);
	gNetPsAllocatedDriverContext = sceNetPsMalloc(
			sizeof(*gNetPsAllocatedDriverContext), 3, 8);
	return 0;
}
