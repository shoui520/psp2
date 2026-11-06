/*
 * The user side of SceSblSsUpdateMgr's system calls, as the stub library links them into a
 * program: each wrapper reaches the system call of its name with a leading underscore, the
 * package calls with their arguments in a SceKernelSpackageArgs block (the library's and the
 * file's names ours; the order is cui_setupper's).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <modulemgr/certified_file.h>
#include <update_mgr/update_mgr.h>


int sceSblUsVerifyPup(const char *path)
{
	return _sceSblUsVerifyPup(path);
}

int sceSblUsVerifyPupHeader(const char *path)
{
	return _sceSblUsVerifyPupHeader(path);
}

int sceSblUsVerifyPupSegmentById(const char *path, SceUInt64 segmentId)
{
	return _sceSblUsVerifyPupSegmentById(path, segmentId);
}

int sceSblUsUpdateSpackage(int packageType, void *addr, SceSize size, uint32_t flags,
			   uint32_t *requestId)
{
	SceKernelSpackageArgs args;

	sceClibMemset(&args, 0, sizeof(args));
	args.arg_size = sizeof(args);
	args.package_type = packageType;
	args.addr = addr;
	args.size = size;
	args.flags = flags;
	return _sceSblUsUpdateSpackage(packageType, &args, requestId);
}

int sceSblUsInspectSpackage(int packageType, void *addr, SceSize size, uint32_t flags,
			    uint32_t *requestId)
{
	SceKernelSpackageArgs args;

	sceClibMemset(&args, 0, sizeof(args));
	args.arg_size = sizeof(args);
	args.package_type = packageType;
	args.addr = addr;
	args.size = size;
	args.flags = flags;
	return _sceSblUsInspectSpackage(packageType, &args, requestId);
}

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

/*
 * The extracted package comes back as a buffer of the update service's: its chunk's data, past
 * the certified-file header and the package's two records, is copied to the caller's buffer.
 */
int sceSblUsGetExtractSpackage(int nodeType, uint32_t requestId, uint32_t extension,
			       SceSize bufferSize, void *buffer)
{
	SceKernelSpackageArgs args;
	const SceSblUsExtractedSpackage *package;
	const char *file;
	SceSize offset;
	int ret;

	sceClibMemset(&args, 0, sizeof(args));
	args.arg_size = sizeof(args);
	args.package_type = nodeType;
	args.extension[0] = extension;
	args.extension[1] = bufferSize;
	ret = _sceSblUsGetExtractSpackage(nodeType, requestId, &args);
	if (ret == 0) {
		file = args.addr;
		offset = ((const cf_header *)file)->file_offset;
		package = (const SceSblUsExtractedSpackage *)(file + offset);
		if (bufferSize < offset + sizeof(*package))
			ret = SCE_SBL_US_ERROR_INVALID_ARGUMENT;
		else if (bufferSize < package->chunk.size)
			ret = SCE_SBL_US_ERROR_INVALID_ARGUMENT;
		else
			sceClibMemcpy(buffer, file + offset + sizeof(*package), package->chunk.size);
	}
	return ret;
}

int sceSblUsAllocateBuffer(SceSize size, void **buffer)
{
	return _sceSblUsAllocateBuffer(size, buffer);
}

int sceSblUsReleaseBuffer(void *buffer)
{
	return _sceSblUsReleaseBuffer(buffer);
}

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

int sceSblUsGetSpkgInfo(int packageType, SceSblUsSpkgInfo *info)
{
	return _sceSblUsGetSpkgInfo(packageType, info);
}

int sceSblUsGetUpdateMode(SceUInt8 *mode)
{
	return _sceSblUsGetUpdateMode(mode);
}

int sceSblUsSetUpdateMode(SceUInt8 mode)
{
	return _sceSblUsSetUpdateMode(mode);
}

int sceSblUsPowerControl(int operation, int flags)
{
	return _sceSblUsPowerControl(operation, flags);
}

int sceSblUsInformUpdateStarted(int number1, int number2, const char *str, SceSize len)
{
	return _sceSblUsInformUpdateStarted(number1, number2, str, len);
}

int sceSblUsInformUpdateOngoing(int number1, int number2)
{
	return _sceSblUsInformUpdateOngoing(number1, number2);
}

int sceSblUsInformUpdateFinished(int number, const char *str, SceSize len)
{
	return _sceSblUsInformUpdateFinished(number, str, len);
}

int sceSblUsSetSwInfoInt(const char *name, SceSize nameLength, SceUInt32 value)
{
	return _sceSblUsSetSwInfoInt(name, nameLength, value);
}

int sceSblUsSetSwInfoStr(const char *name, SceSize nameLength, const char *value,
			 SceSize valueLength)
{
	return _sceSblUsSetSwInfoStr(name, nameLength, value, valueLength);
}
