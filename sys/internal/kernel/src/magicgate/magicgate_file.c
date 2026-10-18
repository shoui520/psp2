/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"
#include <psp2kern/io/dirent.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/ssmgr.h>
#include <rtc/rtc.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include <sysmem/suspend.h>

int sceMsifGetMediaId(uint8_t *mediaId);

/* These supplied keys still need their entries identified in the reference. */
extern const MagicGateKey fileIdentityKey;
extern const MagicGateKey fileServiceKey;

MagicGateFileService magicGateFileService = {
	.selectedGroup = -1,
	.mutex = -1,
};

char magicGateFilePath[32] = "tm0:/drm/magicgate/ibdIIISS.dat";

int recoverFileGroups(void)
{
	uint8_t iv[16] __attribute__((aligned(16)));
	SceIoStat stat;
	SceRtcTick tick;
	MagicGateFileGroup *summary;
	int group = 0;
	int emptyGroup = -1;
	int selectedGroup = -1;
	int found;
	int index;
	SceUID fd;

	magicGateFileService.selectedGroup = -1;
	memset(magicGateFileService.groups, 0, sizeof(magicGateFileService.groups));
	SceKernelSuspendForDriver_4DF40893(0);
	do {
		found = 0;
		summary = &magicGateFileService.groups[group];
		index = 0;
		do {
			magicGateFilePath[23] = group % 100 / 10 + '0';
			magicGateFilePath[22] = '0';
			magicGateFilePath[24] = group % 100 % 10 + '0';
			magicGateFilePath[25] = index / 10 + '0';
			magicGateFilePath[26] = index % 10 + '0';
			fd = ksceIoOpen(magicGateFilePath, 0x04000001, 0);
			if (fd < 0) {
				if (fd != (int)0x80010002)
					goto nextGroup;
			} else {
				if (ksceIoRead(fd, &magicGateFileService.record,
					       sizeof(magicGateFileService.record)) < 0)
					goto nextGroup;
				if (ksceIoGetstatByFd(fd, &stat) < 0)
					memset(&stat, 0, sizeof(stat));
				ksceIoClose(fd);
				memset(iv, group, sizeof(iv));
				if (decryptRecordCbc(&magicGateFileService.key,
						     &magicGateFileService.record,
						     &magicGateFileService.record,
						     sizeof(magicGateFileService.record), iv) < 0)
					goto nextGroup;
				if (magicGateFileService.record.zeroTrailer[0] ||
				    magicGateFileService.record.zeroTrailer[1])
					goto nextGroup;
				summary->identity.words[0] = 0;
				summary->identity.words[1] = 0;
				summary->identity.words[2] = 0;
				summary->identity.words[3] = 0;
				summary->creationTick = 0;
				summary->modificationTick = 0;
				if (found) {
					if (SceRtcForDriver_F139E359(&stat.st_ctime, &tick) >= 0 &&
					    tick.tick && tick.tick < summary->creationTick)
						summary->creationTick = tick.tick;
					if (SceRtcForDriver_F139E359(&stat.st_mtime, &tick) >= 0 &&
					    tick.tick && summary->modificationTick < tick.tick)
						summary->modificationTick = tick.tick;
				} else {
					summary->identity = magicGateFileService.record.identity;
					if (SceRtcForDriver_F139E359(&stat.st_ctime,
								    (SceRtcTick *)&summary->creationTick) < 0)
						summary->creationTick = 0;
					if (SceRtcForDriver_F139E359(&stat.st_mtime,
								    (SceRtcTick *)&summary->modificationTick) < 0)
						summary->modificationTick = 0;
					found = 1;
				}
			}
			++index;
		} while (index != 32);
		if (found) {
			if (!memcmp(&summary->identity, &magicGateFileService.identity, 16))
				selectedGroup = group;
		} else if (emptyGroup == -1)
			emptyGroup = group;
nextGroup:
		++group;
	} while (group != 4);
	if (selectedGroup == -1 && emptyGroup != -1) {
		selectedGroup = emptyGroup;
		summary = &magicGateFileService.groups[emptyGroup];
		summary->creationTick = 0;
		summary->modificationTick = 0;
		summary->identity = magicGateFileService.identity;
	}
	if (selectedGroup != -1) {
		magicGateFileService.info.format = 3;
		magicGateFileService.info.size = 16;
		magicGateFileService.info.group = selectedGroup;
	} else
		magicGateFileService.info = (MagicGateFileInfo){ 0 };
	SceKernelSuspendForDriver_2BB92967(0);
	magicGateFileService.selectedGroup = selectedGroup;
	return selectedGroup;
}

