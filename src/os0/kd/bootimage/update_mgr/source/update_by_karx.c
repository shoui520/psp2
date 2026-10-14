#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/aimgr.h>
#include <sce/debug.h>
#include <sce/certified_file.h>
#include "update_internal.h"
#include "spkg.h"

SCE_DBGINFO_DEFINE(static, updateOpenErrorInfo, 160, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updateStartedInfo, 141, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updateFileMissingInfo, 154, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updateFileOpenedInfo, 177, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updatePupErrorInfo, 186, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updatePupVerifiedInfo, 193, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updateMbrStartedInfo, 201, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updateSegmentInfo, 218, "UpdateMgrdoUpdate");
SCE_DBGINFO_DEFINE(static, updateFinishedInfo, 404, "UpdateMgrdoUpdate");

int SceSysrootForDriver_47724459(int value);

int UpdateMgrWritePupSegment(SceUID heapId, SceUID fd,
			     const PupCachedHeader *pup, const SceMbr *mbr,
			     uint8_t partition, uint64_t segmentId)
{
	const PupSegment *segment;
	const PupSegment *next;
	uint64_t index, segmentCount;
	int64_t remaining;
	char *buffer, *cursor;
	int count, chunk, result;

	if ((heapId <= 0) | ((unsigned int)fd >> 31))
		return (int)0x800f0216;
	if (!pup || !mbr)
		return (int)0x800f0216;
	segmentCount = pup->header.m_segment_num;
	if (!segmentCount)
		return (int)0x800f0202;
	segment = pup->segments;
	if (segment->segment_id != segmentId) {
		next = segment + 1;
		index = 0;
		do {
			segment = next;
			if (++index == segmentCount)
				return (int)0x800f0202;
			next++;
		} while (segment->segment_id != segmentId);
	}
	if (segment->size > 0x18000000)
		return (int)0x800f0224;
	buffer = ksceKernelAllocHeapMemory(heapId, (SceSize)segment->size);
	if (!buffer)
		return (int)0x800f020c;
	remaining = segment->size;
	if (ksceIoLseek(fd, segment->offset, SCE_SEEK_SET) < 0)
		goto readError;
	cursor = buffer;
	while (remaining > 0) {
		chunk = remaining > 0x40000 ? 0x40000 : remaining;
		count = ksceIoRead(fd, cursor, chunk);
		cursor += count;
		if (count < 0 || count != chunk)
			goto readError;
		remaining -= (unsigned int)count;
	}
	result = UpdateUtilWritePartition(buffer, segment->size, 0,
					mbr, partition, 0, NULL);
	goto finish;
readError:
	result = (int)0x800f0205;
finish:
	ksceKernelFreeHeapMemory(heapId, buffer);
	return result;
}

