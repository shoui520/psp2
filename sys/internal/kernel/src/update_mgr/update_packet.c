#include <sysmem/sysmem.h>
#include <sysmem/cpu.h>
#include <string.h>
#include "update_internal.h"
#include "update_objects.h"
#include "update_packet.h"
#include <ss_mgr/ss_mgr.h>

int UpdateUtilCreatePacketPayload(UpdatePacketPayload **packetOut,
		SceKernelPAVector **dataOut, SceKernelPAVector **vectorOut,
		void *buffer, SceSize size)
{
	SceUID heapId = updateHeapId;
	SceKernelPAVector *dataVector = NULL, *vectorVector = NULL;
	UpdatePacketPayload *packet;
	SceAllocOpt option;
	unsigned int vectorCount;
	int result;

	if (!dataOut || !packetOut)
		return (int)0x800f0216;
	if (!buffer || !vectorOut || !size)
		return (int)0x800f0216;
	option.size = sizeof(option);
	option.mmapKind = 0;
	option.align = 4096;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	dataVector = sceKernelAllocHeapMemory(heapId, sizeof(*dataVector));
	if (!dataVector) {
		result = (int)0x800f020c;
		goto freeData;
	}
	dataVector->pRanges = NULL;
	dataVector->capacity = 0;
	if (sceSblSsInfraAllocatePARangeVector(buffer, size, heapId, dataVector)) {
		result = (int)0x800f0205;
		goto freeData;
	}
	vectorVector = sceKernelAllocHeapMemoryWithOption(heapId, 64, &option);
	if (!vectorVector) {
		result = (int)0x800f020c;
		goto freeData;
	}
	vectorVector->pRanges = NULL;
	vectorVector->capacity = 0;
	if (sceSblSsInfraAllocatePARangeVector(dataVector->pRanges,
			8 * dataVector->total, heapId, vectorVector)) {
		result = (int)0x800f0205;
		goto freeVector;
	}
	packet = sceKernelAllocHeapMemory(heapId, sizeof(*packet));
	if (!packet) {
		result = (int)0x800f020c;
		goto freeVector;
	}
	memset(packet, 0, sizeof(*packet));
	packet->vectorVector = *vectorVector;
	packet->dataVector = *dataVector;
	vectorCount = vectorVector->total;
	if (vectorCount >= 482) {
		result = (int)0x800f020c;
		sceKernelFreeHeapMemory(heapId, packet);
		goto freeVector;
	}
	packet->descriptorFormat = 1;
#if FW_VERSION >= 0x04000000
	__builtin___memcpy_chk(packet->records.descriptors, vectorVector->pRanges,
			8 * vectorCount, sizeof(packet->records));
#else
	memcpy(packet->records.descriptors, vectorVector->pRanges,
			8 * vectorCount);
#endif
	sceKernelDcacheCleanRange(dataVector->pRanges,
			(8 * dataVector->total + 63) & ~63);
	*packetOut = packet;
	*dataOut = dataVector;
	*vectorOut = vectorVector;
	return 0;

freeVector:
	if (vectorVector) {
		if (vectorVector->pRanges)
			SceSblSsMgrForDriver_C38D0CEA(heapId, vectorVector);
		sceKernelFreeHeapMemory(heapId, vectorVector);
	}
freeData:
	if (dataVector) {
		if (dataVector->pRanges)
			SceSblSsMgrForDriver_C38D0CEA(heapId, dataVector);
		sceKernelFreeHeapMemory(heapId, dataVector);
	}
	return result;
}
