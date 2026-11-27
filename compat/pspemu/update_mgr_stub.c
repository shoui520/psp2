/*
 * update_mgr_stub.c: SceSblSsUpdateMgr user-mode syscall wrappers linked into
 * ScePspemu (3.63/3.65 SDK variant using sceClibMemcpy_safe and zlib-compressed
 * spkg chunk decompression via sceZlibDecompress, 0x8103a774..0x8103a88b).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <modulemgr/certified_file.h>
#include <update_mgr/update_mgr.h>
#include "deflt.h"

/* 0x8103a774 */
int sceSblUsExtractSpackage(int packageType, void *addr, SceSize size, uint32_t flags,
			    uint32_t *requestId)
{
	SceKernelSpackageArgs args;

	sceClibMemset(&args, 0, sizeof(args));
	args.arg_size = sizeof(args);
	args.package_type = packageType;
	args.addr = addr;
	args.size = size;
	args.flags = flags;
	return _sceSblUsExtractSpackage(packageType, &args, requestId);
}

/* 0x8103a7a4 */
int sceSblUsGetExtractSpackage(int nodeType, uint32_t requestId, uint32_t extension,
			       SceSize bufferSize, void *buffer)
{
	SceKernelSpackageArgs args;
	const uint8_t *file;
	const SceSblUsExtractedSpackage *package;
	const uint8_t *payload;
	uint64_t offset;
	uint64_t payloadSize;
	unsigned int adler = 0U;
	int ret;

	if (buffer == NULL)
		return SCE_SBL_US_ERROR_INVALID_ARGUMENT;

	sceClibMemset(&args, 0, sizeof(args));
	args.arg_size = sizeof(args);
	args.package_type = nodeType;
	args.extension[0] = extension;
	args.extension[1] = bufferSize;
	ret = _sceSblUsGetExtractSpackage(nodeType, requestId, &args);
	if (ret != 0)
		return ret;

	file = (const uint8_t *)args.addr;
	offset = ((const cf_header *)(const void *)file)->file_offset;
	package = (const SceSblUsExtractedSpackage *)(const void *)&file[offset];
	payload = &file[offset + sizeof(*package)];
	payloadSize = package->chunk.size;

	if ((uint64_t)bufferSize < payloadSize)
		return SCE_SBL_US_ERROR_INVALID_ARGUMENT;

	if (package->info.decryptedSize == package->info.finalSize) {
		sceClibMemcpy_safe(buffer, payload, (SceSize)payloadSize);
		return 0;
	}

	if (sceZlibDecompress(buffer, bufferSize, payload, &adler) >= 0)
		return 0;

	sceClibMemset(buffer, 0, bufferSize);
	return SCE_SBL_US_ERROR_INVALID_ARGUMENT;
}

/* 0x8103a844 */
int sceSblUsAllocateBuffer(SceSize size, void **buffer)
{
	return _sceSblUsAllocateBuffer(size, buffer);
}

/* 0x8103a84c */
int sceSblUsReleaseBuffer(void *buffer)
{
	return _sceSblUsReleaseBuffer(buffer);
}

/* 0x8103a854 */
int sceSblUsGetStatus(int nodeType, uint32_t requestId, uint32_t *seqNo, uint32_t *result,
		      uint32_t *progress, uint32_t *writtenRates)
{
	SceKernelSpackageArgs args;

	sceClibMemset(&args, 0, sizeof(args));
	args.arg_size = sizeof(args);
	args.seq_no = seqNo;
	args.result = result;
	args.progress = progress;
	args.written_rates = writtenRates;
	return _sceSblUsGetStatus(nodeType, requestId, &args);
}