int deleteFileGroup(int group)
{
	int fileGroup = (uint8_t)group;
	int index = 0;
	MagicGateFileGroup *summary;

	SceKernelSuspendForDriver_4DF40893(0);
#define REMOVE_RECORD(recordIndex) do { \
	magicGateFilePath[22] = fileGroup / 100 + '0'; \
	magicGateFilePath[23] = fileGroup % 100 / 10 + '0'; \
	magicGateFilePath[24] = fileGroup % 100 % 10 + '0'; \
	magicGateFilePath[25] = (recordIndex) / 10 + '0'; \
	magicGateFilePath[26] = (recordIndex) % 10 + '0'; \
	ksceIoRemove(magicGateFilePath); \
} while (0)
	do {
		REMOVE_RECORD(index);
		REMOVE_RECORD(index + 1);
		REMOVE_RECORD(index + 2);
		REMOVE_RECORD(index + 3);
		index += 4;
	} while (index != 32);
#undef REMOVE_RECORD
	if (group != magicGateFileService.selectedGroup) {
		summary = &magicGateFileService.groups[group];
		summary->identity.words[0] = 0;
		summary->identity.words[1] = 0;
		summary->identity.words[2] = 0;
		summary->identity.words[3] = 0;
		summary->creationTick = 0;
		summary->modificationTick = 0;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return 0;
}

