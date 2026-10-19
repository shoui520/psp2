/* System-data files and their PlayReady information and payload. */

#include <scetypes.h>
#include <kernel/types.h>
#include <sysmem/uid_guid.h>
#include <iofilemgr/iofilemgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include <regmgr/regmgr.h>
#include <modulemgr/self.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <acmgr/acmgr.h>
#include "mount.h"

typedef struct AppmgrPlayReadyFileHeader {
	char magic[4];
	SceUInt32 version;
	SceUInt64 information;
	SceSize payloadSize;
	SceUInt8 reserved[44];
} AppmgrPlayReadyFileHeader;

typedef struct AppmgrMarlinDeviceKeys {
	unsigned char deviceKey[3][1376];
} AppmgrMarlinDeviceKeys;

_Static_assert(sizeof(AppmgrMarlinDeviceKeys) == 4128, "Marlin device keys");

_Static_assert(sizeof(AppmgrPlayReadyFileHeader) == 64, "PlayReady file header");
_Static_assert(offsetof(AppmgrPlayReadyFileHeader, magic) == 0, "PlayReady magic");
_Static_assert(offsetof(AppmgrPlayReadyFileHeader, version) == 4, "PlayReady version");
_Static_assert(offsetof(AppmgrPlayReadyFileHeader, information) == 8,
	       "PlayReady information");
_Static_assert(offsetof(AppmgrPlayReadyFileHeader, payloadSize) == 16,
	       "PlayReady payload size");
_Static_assert(offsetof(AppmgrPlayReadyFileHeader, reserved) == 20,
	       "PlayReady reserved bytes");

extern SceUID g_appmgrCoreHeap;
SceUID appmgrOpenTemporaryCopy(const char *path, int flags, void *copyBuffer,
			      SceSize capacity);
int appmgrReplaceFile(const char *temporaryPath, const char *path);

SceUID g_appmgrSaveDataHeap = -1;
SceUID g_appmgrSaveDataMutex = -1;

static const unsigned char encodedPlayReadyFile[24] = {
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0',
	(unsigned char)~':', (unsigned char)~'p', (unsigned char)~'l',
	(unsigned char)~'a', (unsigned char)~'y', (unsigned char)~'r',
	(unsigned char)~'e', (unsigned char)~'a', (unsigned char)~'d',
	(unsigned char)~'y', (unsigned char)~'/', (unsigned char)~'p',
	(unsigned char)~'l', (unsigned char)~'.', (unsigned char)~'d',
	(unsigned char)~'a', (unsigned char)~'t', 0,
};

static const unsigned char encodedPlayReadyDirectory[16] = {
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0',
	(unsigned char)~':', (unsigned char)~'p', (unsigned char)~'l',
	(unsigned char)~'a', (unsigned char)~'y', (unsigned char)~'r',
	(unsigned char)~'e', (unsigned char)~'a', (unsigned char)~'d',
	(unsigned char)~'y', 0,
};

void *appmgrAllocSaveDataHeapMemory(SceSize size)
{
	SceAllocOpt opt;

	opt.mmapKind = 0;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	opt.size = sizeof(opt);
	opt.align = 32;
	return SceSysmemForDriver_49D4DD9B(g_appmgrSaveDataHeap, size, &opt);
}

int appmgrFreeSaveDataHeapMemory(void *memory)
{
	return sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, memory);
}