int UpdateMgrdoUpdate(SceUID heapId, const char *path)
{
	PupCachedHeader pup;
	cf_header certifiedHeader;
	SpkgPackageInfo packageInfo;
	const PupSegment *segment;
	const SceMbr *mbr;
	SceUID fd;
	int result, stage, updated, nodeType;
	unsigned int index;

	ksceKernelPrintfWithInfo(0, &updateStartedInfo,
		"[I] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, 0, 0);
	memset(&pup, 0, sizeof(pup));
	fd = ksceIoOpen(path, 0x04000001, 0);
	if (fd < 0) {
		if (fd == (int)0x80010002)
			ksceKernelPrintfWithInfo(0, &updateFileMissingInfo,
				"[I] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, 1, fd);
		else
			ksceKernelPrintfWithInfo(0, &updateOpenErrorInfo,
				"[E] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, 1, fd);
		return (int)0x800f0202;
	}
	if ((ksceSblAimgrIsTool() || ksceSblAimgrIsTest()) && updateCpupConnected)
		SceSysrootForDriver_47724459(0);
	ksceKernelPrintfWithInfo(0, &updateFileOpenedInfo,
		"[I] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, 1, 0);
	if (heapId <= 0 || !path) {
		result = (int)0x800f0216;
		goto pupError;
	}
	result = sceSblUsVerifyPup(path);
	if (result)
		goto pupError;
	result = PupUtilPupGetCachedHeader(&pup);
	if (result)
		goto pupError;
	ksceKernelPrintfWithInfo(0, &updatePupVerifiedInfo,
		"[I] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, 2, 0);
	ksceKernelPrintfWithInfo(0, &updateMbrStartedInfo,
		"[I] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, 3, 0);
	stage = 4;
	mbr = SpkgUtilGetMbr();
	if (!mbr)
		goto readError;
	updated = 0;
	for (index = 0; index < pup.header.m_segment_num; index++) {
		segment = &pup.segments[index];
		ksceKernelPrintfWithInfo(0, &updateSegmentInfo,
			"[I] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n",
			11, 0, (unsigned int)segment->segment_id, 0);
		switch (segment->segment_id) {
		case 0x2002:
			result = UpdateMgrWritePupSegment(heapId, fd, &pup, mbr, 5, 0x2002);
			continue;
		case 0x2003:
			result = UpdateMgrWritePupSegment(heapId, fd, &pup, mbr, 6, 0x2003);
			continue;
		case 0x2004:
			result = UpdateMgrWritePupSegment(heapId, fd, &pup, mbr, 7, 0x2004);
			continue;
		case 0x2007:
			result = UpdateMgrWritePupSegment(heapId, fd, &pup, mbr, 11, 0x2007);
			continue;
		}
		if (segment->segment_id - 0x300 >= 0x100) {
			if (segment->segment_id - 0x100 >= 0x200 &&
			    segment->segment_id - 0x400 >= 0x100 &&
			    segment->segment_id - 0x2000 >= 0x100) {
				result = (int)0x800f0202;
				goto finished;
			}
			result = 0;
			continue;
		}
		certifiedHeader = (cf_header){0};
		memset(&packageInfo, 0, sizeof(packageInfo));
		if (ksceIoLseek(fd, segment->offset, SCE_SEEK_SET) < 0 ||
		    ksceIoRead(fd, &certifiedHeader, sizeof(certifiedHeader)) != sizeof(certifiedHeader))
			goto readError;
		if ((certifiedHeader.magic & 0x00ffffff) != 0x00454353)
			goto formatError;
		if (ksceIoLseek(fd, segment->offset + certifiedHeader.file_offset, SCE_SEEK_SET) < 0 ||
		    ksceIoRead(fd, &packageInfo, sizeof(packageInfo)) != sizeof(packageInfo))
			goto readError;
		if (packageInfo.version != 4)
			goto formatError;
		switch (packageInfo.type) {
		case 9:
			nodeType = 2;
			break;
		case 1:
			nodeType = 3;
			break;
		case 10:
			nodeType = 4;
			break;
		default:
			nodeType = 0;
			break;
		}
		result = SpkgUtilUpdateSpackageSync(fd, mbr, nodeType,
			packageInfo.type, segment, 5);
		if (!result)
			updated++;
	}
	if (updated)
		result = 0;
	SpkgUtilSetUpdateMode(255);
finished:
	ksceKernelPrintfWithInfo(0, &updateFinishedInfo,
		"[I] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, stage, result);
	ksceIoClose(fd);
	if (pup.allocation)
		ksceKernelFreeHeapMemory(heapId, pup.allocation);
	PupUtilPupFreeCachedHeader();
	return result;

pupError:
	ksceKernelPrintfWithInfo(0, &updatePupErrorInfo,
		"[E] 0x%08x : 0x%08x : 0x%04x : 0x%08x\n", 11, 0, 2, result);
	stage = 3;
	goto finished;
readError:
	result = (int)0x800f0205;
	goto finished;
formatError:
	result = (int)0x800f0224;
	goto finished;
}
