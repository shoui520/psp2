#include <sysmem/sysmem.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/cpu.h>
#include "update_internal.h"
#include "update_packet.h"
#include "sbls.h"
#include "slsk.h"

extern int slskSmId;

int SpkgUtilDoPersonalizeSLSK(const char *sourceName,
		const char *destinationName, const char *previousName,
		const char *retiredName, SceSlb2Header *sbls)
{
	unsigned int sourceIndex, sourceSector, sourceSize;
	unsigned int previousIndex, previousSector, previousSize;
	unsigned int serviceResult = 0;
	SceKernelPAVector *dataVector = NULL, *vectorVector = NULL;
	UpdatePacketPayload *packet = NULL;
	SceSlb2Entry *entries = sbls->entryList;
	SlskHeader *header;
	SceUID heapId = UpdateUtilGetHeapId();
	int result;

	if (StorageUtilSblsSearchEntry(sbls, entries, sourceName,
			&sourceIndex, &sourceSector, &sourceSize) ||
	    StorageUtilSblsSearchEntry(sbls, entries, previousName,
			&previousIndex, &previousSector, &previousSize)) {
		result = (int)0x800f0224;
		goto cleanup;
	}
	header = (SlskHeader *)((uint8_t *)sbls + (sourceSector << 9));
	if (header->magic != 0x64b2c8e5) {
		result = (int)0x800f0224;
		goto cleanup;
	}
	result = UpdateUtilCreatePacketPayload(&packet, &dataVector, &vectorVector,
			(uint8_t *)header + header->headerSize - sizeof(SlskPersonalization),
			header->bodySize + sizeof(SlskPersonalization));
	if (result)
		goto cleanup;
	sceKernelDcacheCleanRange((uint8_t *)sbls + (sourceSector << 9),
			(sourceSize + 63) & ~63);
	result = UpdateUtilSetupSm(0, &slskSmId, 0);
	if (result)
		goto cleanup;
	result = sceSblSmCommCallFunc(slskSmId, 0x50002, &serviceResult,
			packet, sizeof(*packet));
	if (result)
		goto cleanup;
	result = serviceResult;
	if (result)
		goto cleanup;
	sceKernelDcacheInvalidateRange((uint8_t *)sbls + (sourceSector << 9),
			(sourceSize + 63) & ~63);
	result = StorageUtilSblsRename(sbls, entries, previousName, retiredName);
	if (!result)
		result = StorageUtilSblsRename(sbls, entries, sourceName, destinationName);
cleanup:
	if (dataVector) {
		UpdateUtilFreePARangeVector(heapId, dataVector);
		sceKernelFreeHeapMemory(heapId, dataVector);
	}
	if (vectorVector) {
		UpdateUtilFreePARangeVector(heapId, vectorVector);
		sceKernelFreeHeapMemory(heapId, vectorVector);
	}
	if (packet)
		sceKernelFreeHeapMemory(heapId, packet);
	if (slskSmId != -1)
		UpdateUtilStopSm(&slskSmId);
	return result;
}