int appmgrReadPlayReadySystemData(SceUInt32 location, void *information,
				void *userData, SceSize dataSize)
{
	void *kernelPage = NULL;
	SceSize kernelSize = 0;
	SceUInt32 kernelOffset = 0;
	AppmgrPlayReadyFileHeader header;
	SceIoStat stat;
	char path[32];
	SceUInt32 syscallState;
	SceUID fd, mapping;
	unsigned char *payload;
	int permission, result;

	syscallState = sceSyscallEnter();
	permission = sceKernelSetThreadAccessLevel(128);
	__builtin_memset(path, 0, sizeof(path));
	memset(&header, 0, sizeof(header));
	if (!SceSblACMgrForDriver_AE1AF154(0)) {
		result = (int)0x80800009;
		goto leave;
	}
	if (location == 1) {
		if (information == NULL) {
			result = (int)0x80801001;
			goto leave;
		}
	} else {
		if (location != 2) {
			result = (int)0x80800001;
			goto leave;
		}
		if (userData == NULL) {
			result = (int)0x80801001;
			goto leave;
		}
		if (dataSize < 16384) {
			result = (int)0x80800001;
			goto leave;
		}
	}
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto leave;
	result = appmgrDecodeString(encodedPlayReadyFile, path, sizeof(path));
	if (result < 0)
		goto unlock;
	if (sceIoGetstat(path, &stat) < 0) {
		result = (int)0x808010f3;
		goto unlock;
	}
	fd = sceIoOpen(path, 1, 0);
	result = fd;
	if (fd < 0)
		goto unlock;
	if (sceIoRead(fd, &header, sizeof(header)) != sizeof(header)) {
		result = (int)0x808010f1;
		goto closeFile;
	}
	if (strncmp(header.magic, "SDPR", sizeof(header.magic)) != 0 ||
	    header.version != 1 || header.payloadSize > 16384 ||
	    (SceUInt64)(header.payloadSize + sizeof(header)) != (SceUInt64)stat.st_size) {
		result = (int)0x808010f2;
		goto closeFile;
	}
	if (location == 1) {
		sceIoClose(fd);
		if (header.information == 0) {
			result = (int)0x808010f3;
		} else {
			*(SceUInt64 *)information = header.information;
			result = 0;
		}
		goto unlock;
	}
	if (header.payloadSize == 0) {
		result = (int)0x808010f3;
		goto closeFile;
	}
	result = sceKernelIsAccessibleRange(48, userData, 16384);
	if (result < 0)
		goto closeFile;
	mapping = SceSysmemForDriver_278BC201("SceAppMgrGetSystemData", 2, userData, 16384,
				    &kernelPage, &kernelSize, &kernelOffset);
	result = mapping;
	if (mapping < 0)
		goto closeFile;
	payload = (unsigned char *)kernelPage + kernelOffset;
	memset(payload, 0, 16384);
	result = sceIoRead(fd, payload, header.payloadSize);
	if (header.payloadSize == (SceUInt32)result) {
		result = header.payloadSize;
		sceKernelUserUnmap(mapping, userData, 16384);
	} else {
		memset(payload, 0, 16384);
		sceKernelUserUnmap(mapping, userData, 16384);
		result = (int)0x808010f0;
	}
closeFile:
	sceIoClose(fd);
unlock:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
leave:
	sceKernelSetThreadAccessLevel(permission);
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrWritePlayReadySystemData(SceUInt32 location, const void *information,
				 const void *userData, SceSize dataSize)
{
	void *kernelPage = NULL;
	SceSize kernelSize = 0;
	SceUInt32 kernelOffset = 0;
	AppmgrPlayReadyFileHeader header;
	SceIoStat stat;
	char path[32];
	char *scratch;
	SceUInt32 syscallState;
	SceUID fd, mapping;
	int permission, result, transferred;

	syscallState = sceSyscallEnter();
	permission = sceKernelSetThreadAccessLevel(128);
	__builtin_memset(path, 0, sizeof(path));
	memset(&header, 0, sizeof(header));
	if (!SceSblACMgrForDriver_AE1AF154(0)) {
		result = (int)0x80800009;
		goto leave;
	}
	if (location == 1) {
		if (information == NULL) {
			result = (int)0x80801001;
			goto leave;
		}
	} else {
		if (location != 2) {
			result = (int)0x80800001;
			goto leave;
		}
		if (userData == NULL) {
			result = (int)0x80801001;
			goto leave;
		}
		if (dataSize > 16384) {
			result = (int)0x80800001;
			goto leave;
		}
	}
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto leave;
	result = appmgrDecodeString(encodedPlayReadyDirectory, path, sizeof(path));
	if (result < 0)
		goto unlock;
	sceIoMkdir(path, 6);
	result = appmgrDecodeString(encodedPlayReadyFile, path, sizeof(path));
	if (result < 0)
		goto unlock;
	result = sceIoGetstat(path, &stat);
	if (result == 0) {
		fd = sceIoOpen(path, 1, 0);
		result = fd;
		if (fd < 0)
			goto unlock;
		transferred = sceIoRead(fd, &header, sizeof(header));
		sceIoClose(fd);
		if (transferred != sizeof(header)) {
			result = (int)0x808010f1;
			goto unlock;
		}
	} else if (result == (int)0x80010002) {
		memset(&header, 0, sizeof(header));
		header.magic[0] = 'S';
		header.magic[1] = 'D';
		header.magic[2] = 'P';
		header.magic[3] = 'R';
		header.version = 1;
		fd = sceIoOpen(path, 0x603, 6);
		result = fd;
		if (fd < 0)
			goto unlock;
		if (sceIoWrite(fd, &header, sizeof(header)) != sizeof(header)) {
			result = (int)0x808010f4;
			sceIoClose(fd);
			goto unlock;
		}
		result = SceIofilemgrForDriver_462F059B(fd, &stat);
		sceIoClose(fd);
		if (result < 0)
			goto unlock;
	} else if (result < 0) {
		goto unlock;
	}
	if (strncmp(header.magic, "SDPR", sizeof(header.magic)) != 0 ||
	    header.version != 1 || header.payloadSize > 16384 ||
	    (SceUInt64)(header.payloadSize + sizeof(header)) != (SceUInt64)stat.st_size) {
		result = (int)0x808010f2;
		goto unlock;
	}
	scratch = appmgrAllocSaveDataHeapMemory(2048);
	if (scratch == NULL) {
		result = (int)0x80801006;
		goto unlock;
	}
	fd = appmgrOpenTemporaryCopy(path, location == 1 ? 3 : 0x403,
				     scratch, 2048);
	result = fd;
	if (fd < 0)
		goto unlockAndFree;
	if (location == 1) {
		header.information = *(const SceUInt64 *)information;
		transferred = sceIoWrite(fd, &header, sizeof(header));
		sceIoClose(fd);
		if (transferred != sizeof(header)) {
			result = (int)0x808010f4;
			goto unlockAndFree;
		}
	} else {
		header.payloadSize = dataSize;
		if (sceIoWrite(fd, &header, sizeof(header)) != sizeof(header)) {
			result = (int)0x808010f4;
			sceIoClose(fd);
			goto unlockAndFree;
		}
		if (dataSize == 0) {
			sceIoClose(fd);
		} else {
			result = sceKernelIsAccessibleRange(48, userData, dataSize);
			if (result < 0) {
				sceIoClose(fd);
				goto unlockAndFree;
			}
			mapping = SceSysmemForDriver_278BC201("SceAppMgrSetSystemData", 1,
					userData, dataSize, &kernelPage,
					&kernelSize, &kernelOffset);
			result = mapping;
			if (mapping < 0) {
				sceIoClose(fd);
				goto unlockAndFree;
			}
			transferred = sceIoWrite(fd,
					(char *)kernelPage + kernelOffset, dataSize);
			sceIoClose(fd);
			sceKernelUserUnmap(mapping, userData, dataSize);
			if ((SceUInt32)transferred != dataSize) {
				result = (int)0x808010f4;
				goto unlockAndFree;
			}
		}
	}
	snprintf(scratch, 2048, "%s%s", path, "_");
	result = appmgrReplaceFile(scratch, path);
	if (result >= 0)
		result = location == 1 ? 0 : dataSize;
unlockAndFree:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
	appmgrFreeSaveDataHeapMemory(scratch);
	goto leave;
unlock:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
leave:
	sceKernelSetThreadAccessLevel(permission);
	sceSyscallLeave(syscallState);
	return result;
}
int appmgrSetSystemDataFile(SceUInt32 location, const void *userData,
			   SceSize dataSize)
{
	AppmgrMarlinDeviceKeys *keys;
	SceUInt32 syscallState;
	int permission, result;

	syscallState = sceSyscallEnter();
	permission = sceKernelSetThreadAccessLevel(128);
	if (location != 3) {
		result = (int)0x80800001;
		goto leave;
	}
	if (!SceSblACMgrForDriver_A92CD636(0)) {
		result = (int)0x80800009;
		goto leave;
	}
	if (dataSize != 4096 || userData == NULL) {
		result = (int)0x80800001;
		goto leave;
	}
	keys = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, sizeof(*keys));
	if (keys == NULL) {
		result = (int)0x80801006;
		goto leave;
	}
	memset(keys, 0, sizeof(*keys));
	result = sceKernelCopyFromUser(keys, userData, 4096);
	if (result < 0)
		goto freeKeys;
	result = sceRegMgrSetKeyInt("/CONFIG/VIDEOSERVICE/MARLINIPTV",
				 "dirty_flag", 1);
	if (result < 0)
		goto freeKeys;
	result = sceRegMgrSetKeyBin("/CONFIG/VIDEOSERVICE/MARLINIPTV/01",
				 "device_key", keys->deviceKey[0],
				 sizeof(keys->deviceKey[0]));
	if (result < 0)
		goto freeKeys;
	result = sceRegMgrSetKeyBin("/CONFIG/VIDEOSERVICE/MARLINIPTV/02",
				 "device_key", keys->deviceKey[1],
				 sizeof(keys->deviceKey[1]));
	if (result < 0)
		goto freeKeys;
	result = sceRegMgrSetKeyBin("/CONFIG/VIDEOSERVICE/MARLINIPTV/03",
				 "device_key", keys->deviceKey[2],
				 sizeof(keys->deviceKey[2]));
	if (result < 0)
		goto freeKeys;
	result = sceRegMgrSetKeyInt("/CONFIG/VIDEOSERVICE/MARLINIPTV",
				 "dirty_flag", 0);
freeKeys:
	sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, keys);
