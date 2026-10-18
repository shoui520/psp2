#include <threadmgr/threadmgr.h>
#include "update_internal.h"
#include "update_packet.h"

int UpdateUtilCreatePacketPayload(UpdatePacketPayload **packetOut,
		SceKernelPAVector **dataOut, SceKernelPAVector **vectorOut,
		void *buffer, SceSize size);

int UpdateUtilCreatePacketPayloadWithBuffer(void **packet,
		SceKernelPAVector **dataVector, SceKernelPAVector **vectorVector,
		void **bufferOut, SceSize size)
{
	void *buffer = NULL;
	void *releaseBuffer;
	int result;

	if (!packet || !dataVector)
		return (int)0x800f0216;
	if (!vectorVector || !bufferOut)
		return (int)0x800f0216;
	if (!size)
		return (int)0x800f0216;
	if (UpdateUtilAllocateBuffer(size, &buffer, sceKernelGetProcessId())) {
		result = (int)0x800f020c;
		goto release;
	}
	if (!buffer)
		return (int)0x800f020c;
	result = UpdateUtilCreatePacketPayload((UpdatePacketPayload **)packet,
			dataVector, vectorVector, buffer, size);
	if (!result) {
		*bufferOut = buffer;
		return result;
	}
release:
	releaseBuffer = buffer;
	if (releaseBuffer)
		UpdateUtilReleaseBuffer(releaseBuffer, sceKernelGetProcessId());
	return result;
}
