/*
 * SpkgGroupDiff (Sony's spkg_group_diff.cpp): differential system package group for os0: and
 * vs0:. Extracts the diff manifest and TAR chunks, builds a new FAT image in memory from the
 * existing partition and updated files, writes it to the block device, and applies the SPKG.
 * The class and method names are ours.
 */
#ifndef UPDATER_CORE_SPKG_GROUP_DIFF_H
#define UPDATER_CORE_SPKG_GROUP_DIFF_H

#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include "memory_fat_fs.h"
#include "spkg_group.h"
#include "tar_archive.h"

/* 2392 bytes (0x958) */
class SpkgGroupDiff : public SpkgGroup {
public:
	SpkgGroupDiff();
	virtual ~SpkgGroupDiff();

	virtual void EstimateTime(SceUpdaterEstimateTime *estimateTime);
	virtual int EnqueueUpdate(SceUID pupFd);
	virtual void SetDiffManifestSpkg(const SpkgEntry *entry);
	virtual int UpdateSpkg(SpkgOperationElement *element);
	virtual void OnInspectSkipped(SpkgEntry *entry);

private:
	int freeImageMemBlock(void);
	void freeTarBuffer(void);
	int unmountTargetPartition(void);
	int convertErrorCodeUpdateResult(int packageType, int error);
#ifdef UPDATER_CORE_2014
	__attribute__((noinline)) int convertErrorCodeUpdateStatus(int packageType, int error);
#else
	int convertErrorCodeUpdateStatus(int packageType, int error);
#endif
	int convertErrorCodeUpdateRequest(int packageType, int error);
	int commitHeaderSpkg(SceUID pupFd);
	int flushImageAndCommit(SceUID pupFd);
#ifdef UPDATER_CORE_2014
	__attribute__((noinline)) int convertErrorCodeGetExtract(int packageType, int error);
#else
	int convertErrorCodeGetExtract(int packageType, int error);
#endif
	int convertErrorCodeExtractResult(int packageType, int error);
#ifdef UPDATER_CORE_2014
	__attribute__((noinline)) int convertErrorCodeExtractStatus(int packageType, int error);
#else
	int convertErrorCodeExtractStatus(int packageType, int error);
#endif
	int convertErrorCodeExtractRequest(int packageType, int error);
	int extractSpkg(SceUID pupFd, const SpkgEntry *entry, void *outputBuffer);
#ifdef UPDATER_CORE_2014
	int addTarFileToMemoryFs(SceUID pupFd, const char *tarPath, const char *fileName,
				 SceIoStat *stat, bool directWrite);
	int applyManifestDirectory(SceUID pupFd);
	int decodeSerialDateTime(SceDateTime *dateTime);
	int applyManifestDirect(SceUID pupFd);
	int updateDirect(SceUID pupFd);
	int mountTargetPartition(bool writable);
#else
	int addTarFileToMemoryFs(SceUID pupFd, const char *tarPath, const char *fileName,
				 SceIoStat *stat);
	int applyManifestDirectory(SceUID pupFd);
	int mountTargetPartition(void);
#endif
	int allocateTarBuffer(void);
	int allocateImageMemBlock(void);
	int setupMemoryFs(void);
	int parseManifestSerialNumber(void);
	int extractManifest(SceUID pupFd);

	char *m_manifestBuffer;
	SpkgEntry m_manifestSpkg;
	SceUID m_memBlockId;
	SceUInt8 *m_memBlockBase;
	int m_manifestOffset;
	SceUInt32 m_serialNumber;
	char m_currentDirPath[1024];
	SceUInt8 *m_tarBuffer;
	SceUInt32 m_tarSpkgIndex;
	SceUInt32 m_unusedDiffState;
	MemoryFatFs m_fatFs;
	TarArchive m_tarArchive;
};

#endif