leave:
	sceKernelSetThreadAccessLevel(permission);
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrReadSelfControlInfo(const char *path, void *controlInfo)
{
	SceSelfHeader header;
	SceAllocOpt opt;
	SceUInt64 extension, size, offset, selectedOffset;
	SceInt64 seekResult;
	unsigned char *records;
	const SceSelfControlInfo *record;
	SceUID fd;
	int result, cleanupResult;

	if (controlInfo == NULL || path == NULL)
		return (int)0x80800001;
	fd = sceIoOpen(path, 1, 0);
	if (fd < 0)
		return fd;
	memset(&header, 0, sizeof(header));
	result = sceIoRead(fd, &header, sizeof(header));
	if (result < 0)
		goto closeFile;
	if (result != sizeof(header)) {
		result = (int)0x8080001b;
		goto closeFile;
	}
	__builtin_memcpy(&extension, header.cfHeader.versionExtension,
			 sizeof(extension));
	if (header.cfHeader.magic != 0x454353 || header.cfHeader.version != 3 ||
	    (header.cfHeader.attribute & 0x40) == 0 || header.cfHeader.category != 1 ||
	    (header.cfHeader.ext_header_size & 15) != 0 ||
	    (header.cfHeader.file_offset & 15) != 0 ||
	    header.cfHeader.file_offset > 4096 || extension != 0 ||
	    header.selfType != 4 || (header.controlInfoOffset & 15) != 0 ||
	    header.cfHeader.file_offset < header.controlInfoOffset ||
	    header.cfHeader.ext_header_size < header.controlInfoSize ||
	    header.cfHeader.file_offset < header.controlInfoSize + header.controlInfoOffset) {
		result = (int)0x8080001a;
		goto closeFile;
	}
	size = header.controlInfoSize;
	opt.size = sizeof(opt);
	opt.align = 32;
	records = SceSysmemForDriver_49D4DD9B(g_appmgrCoreHeap, size, &opt);
	if (records == NULL) {
		result = (int)0x80801006;
		goto closeFile;
	}
	seekResult = sceIoLseek(fd, header.controlInfoOffset, 0);
	if (seekResult < 0) {
		result = seekResult;
		if (result < 0)
			goto freeRecords;
	} else {
		result = sceIoRead(fd, records, size);
		if (result < 0)
			goto freeRecords;
		if ((SceUInt64)(SceInt64)result != size) {
			result = (int)0x8080001b;
			goto freeRecords;
		}
	}
	result = (int)0x8080001a;
	if (size == 0)
		goto freeRecords;
	selectedOffset = 0;
	offset = 0;
	do {
		record = (const SceSelfControlInfo *)(records + offset);
		if (record->type == 6) {
			selectedOffset += offset + sizeof(*record);
			offset += sizeof(*record) + 256;
		} else if (record->type == 5) {
			offset += sizeof(*record) + 256;
		} else {
			if (record->type != 7 && record->type != 4)
				goto freeRecords;
			offset += sizeof(*record) + 64;
		}
		if (size < offset)
			goto freeRecords;
		if (record->next == 0)
			break;
	} while (offset < size);
	if (size == offset && selectedOffset != 0) {
		memcpy(controlInfo, records + selectedOffset, 256);
		result = 0;
	}
freeRecords:
	cleanupResult = sceKernelFreeHeapMemory(g_appmgrCoreHeap, records);
	if (cleanupResult < 0)
		return cleanupResult;
closeFile:
	cleanupResult = sceIoClose(fd);
	return cleanupResult < 0 ? cleanupResult : result;
}

