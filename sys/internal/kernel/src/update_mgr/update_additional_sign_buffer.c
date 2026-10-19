#include <threadmgr/threadmgr.h>
#include "update_internal.h"
#include "update_packet.h"

int UpdateUtilCreatePacketPayloadPupASWithBuffer(void **packet,
		SceKernelPAVector **dataVector, SceKernelPAVector **vectorVector,
		const PupHeaderDigest *headerDigest, void **bufferOut, SceSize size)
{
	void *buffer = NULL;
	int result;

	if (!dataVector || !packet)
		return (int)0x800f0216;
	if (!headerDigest || !vectorVector)
		return (int)0x800f0216;
	if (!size || !bufferOut)
		return (int)0x800f0216;
	if (UpdateUtilAllocateBuffer(size, &buffer, sceKernelGetProcessId())) {
		result = (int)0x800f020c;
		goto release;
	}
	if (!buffer)
		return (int)0x800f020c;
	result = UpdateUtilCreatePacketPayloadPupAS((UpdateDigestPacketPayload **)packet,
			dataVector, vectorVector, headerDigest, buffer, size);
	if (!result) {
		*bufferOut = buffer;
		return result;
	}
release:
	{
		void *releaseBuffer = buffer;

		if (releaseBuffer)
			UpdateUtilReleaseBuffer(releaseBuffer, sceKernelGetProcessId());
	}
	return result;
}
