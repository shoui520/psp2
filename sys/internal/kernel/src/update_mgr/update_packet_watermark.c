#include <sysmem/sysmem.h>
#include <sysmem/cpu.h>
#include <string.h>
#include "update_internal.h"
#include "update_objects.h"
#include "update_packet.h"

int sceSblSsInfraAllocatePARangeVector(void *buffer, SceSize size,
		SceUID heapId, SceKernelPAVector *vector);
int SceSblSsMgrForDriver_C38D0CEA(SceUID heapId, SceKernelPAVector *vector);

int UpdateUtilCreatePacketPayloadPupWM(UpdateDigestPacketPayload **packetOut,
		SceKernelPAVector **dataOut, SceKernelPAVector **vectorOut,
		const PupHeaderDigest *headerDigest, void *buffer, SceSize size)
{
	SceUID heapId = updateHeapId;
	SceKernelPAVector *dataVector = NULL, *vectorVector = NULL;
	SceKernelPARange *dataRanges;
	SceUInt32 dataCount;
	UpdateDigestPacketPayload *packet;
	SceAllocOpt option;
	int result;

	if (!dataOut || !packetOut)
		return (int)0x800f0216;
	if (!headerDigest || !vectorOut)
		return (int)0x800f0216;
	if (!size || !buffer)
		return (int)0x800f0216;
	option.size = sizeof(option);
	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.align = 4096;
	dataVector = SceSysmemForDriver_7B4CB60A(heapId, sizeof(*dataVector));
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
	vectorVector = SceSysmemForDriver_49D4DD9B(heapId, 64, &option);
	if (!vectorVector) {
		result = (int)0x800f020c;
		goto freeData;
	}
	dataCount = dataVector->total;
	dataRanges = dataVector->pRanges;
	vectorVector->pRanges = NULL;
	vectorVector->capacity = 0;
	if (sceSblSsInfraAllocatePARangeVector(dataRanges,
			8 * dataCount, heapId, vectorVector)) {
		result = (int)0x800f0205;
		goto freeVector;
	}
	packet = SceSysmemForDriver_7B4CB60A(heapId, sizeof(*packet));
	if (!packet) {
		result = (int)0x800f020c;
		goto freeVector;
	}
	memset(packet, 0, sizeof(*packet));
	packet->vectorVector = *vectorVector;
	packet->dataVector = *dataVector;
	packet->headerDigest = *headerDigest;
	if (vectorVector->total >= 482) {
		result = (int)0x800f020c;
		sceKernelFreeHeapMemory(heapId, packet);
		goto freeVector;
	}
	packet->descriptorFormat = 1;
#if FW_VERSION >= 0x04000000
	__builtin___memcpy_chk(packet->descriptors, vectorVector->pRanges,
			8 * vectorVector->total, sizeof(packet->descriptors));
#else
	memcpy(packet->descriptors, vectorVector->pRanges, 8 * vectorVector->total);
#endif
	SceCpuForDriver_103872A5(dataVector->pRanges,
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