SceUID appmgrOpenTemporaryCopy(const char *path, int flags, void *copyBuffer,
			      SceSize capacity)
{
	SceIoStat stat;
	SceInt64 copied;
	char *temporaryPath;
	SceUID destination, source;
	int result, received, written;

	if (path == NULL || strnlen(path, 291) > 290)
		return (int)0x80800001;
	temporaryPath = appmgrAllocSaveDataHeapMemory(292);
	if (temporaryPath == NULL)
		return (int)0x80801006;
	snprintf(temporaryPath, 292, "%s%s", path, "_");
	destination = sceIoOpen(temporaryPath, 0x603, 6);
	result = destination;
	if (destination < 0)
		goto freePath;
	if (copyBuffer == NULL)
		goto reopen;
	source = sceIoOpen(path, 1, 0);
	result = source;
	if (source == (int)0x80010002) {
		result = 0;
		goto freePath;
	}
	if (source < 0)
		goto freePath;
	if (capacity > 32768) {
		result = (int)0x80800001;
		goto copyFailed;
	}
	result = SceIofilemgrForDriver_462F059B(source, &stat);
	if (result < 0)
		goto copyFailed;
	copied = 0;
	while (copied < stat.st_size) {
		received = sceIoRead(source, copyBuffer, capacity);
		result = received;
		if (received < 0)
			goto copyFailed;
		written = sceIoWrite(destination, copyBuffer, received);
		result = written;
		if (written < 0)
			goto copyFailed;
		if (written != received) {
			result = (int)0x808000a4;
			goto copyFailed;
		}
		copied += received;
	}
	if (copied != stat.st_size) {
		result = (int)0x808000a4;
		goto copyFailed;
	}
	sceIoClose(source);
reopen:
	sceIoClose(destination);
	destination = sceIoOpen(temporaryPath, flags, 6);
	result = destination < 0 ? destination : 0;
freePath:
	appmgrFreeSaveDataHeapMemory(temporaryPath);
	if (result == 0)
		return destination;
	if (destination >= 0)
		sceIoClose(destination);
	return result;
copyFailed:
	appmgrFreeSaveDataHeapMemory(temporaryPath);
	sceIoClose(source);
	if (result == 0)
		return destination;
	if (destination >= 0)
		sceIoClose(destination);
	return result;
}

int appmgrReplaceFile(const char *temporaryPath, const char *path)
{
	SceIoStat stat;
	char *backupPath;
	int result;

	if (temporaryPath == NULL || strnlen(temporaryPath, 292) >= 292 ||
	    path == NULL || strnlen(path, 291) > 290)
		return (int)0x80800001;
	result = sceIoGetstat(temporaryPath, &stat);
	if (result < 0)
		return result;
	backupPath = appmgrAllocSaveDataHeapMemory(292);
	if (backupPath == NULL)
		return (int)0x80801006;
	snprintf(backupPath, 292, "%s%s", path, "__");
	result = sceIoRemove(backupPath);
	if (result == 0 || result == (int)0x80010002) {
		result = sceIoRename(path, backupPath);
		if (result == 0 || result == (int)0x80010002) {
			result = sceIoRename(temporaryPath, path);
			if (result < 0)
				sceIoRename(backupPath, path);
			else
				result = sceIoRemove(backupPath);
		}
	}
	appmgrFreeSaveDataHeapMemory(backupPath);
	return result;
}