int readFileRecord(MagicGateFileRequest *request)
{
	uint8_t iv[16] __attribute__((aligned(16)));
	unsigned int index = request->index;
	MagicGateFileRecord *record = request->record;
	SceSize size = request->size;
	int group = magicGateFileService.selectedGroup;
	int fileGroup;
	int fileIndex;
	int result;
	SceUID fd;

	if (!(magicGateFileService.flags & 1))
		return 0x80505005;
	if (group < 0)
		return 0x80504004;
	if (!record || index > 32)
		return 0x80501000;
	SceKernelSuspendForDriver_4DF40893(0);
	fileGroup = (uint8_t)group;
	fileIndex = index & 31;
	magicGateFilePath[22] = fileGroup / 100 + '0';
	magicGateFilePath[25] = fileIndex / 10 + '0';
	magicGateFilePath[26] = fileIndex % 10 + '0';
	magicGateFilePath[23] = fileGroup % 100 / 10 + '0';
	magicGateFilePath[24] = fileGroup % 100 % 10 + '0';
	fd = ksceIoOpen(magicGateFilePath, 0x04000001, 0);
	if (fd < 0) {
		result = fd;
		if (result == (int)0x80010002)
			goto emptyRecord;
		goto ioError;
	}
	result = ksceIoRead(fd, &magicGateFileService.record,
			    sizeof(magicGateFileService.record));
	if (result < 0)
		goto ioError;
	ksceIoClose(fd);
	if (result != sizeof(magicGateFileService.record)) {
emptyRecord:
		record->index = 0;
		record->reserved[0] = 0;
		record->reserved[1] = 0;
		record->reserved[2] = 0;
		record->sequence[0] = 0;
		record->sequence[1] = 0;
		record->sequence[2] = 0;
		record->sequence[3] = 0;
		memset(record->payload, 255, size - 8);
		record->index = index;
	} else {
		memset(iv, group, sizeof(iv));
		result = decryptRecordCbc(&magicGateFileService.key,
					  &magicGateFileService.record,
					  &magicGateFileService.record,
					  sizeof(magicGateFileService.record), iv);
		if (result < 0)
			goto unlock;
		if (magicGateFileService.record.zeroTrailer[0] ||
		    magicGateFileService.record.zeroTrailer[1] ||
		    memcmp(&magicGateFileService.record.identity,
			   &magicGateFileService.identity, 16)) {
			result = 0x80501004;
			goto unlock;
		}
		memcpy(record, &magicGateFileService.record, size);
	}
	result = sceKernelIsUserModeThread();
	if (result) {
		result = 0;
		magicGateFileService.flags &= ~2U;
	}
	goto unlock;
ioError:
	if (result == (int)0x80010009)
		result = 0x80501001;
	else if (result == (int)0x80010013)
		result = 0x80505001;
	else
		result &= result >> 31;
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int writeFileRecord(MagicGateFileRequest *request)
{
	uint8_t iv[2][16] __attribute__((aligned(16)));
	SceIoStat stat;
	SceRtcTick tick;
	unsigned int index = request->index;
	MagicGateFileRecord *record = request->record;
	SceSize size = request->size;
	int group = magicGateFileService.selectedGroup;
	unsigned int previousSequence;
	unsigned int sequence;
	int fileGroup;
	int fileIndex;
	int result;
	SceOff position;
	SceUID fd;

	if (!(magicGateFileService.flags & 1))
		return 0x80505005;
	if (group < 0)
		return 0x80504004;
	if (!record || index > 32 || size <= 7)
		return 0x80501000;
	SceKernelSuspendForDriver_4DF40893(0);
	fileGroup = (uint8_t)group;
	fileIndex = index & 31;
	magicGateFilePath[22] = fileGroup / 100 + '0';
	magicGateFilePath[25] = fileIndex / 10 + '0';
	magicGateFilePath[26] = fileIndex % 10 + '0';
	magicGateFilePath[23] = fileGroup % 100 / 10 + '0';
	magicGateFilePath[24] = fileGroup % 100 % 10 + '0';
	fd = ksceIoOpen(magicGateFilePath, 0x04000203, 6);
	if (fd < 0) {
		result = fd;
		goto ioError;
	}
	result = ksceIoRead(fd, &magicGateFileService.record,
			    sizeof(magicGateFileService.record));
	if (result < 0)
		goto ioErrorClose;
	if (result == sizeof(magicGateFileService.record))
		memset(iv[0], group, sizeof(iv[0]));
	if (result != sizeof(magicGateFileService.record) ||
	    decryptRecordCbc(&magicGateFileService.key, &magicGateFileService.record,
			     &magicGateFileService.record,
			     sizeof(magicGateFileService.record), iv[0]) < 0 ||
	    magicGateFileService.record.zeroTrailer[0] ||
	    magicGateFileService.record.zeroTrailer[1] ||
	    memcmp(&magicGateFileService.record.identity,
		   &magicGateFileService.identity, 16)) {
		memset(&magicGateFileService.record, 0, sizeof(magicGateFileService.record));
		sequence = (unsigned int)record->sequence[1] << 16 |
			   (unsigned int)record->sequence[0] << 24 |
			   record->sequence[3] | (unsigned int)record->sequence[2] << 8;
		magicGateFileService.record.index = index;
		previousSequence = sequence - 1;
	} else {
		previousSequence =
			(unsigned int)magicGateFileService.record.sequence[1] << 16 |
			(unsigned int)magicGateFileService.record.sequence[0] << 24 |
			magicGateFileService.record.sequence[3] |
			(unsigned int)magicGateFileService.record.sequence[2] << 8;
		sequence = (unsigned int)record->sequence[0] << 24 |
			   (unsigned int)record->sequence[1] << 16 |
			   record->sequence[3] | (unsigned int)record->sequence[2] << 8;
	}
	if (previousSequence + 1 != sequence) {
		result = 0x80501004;
		goto close;
	}
	memcpy(&magicGateFileService.record, record, size);
	magicGateFileService.record.zeroTrailer[0] = 0;
	magicGateFileService.record.zeroTrailer[1] = 0;
	magicGateFileService.record.identity = magicGateFileService.identity;
	memset(iv[1], group, sizeof(iv[1]));
	result = encryptRecordCbc(&magicGateFileService.key, &magicGateFileService.record,
				  &magicGateFileService.record,
				  sizeof(magicGateFileService.record), iv[1]);
	if (result < 0)
		goto close;
	position = ksceIoLseek(fd, 0, 0);
	if (position < 0) {
		result = (int)position;
		if (!result)
			goto close;
		goto ioErrorClose;
	}
	result = ksceIoWrite(fd, &magicGateFileService.record,
			     sizeof(magicGateFileService.record));
	if (result < 0)
		goto ioErrorClose;
	if (result == sizeof(magicGateFileService.record)) {
		if (ksceIoGetstatByFd(fd, &stat) >= 0 &&
		    SceRtcForDriver_F139E359(&stat.st_mtime, &tick) >= 0)
			magicGateFileService.groups[group].modificationTick = tick.tick;
		result = 0;
		if (!sceKernelIsUserModeThread())
			magicGateFileService.flags |= 2;
	}
	goto close;
ioErrorClose:
	if (result == (int)0x80010009)
		result = 0x80501001;
	else if (result == (int)0x80010013)
		result = 0x80505001;
	else
		result &= result >> 31;
close:
	ksceIoClose(fd);
	goto unlock;
ioError:
	if (result == (int)0x80010009)
		result = 0x80501001;
	else if (result == (int)0x80010013)
		result = 0x80505001;
	else
		result &= result >> 31;
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int SceMagicGateForDriver_1BCE34AD(int handle, unsigned int index,
				 void *record, SceSize size)
{
	MagicGateFileRequest request;

	request.index = index;
	request.handle = handle;
	request.record = record;
	request.size = size;
	return SceThreadmgrForDriver_E54FD746(4096, (int (*)(void *))readFileRecord, &request);
}

int SceMagicGateForDriver_75B445DA(int handle, unsigned int index,
				 const void *record, SceSize size)
{
	MagicGateFileRequest request;

	request.index = index;
	request.handle = handle;
	request.record = (MagicGateFileRecord *)record;
	request.size = size;
	return SceThreadmgrForDriver_E54FD746(4096, (int (*)(void *))writeFileRecord, &request);
}

int SceMagicGateForDriver_3EDD73EB(int handle, int index)
{
	MagicGateFileGroup *summary;
	int group;
	int first;
	int last;
	int remaining;

	(void)handle;
	if ((unsigned int)index + 1 > 33)
		return 0x80501000;
#define REMOVE_GROUP_RECORD(recordIndex) do { \
	int fileIndex = (recordIndex) & 31; \
	magicGateFilePath[22] = '0'; \
	magicGateFilePath[23] = group % 100 / 10 + '0'; \
	magicGateFilePath[24] = group % 100 % 10 + '0'; \
	magicGateFilePath[25] = fileIndex / 10 + '0'; \
	magicGateFilePath[26] = fileIndex % 10 + '0'; \
	ksceIoRemove(magicGateFilePath); \
} while (0)
	for (group = 0; group != 4; ++group) {
		first = index;
		last = index;
		if (index == -1) {
			first = 0;
			last = 31;
		}
		SceKernelSuspendForDriver_4DF40893(0);
		remaining = (last - first) & 1;
		REMOVE_GROUP_RECORD(first);
		++first;
		if (first <= last) {
			if (remaining) {
				REMOVE_GROUP_RECORD(first);
				++first;
			}
			while (first <= last) {
				REMOVE_GROUP_RECORD(first);
				REMOVE_GROUP_RECORD(first + 1);
				first += 2;
			}
		}
		if (group != magicGateFileService.selectedGroup) {
			summary = &magicGateFileService.groups[group];
			summary->identity.words[0] = 0;
			summary->identity.words[1] = 0;
			summary->identity.words[2] = 0;
			summary->identity.words[3] = 0;
			summary->creationTick = 0;
			summary->modificationTick = 0;
		}
		SceKernelSuspendForDriver_2BB92967(0);
	}
#undef REMOVE_GROUP_RECORD
	recoverFileGroups();
	magicGateFileService.flags |= 2;
	return 0;
}

int SceMagicGateForDriver_8E87BA88(int handle, unsigned int group)
{
	int result;

	(void)handle;
	if (group == (unsigned int)-1) {
		deleteFileGroup(0);
		deleteFileGroup(1);
		deleteFileGroup(2);
		deleteFileGroup(3);
		recoverFileGroups();
		goto changed;
	}
	if (group > 4)
		return 0x80501000;
	result = deleteFileGroup(group);
	if (result < 0)
		return result;
changed:
	magicGateFileService.flags |= 2;
	return 0;
}

int SceMagicGateForDriver_7B052A10(void)
{
	return 0;
}

void terminateFileService(void)
{
	SceUID mutex = magicGateFileService.mutex;

	magicGateFileService.flags = 0;
	if (mutex > 0) {
		magicGateFileService.mutex = -1;
		sceKernelUnlockMutex(mutex, 1);
		sceKernelDeleteMutex(mutex);
	}
}

int SceMagicGateForDriver_30DF2E82(void)
{
	struct {
		MagicGateFileIdentity identity;
		MagicGateFileInfo info;
		MagicGateKey identityKey;
		MagicGateKey serviceKey;
		MagicGateFileInfo keyInput;
	} workspace __attribute__((aligned(16)));
	SceIoStat stat;
	SceUID mutex;
	int result;

	result = SceMagicGateForDriver_A49B9D70();
	if (result < 0)
		return result;
	if (magicGateFileService.references > 0) {
		magicGateFileService.references++;
		SceMagicGateForDriver_492EE950();
		return 0;
	}
	if (magicGateFileService.mutex <= 0) {
		result = sceKernelCreateMutex("SceMagicGateEmDrv", 0, 1, NULL);
		if (result < 0) {
			SceMagicGateForDriver_492EE950();
			return result;
		}
		magicGateFileService.mutex = result;
	}
	SceMagicGateForDriver_492EE950();
	result = ksceIoDopen("tm0:");
	if (result < 0) {
		if (result == (int)0x80010002)
			result = 0x80505001;
		goto failed;
	}
	ksceIoDclose(result);
	result = ksceIoGetstat("tm0:/drm/magicgate", &stat);
	if (result < 0) {
		if (result != (int)0x80010002)
			goto ioFailed;
		goto createDirectory;
	}
	if ((stat.st_mode & 0xf000) != 0x1000) {
		result = ksceIoRemove("tm0:/drm/magicgate");
		if (result < 0)
			goto ioFailed;
createDirectory:
		ksceIoMkdir("tm0:/drm", 6);
		result = ksceIoMkdir("tm0:/drm/magicgate", 6);
		if (result < 0)
			goto ioFailed;
	}
	workspace.identityKey = fileIdentityKey;
	result = sceMsifGetMediaId((uint8_t *)&workspace.identity);
	if (result < 0)
		goto failed;
	result = ksceSblAimgrGetOpenPsId((SceOpenPsId *)&workspace.info);
	if (result < 0)
		goto failed;
	result = encryptRecordEcb(&workspace.identityKey, &workspace.info,
				  &workspace.info, sizeof(workspace.info));
	if (result < 0)
		goto failed;
	workspace.info.format = 3;
	workspace.info.size = 16;
	workspace.info.group = 0;
	result = encryptRecordEcb(&workspace.identityKey, &workspace.identity,
				  &workspace.identity, sizeof(workspace.identity));
	if (result < 0)
		goto failed;
	if (!workspace.identity.halves[0] && !workspace.identity.halves[1])
		workspace.identity.halves[0] = 0x80;
	magicGateFileService.identity = workspace.identity;
	workspace.keyInput = workspace.info;
	magicGateFileService.info = workspace.info;
	workspace.keyInput.format = 3;
	workspace.keyInput.size = 16;
	workspace.keyInput.group = 0;
	result = encryptRecordEcb(&fileServiceKey, &workspace.keyInput,
				  &workspace.serviceKey, sizeof(workspace.keyInput));
	if (result < 0)
		goto failed;
	magicGateFileService.key = workspace.serviceKey;
	recoverFileGroups();
	magicGateFileService.references = 1;
	magicGateFileService.flags = 1;
	sceKernelUnlockMutex(magicGateFileService.mutex, 1);
	return 0;

ioFailed:
	if (result == (int)0x80010009)
		result = 0x80501001;
	else if (result == (int)0x80010013)
		result = 0x80505001;
	else
		result &= result >> 31;
failed:
	mutex = magicGateFileService.mutex;
	magicGateFileService.flags = 0;
	if (mutex > 0) {
		magicGateFileService.mutex = -1;
		sceKernelUnlockMutex(mutex, 1);
		sceKernelDeleteMutex(mutex);
	}
	return result;
}

int SceMagicGateForDriver_E62B8A67(int handle, MagicGateFileGroup *info, int group)
{
	int result;

	(void)handle;
	if (!(magicGateFileService.flags & 1))
		return 0x80505005;
	if (group == -1) {
		group = magicGateFileService.selectedGroup;
		if (group < 0) {
			memset(info, 0, sizeof(*info));
			info->identity = magicGateFileService.identity;
			return 0;
		}
		result = group + 1;
	} else {
		if (group > 3)
			return 0x80501000;
		result = 4;
	}
	*info = magicGateFileService.groups[group];
	return result;
}

int SceMagicGateForDriver_8E2E577E(void)
{
	SceUID mutex;
	int result;

	result = sceKernelLockMutex(magicGateFileService.mutex, 1, NULL);
	if (result == (int)0x80024504)
		return 0x80501001;
	if (result < 0)
		return result;
	if (--magicGateFileService.references <= 0) {
		mutex = magicGateFileService.mutex;
		magicGateFileService.flags = 0;
		if (mutex > 0) {
			magicGateFileService.mutex = -1;
			sceKernelUnlockMutex(mutex, 1);
			sceKernelDeleteMutex(mutex);
		}
		return 0;
	}
	sceKernelUnlockMutex(magicGateFileService.mutex, 1);
	return 0;
}

int SceMagicGateForDriver_D819D5C6(int handle, MagicGateFileInfo *info)
{
	(void)handle;
	if (!(magicGateFileService.flags & 1))
		return 0x80505005;
	if (magicGateFileService.selectedGroup < 0)
		return 0x80504004;
	if ((magicGateFileService.flags & 2) && sceKernelIsUserModeThread()) {
		magicGateFileService.flags &= ~2U;
		return 0x80505002;
	}
	*info = magicGateFileService.info;
	return 0;
}
