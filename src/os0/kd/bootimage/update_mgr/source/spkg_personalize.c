#include <sce/sysmem.h>
#include <sce/sm_comm.h>
#include <psp2kern/kernel/cpu/cache.h>
#include "update_internal.h"
#include "update_packet.h"
#include "sbls.h"
#include "slsk.h"

extern int slskSmId;
int StorageUtilSblsSearchEntry(const SceSlb2Header *header,
		const SceSlb2Entry *entry, const char *name, uint32_t *index,
		uint32_t *sector, uint32_t *size);
int StorageUtilSblsRename(const SceSlb2Header *header, SceSlb2Entry *entries,
		const char *oldName, const char *newName);
int UpdateUtilCreatePacketPayload(UpdatePacketPayload **packetOut,
		SceKernelPAVector **dataOut, SceKernelPAVector **vectorOut,
		void *buffer, SceSize size);
int UpdateUtilFreePARangeVector(SceUID heapId, SceKernelPAVector *vector);

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
	ksceKernelDcacheCleanRange((uint8_t *)sbls + (sourceSector << 9),
			(sourceSize + 63) & ~63);
	result = UpdateUtilSetupSm(0, &slskSmId, 0);
	if (result)
		goto cleanup;
	result = ksceSblSmCommCallFunc(slskSmId, 0x50002, &serviceResult,
			packet, sizeof(*packet));
	if (result)
		goto cleanup;
	result = serviceResult;
	if (result)
		goto cleanup;
	ksceKernelDcacheInvalidateRange((uint8_t *)sbls + (sourceSector << 9),
			(sourceSize + 63) & ~63);
	result = StorageUtilSblsRename(sbls, entries, previousName, retiredName);
	if (!result)
		result = StorageUtilSblsRename(sbls, entries, sourceName, destinationName);
cleanup:
	if (dataVector) {
		UpdateUtilFreePARangeVector(heapId, dataVector);
		ksceKernelFreeHeapMemory(heapId, dataVector);
	}
	if (vectorVector) {
		UpdateUtilFreePARangeVector(heapId, vectorVector);
		ksceKernelFreeHeapMemory(heapId, vectorVector);
	}
	if (packet)
		ksceKernelFreeHeapMemory(heapId, packet);
	if (slskSmId != -1)
		UpdateUtilStopSm(&slskSmId);
	return result;
}
