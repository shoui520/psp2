#include <sysmem/sysmem.h>
#include <psp2kern/kernel/cpu/cache.h>
#include <string.h>
#include "update_internal.h"
#include "update_objects.h"
#include "update_packet.h"

int sceSblSsInfraAllocatePARangeVector(void *buffer, SceSize size,
		SceUID heapId, SceKernelPAVector *vector);
int SceSblSsMgrForDriver_C38D0CEA(SceUID heapId, SceKernelPAVector *vector);

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
	dataVector = ksceKernelAllocHeapMemory(heapId, sizeof(*dataVector));
	if (!dataVector) {
		result = (int)0x800f020c;
		goto freeData;
	}
	dataVector->ranges = NULL;
	dataVector->ranges_size = 0;
	if (sceSblSsInfraAllocatePARangeVector(buffer, size, heapId, dataVector)) {
		result = (int)0x800f0205;
		goto freeData;
	}
	vectorVector = ksceKernelAllocHeapMemoryWithOption(heapId, 64, &option);
	if (!vectorVector) {
		result = (int)0x800f020c;
		goto freeData;
	}
	vectorVector->ranges = NULL;
	vectorVector->ranges_size = 0;
	if (sceSblSsInfraAllocatePARangeVector(dataVector->ranges,
			8 * dataVector->count, heapId, vectorVector)) {
		result = (int)0x800f0205;
		goto freeVector;
	}
	packet = ksceKernelAllocHeapMemory(heapId, sizeof(*packet));
	if (!packet) {
		result = (int)0x800f020c;
		goto freeVector;
	}
	memset(packet, 0, sizeof(*packet));
	vectorCount = vectorVector->count;
	packet->vectorVector = *vectorVector;
	packet->dataVector = *dataVector;
	if (vectorCount >= 482) {
		result = (int)0x800f020c;
		ksceKernelFreeHeapMemory(heapId, packet);
		goto freeVector;
	}
	packet->descriptorFormat = 1;
#if FW_VERSION >= 0x04000000
	__builtin___memcpy_chk(packet->records.descriptors, vectorVector->ranges,
			8 * vectorCount, sizeof(packet->records));
#else
	memcpy(packet->records.descriptors, vectorVector->ranges,
			8 * vectorCount);
#endif
	ksceKernelDcacheCleanRange(dataVector->ranges,
			(8 * dataVector->count + 63) & ~63);
	*packetOut = packet;
	*dataOut = dataVector;
	*vectorOut = vectorVector;
	return 0;

freeVector:
	if (vectorVector) {
		if (vectorVector->ranges)
			SceSblSsMgrForDriver_C38D0CEA(heapId, vectorVector);
		ksceKernelFreeHeapMemory(heapId, vectorVector);
	}
freeData:
	if (dataVector) {
		if (dataVector->ranges)
			SceSblSsMgrForDriver_C38D0CEA(heapId, dataVector);
		ksceKernelFreeHeapMemory(heapId, dataVector);
	}
	return result;
}
