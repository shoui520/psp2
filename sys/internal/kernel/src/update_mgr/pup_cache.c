#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include "update_internal.h"

int PupUtilPupGetCachedHeader(PupCachedHeader *header)
{
	PupSegment *segments, *fileSegments;
	PupSegmentSignature *fileSignatures;
	PupSegmentSignature *signatures;
	PupHeader *fileHeader;
	SceUID heapId;
	uint32_t count;

	heapId = UpdateUtilGetHeapId();
	if (!header)
		return (int)0x800f0216;
	UpdateUtilLockPupHeader();
	memcpy(&header->header, &pupFileHeader, sizeof(PupHeader));
	count = pupFileHeader.m_segment_num;
	segments = SceSysmemForDriver_7B4CB60A(heapId,
		(sizeof(PupSegment) + sizeof(PupSegmentSignature)) * count + sizeof(PupHeaderDigest));
	if (!segments) {
		UpdateUtilUnlockPupHeader();
		return (int)0x800f020c;
	}
	header->allocation = segments;
	fileHeader = pupFileBuffer;
	header->segments = segments;
	fileSegments = (PupSegment *)(fileHeader + 1);
	memcpy(header->segments, fileSegments, sizeof(PupSegment) * count);
	signatures = (PupSegmentSignature *)(segments + count);
	header->signatures = signatures;
	fileSignatures = (PupSegmentSignature *)(fileSegments + count);
	memcpy(signatures, fileSignatures, sizeof(PupSegmentSignature) * count);
	header->headerDigest = (PupHeaderDigest *)(signatures + count);
	*header->headerDigest = *(PupHeaderDigest *)(fileSignatures + count);
	UpdateUtilUnlockPupHeader();
	return 0;
}

int PupUtilPupFreeCachedHeader(void)
{
	PupHeader *buffer;

	UpdateUtilLockPupHeader();
	buffer = pupFileBuffer;
	pupFileBufferSize = 0;
	if (buffer)
		UpdateUtilReleaseBuffer(buffer, sceKernelGetProcessId());
	pupFileBuffer = NULL;
	return UpdateUtilUnlockPupHeader();
}
