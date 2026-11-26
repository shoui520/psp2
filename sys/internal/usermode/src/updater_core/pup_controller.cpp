/*
 * A PUP file (Sony's pup_controller.cpp): its verification through the update service, its
 * header and segment table, a segment's bytes, and the system version checks the network
 * update makes before it downloads one.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#ifdef UPDATER_CORE_2014
#include <kernel/threadmgr.h>
#endif
#include <libkernel/libkernel.h>
#include <ss_mgr/ss_mgr.h>
#include <update_mgr/update_mgr.h>
#include <update_mgr/pup_file.h>
#include <vshbridge/vshbridge.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "updater_memory.h"
#include "update_utility.h"
#include "pup_controller.h"

#define UPDATER_FILE_ID		0xbefcec95

/*
 * The updater's codes for the update service's errors, a block for each check: the verify's,
 * the header's, the segment's (0x80180101-07, 09-0f, 19-1f) and the system version's.
 */
#define PUP_ERROR_VERIFY		0x80180100
#define PUP_ERROR_VERIFY_HEADER		0x80180108
#define PUP_ERROR_VERIFY_SEGMENT	0x80180118
#define PUP_ERROR_SYSTEM_VERSION	0x801804e0

#define PUP_SECTOR_SIZE		2048
#define PUP_READ_ALIGN		512
#define PUP_READ_SIZE		0x40000
#define PUP_SEGMENT_ANALYSIS_SIZE	0x80

#ifdef UPDATER_CORE_2014
#define PUP_CACHED_PATH_SIZE	256

extern "C" {
int sceUpdaterGetState(void);
int SceLibKernel_A7819967(SceKernelLwMutexWork *work, int count, unsigned int *timeout);
int SceLibKernel_499EA781(SceKernelLwMutexWork *work, int count);
}

static SceUID s_cachedPupFd = -1;
static char *s_cachedPupPath = NULL;
static SceUpdaterPupInformation *s_cachedPupInfo = NULL;
static PupSegment *s_cachedSegments = NULL;
static int s_hasCachedHeader = 0;
static SceUInt64 s_cachedSegmentCount = 0;
static int s_hasCachedSegments = 0;
static SceKernelLwMutexWork s_cacheMutex;
#endif

PupController::PupController()
{
}

PupController::~PupController()
{
}

/* verifies the whole PUP */
int PupController::VerifyPup(const char *fileName)
{
	int ret;

	ret = sceSblUsVerifyPup(fileName);
	if (ret != 0) {
		UPDATER_ERROR_FMT(78, "%s : %x", fileName, ret);
		switch (ret) {
		case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
			return PUP_ERROR_VERIFY + 1;
		case SCE_SBL_US_ERROR_IO:
			return PUP_ERROR_VERIFY + 2;
		case SCE_SBL_US_ERROR_TRY_AGAIN:
			return PUP_ERROR_VERIFY + 3;
		case SCE_SBL_US_ERROR_NO_MEMORY:
			return PUP_ERROR_VERIFY + 4;
		case SCE_SBL_US_ERROR_MISMATCH:
			return PUP_ERROR_VERIFY + 5;
		case SCE_SBL_US_ERROR_BAD_MAGIC:
			return PUP_ERROR_VERIFY + 6;
		case SCE_SBL_US_ERROR_NOT_FOUND:
			return PUP_ERROR_VERIFY + 7;
		case SCE_SBL_US_ERROR_BAD_VERSION:
			return SCE_SBL_US_ERROR_BAD_VERSION;
		case SCE_SBL_US_ERROR_PLATFORM:
			return SCE_SBL_US_ERROR_PLATFORM;
		default:
			break;
		}
	}
	return ret;
}

/* verifies one segment of the PUP */
int PupController::VerifyPupSegment(const char *fileName, SceUInt64 segmentId)
{
	int ret;

	ret = sceSblUsVerifyPupSegmentById(fileName, segmentId);
	if (ret != 0) {
		UPDATER_ERROR_FMT(129, "%s %llx : %x", fileName, segmentId, ret);
		switch (ret) {
		case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
			return PUP_ERROR_VERIFY_SEGMENT + 1;
		case SCE_SBL_US_ERROR_IO:
			return PUP_ERROR_VERIFY_SEGMENT + 2;
		case SCE_SBL_US_ERROR_TRY_AGAIN:
			return PUP_ERROR_VERIFY_SEGMENT + 3;
		case SCE_SBL_US_ERROR_NO_MEMORY:
			return PUP_ERROR_VERIFY_SEGMENT + 4;
		case SCE_SBL_US_ERROR_MISMATCH:
			return PUP_ERROR_VERIFY_SEGMENT + 5;
		case SCE_SBL_US_ERROR_BAD_MAGIC:
			return PUP_ERROR_VERIFY_SEGMENT + 6;
		case SCE_SBL_US_ERROR_NOT_FOUND:
			return PUP_ERROR_VERIFY_SEGMENT + 7;
		default:
			break;
		}
	}
	return ret;
}

/* finds a segment in the PUP's table: its size and where it starts */
int PupController::getSegmentInfo(const char *fileName, SceUInt64 segmentId, SceUInt64 *size,
				  SceUInt64 *offset)
{
	PupSegment segment;
	PupHeader header;
	SceUInt32 i;
	SceUID fd;
	int ret, result;

	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
	if (fd < 0) {
		UPDATER_ERROR_FMT(339, "%s %o : %x", fileName, 0, fd);
		return SCE_UPDATER_ERROR_PUP_OPEN;
	}
	ret = sceIoRead(fd, &header, sizeof(header));
	if (ret < 0) {
		UPDATER_ERROR_FMT(347, "%x", ret);
		sceIoClose(fd);
		return SCE_UPDATER_ERROR_PUP_READ;
	}
	result = SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < header.m_segment_num; i++) {
		ret = sceIoRead(fd, &segment, sizeof(segment));
		if (ret < 0) {
			UPDATER_ERROR_FMT(361, "%x", ret);
			sceIoClose(fd);
			return SCE_UPDATER_ERROR_PUP_READ;
		}
		if (segmentId == segment.segment_id) {
			*size = segment.size;
			*offset = segment.offset;
			result = 0;
			break;
		}
	}
	ret = sceIoClose(fd);
	if (ret < 0) {
		UPDATER_ERROR_FMT(377, "%x", ret);
		return SCE_UPDATER_ERROR_PUP_CLOSE;
	}
	return result;
}

/* reads size bytes of a segment from offset, a sector-aligned buffer at a time */
int PupController::Parse(const char *fileName, SceUInt64 segmentId, void *pData, SceUInt64 size,
			 SceUInt64 offset)
{
	SceUInt64 segmentSize, segmentOffset;
	SceOff position, sectorOffset;
	SceUInt32 remaining, total;
	SceSize length, copied;
	SceUInt8 *dst = (SceUInt8 *)pData;
	SceUInt8 *buffer;
	SceSSize read;
	SceUID fd;
	int ret, result;

	result = getSegmentInfo(fileName, segmentId, &segmentSize, &segmentOffset);
	if (result != 0)
		return result;
	if (offset + size > segmentSize) {
		UPDATER_ERROR_FMT(193, "%lld %lld", size, segmentSize);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
	if (fd < 0) {
		UPDATER_ERROR_FMT(202, "%s %o : %x", fileName, 0, fd);
		return SCE_UPDATER_ERROR_PUP_OPEN;
	}
	sectorOffset = (SceInt32)((offset + segmentOffset) / PUP_SECTOR_SIZE) * PUP_SECTOR_SIZE;
	position = sceIoLseek(fd, sectorOffset, SCE_SEEK_SET);
	if (position < 0) {
		UPDATER_ERROR_FMT(216, "%d : %lld", (SceInt32)sectorOffset, position);
		sceIoClose(fd);
		return SCE_UPDATER_ERROR_PUP_SEEK;
	}
	buffer = (SceUInt8 *)g_updaterMemoryInfo.updaterCoreMemalign(PUP_READ_ALIGN, PUP_READ_SIZE);
	if (buffer == NULL) {
		UPDATER_ERROR_FMT(227, "%d %d", PUP_READ_ALIGN, PUP_READ_SIZE);
		sceIoClose(fd);
		return SCE_UPDATER_ERROR_NO_MEMORY;
	}
	total = (SceUInt32)(offset + segmentOffset - sectorOffset + size);
	remaining = total;
	length = PUP_READ_SIZE;
	if (remaining < PUP_READ_SIZE)
		length = remaining;
	for (;;) {
		read = sceIoRead(fd, buffer, length);
		if (read < 0) {
			UPDATER_ERROR_FMT(245, "%x", read);
			sceIoClose(fd);
			g_updaterMemoryInfo.updaterCoreFree(buffer);
			return SCE_UPDATER_ERROR_PUP_READ;
		}
		if (remaining == total) {
			/* the first sector starts before the bytes asked for */
			copied = length - (offset + segmentOffset - sectorOffset);
			sceClibMemcpy(dst, buffer + (offset + segmentOffset - sectorOffset), copied);
		} else {
			sceClibMemcpy(dst, buffer, length);
			copied = length;
		}
		dst += copied;
		if (length < PUP_READ_SIZE)
			break;
		remaining -= PUP_READ_SIZE;
		if (remaining < PUP_READ_SIZE)
			length = remaining;
	}
	g_updaterMemoryInfo.updaterCoreFree(buffer);
	ret = sceIoClose(fd);
	if (ret < 0) {
		UPDATER_ERROR_FMT(281, "%x", ret);
		return SCE_UPDATER_ERROR_PUP_CLOSE;
	}
	return result;
}

int PupController::GetSegmentSize(const char *fileName, SceUInt64 segmentId, SceUInt64 *size)
{
	SceUInt64 offset;

	return getSegmentInfo(fileName, segmentId, size, &offset);
}

#ifdef UPDATER_CORE_2014
int PupController::clearCache(void)
{
	int ret = 0;

	if (s_cachedPupFd != -1) {
		int closeRet = sceIoClose(s_cachedPupFd);

		if (closeRet < 0) {
			UPDATER_ERROR_FMT(1147, "%x", closeRet);
			ret = SCE_UPDATER_ERROR_PUP_CLOSE;
		}
		s_cachedPupFd = -1;
	}
	if (s_cachedPupPath != NULL) {
		g_updaterMemoryInfo.updaterCoreFree(s_cachedPupPath);
		s_cachedPupPath = NULL;
	}
	if (s_cachedPupInfo != NULL) {
		g_updaterMemoryInfo.updaterCoreFree(s_cachedPupInfo);
		s_cachedPupInfo = NULL;
	}
	s_hasCachedHeader = 0;
	if (s_cachedSegments != NULL) {
		g_updaterMemoryInfo.updaterCoreFree(s_cachedSegments);
		s_cachedSegments = NULL;
	}
	s_cachedSegmentCount = 0;
	s_hasCachedSegments = 0;
	return ret;
}

int PupController::InitializeCache(void)
{
	return sceKernelCreateLwMutex(&s_cacheMutex, "SceUpdaterCore PupController",
				      SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
}

int PupController::FinalizeCache(void)
{
	int ret;
	int deleteRet;

	SceLibKernel_A7819967(&s_cacheMutex, 1, NULL);
	ret = clearCache();
	SceLibKernel_499EA781(&s_cacheMutex, 1);
	deleteRet = sceKernelDeleteLwMutex(&s_cacheMutex);
	if (deleteRet < 0 && ret == 0)
		return deleteRet;
	return ret;
}

int PupController::openCachedPup(const char *fileName)
{
	int ret = 0;

	SceLibKernel_A7819967(&s_cacheMutex, 1, NULL);
	if (s_cachedPupFd != -1 && s_cachedPupPath != NULL &&
	    sceClibStrncmp(s_cachedPupPath, fileName, PUP_CACHED_PATH_SIZE) == 0) {
		SceLibKernel_499EA781(&s_cacheMutex, 1);
		return 0;
	}
	if (s_cachedPupFd != -1 && s_cachedPupPath != NULL)
		ret = clearCache();
	if (ret == 0) {
		if (s_cachedPupPath == NULL) {
			s_cachedPupPath = static_cast<char *>(
				g_updaterMemoryInfo.updaterCoreMemalign(PUP_READ_ALIGN,
									PUP_CACHED_PATH_SIZE));
		}
		if (s_cachedPupPath == NULL) {
			UPDATER_ERROR_FMT(946, "%d %d", PUP_READ_ALIGN, PUP_CACHED_PATH_SIZE);
			ret = SCE_UPDATER_ERROR_NO_MEMORY;
		} else {
			s_cachedPupFd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
			if (s_cachedPupFd < 0) {
				UPDATER_ERROR_FMT(956, "%s %o : %x", fileName, 0, s_cachedPupFd);
				ret = SCE_UPDATER_ERROR_PUP_OPEN;
				clearCache();
			} else {
				sceClibStrncpy(s_cachedPupPath, fileName, PUP_CACHED_PATH_SIZE);
			}
		}
	}
	SceLibKernel_499EA781(&s_cacheMutex, 1);
	return ret;
}

int PupController::readCachedHeader(void)
{
	PupHeader header;
	SceOff seekRet;
	SceSSize readRet;
	int ret = 0;

	SceLibKernel_A7819967(&s_cacheMutex, 1, NULL);
	s_cachedPupInfo = static_cast<SceUpdaterPupInformation *>(
		g_updaterMemoryInfo.updaterCoreMemalign(PUP_READ_ALIGN,
							sizeof(SceUpdaterPupInformation)));
	if (s_cachedPupInfo == NULL) {
		UPDATER_ERROR_FMT(990, "%d %u", PUP_READ_ALIGN,
				  (unsigned int)sizeof(SceUpdaterPupInformation));
		ret = SCE_UPDATER_ERROR_NO_MEMORY;
	} else {
		seekRet = sceIoLseek(s_cachedPupFd, 0, SCE_SEEK_SET);
		if (seekRet < 0) {
			UPDATER_ERROR_FMT(997, "%lld : %lld", 0LL, seekRet);
			ret = SCE_UPDATER_ERROR_PUP_SEEK;
		} else {
			readRet = sceIoRead(s_cachedPupFd, &header, sizeof(header));
			if (readRet < 0) {
				UPDATER_ERROR_FMT(1005, "%x", readRet);
				ret = SCE_UPDATER_ERROR_PUP_READ;
			} else {
				s_cachedPupInfo->magicVersion = header.m_magic[7];
				s_cachedPupInfo->version = header.m_format_version;
				s_cachedPupInfo->fileSize = header.m_file_offset + header.m_file_size;
				s_cachedPupInfo->attribute[0] = header.m_attribute[0];
				s_cachedPupInfo->attribute[1] = header.m_attribute[1];
				s_cachedPupInfo->attribute[2] = header.m_attribute[2];
				s_cachedPupInfo->attribute[3] = header.m_attribute[3];
				s_cachedPupInfo->platform = header.platform;
				s_cachedPupInfo->formatPolicy = header.formatPolicy;
				s_cachedPupInfo->supportList = header.m_support_list;
				s_cachedPupInfo->baseVersion = header.m_base_version;
				s_cachedPupInfo->baseBuildNo = header.m_base_buildno;
				s_cachedSegmentCount = header.m_segment_num;
			}
		}
	}
	SceLibKernel_499EA781(&s_cacheMutex, 1);
	return ret;
}

int PupController::getPupInfoCached(const char *fileName, SceUpdaterPupInformation *information)
{
	int ret;

	ret = openCachedPup(fileName);
	if (ret != 0)
		return ret;
	if (!s_hasCachedHeader) {
		s_hasCachedHeader = 1;
		ret = readCachedHeader();
		if (ret != 0)
			return ret;
	}
	*information = *s_cachedPupInfo;
	return 0;
}

int PupController::getPupInfoDirect(const char *fileName, SceUpdaterPupInformation *information)
#else
/* reads the PUP's header */
int PupController::GetPupInformation(const char *fileName, SceUpdaterPupInformation *information)
#endif
{
	PupHeader header;
	SceUID fd;
	int ret;
	int result = 0;

	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
	if (fd < 0) {
		UPDATER_ERROR_FMT(489, "%s %o : %x", fileName, 0, fd);
		return SCE_UPDATER_ERROR_PUP_OPEN;
	}
	ret = sceIoRead(fd, &header, sizeof(header));
	if (ret < 0) {
		UPDATER_ERROR_FMT(497, "%x", ret);
		sceIoClose(fd);
		return SCE_UPDATER_ERROR_PUP_READ;
	}
	information->magicVersion = header.m_magic[7];
	information->version = header.m_format_version;
	information->fileSize = header.m_file_offset + header.m_file_size;
	information->attribute[0] = header.m_attribute[0];
	information->attribute[1] = header.m_attribute[1];
	information->attribute[2] = header.m_attribute[2];
	information->attribute[3] = header.m_attribute[3];
	information->platform = header.platform;
	information->formatPolicy = header.formatPolicy;
	information->supportList = header.m_support_list;
	information->baseVersion = header.m_base_version;
	information->baseBuildNo = header.m_base_buildno;
	ret = sceIoClose(fd);
	if (ret < 0) {
		UPDATER_ERROR_FMT(534, "%x", ret);
		return SCE_UPDATER_ERROR_PUP_CLOSE;
	}
	return result;
}

#ifdef UPDATER_CORE_2014
/* reads the PUP's header, using the cached descriptor while the updater is initialized */
int PupController::GetPupInformation(const char *fileName, SceUpdaterPupInformation *information)
{
	int state = sceUpdaterGetState();

	if (state != 0 && state != 2)
		return getPupInfoCached(fileName, information);
	return getPupInfoDirect(fileName, information);
}
#endif

/* checks the PUP's header, and on a retail console that may take only QA's PUPs its version */
int PupController::IsApplicablePup(const char *fileName)
{
	SceUpdaterPupInformation information;
	SceUpdaterDeviceInfo deviceInfo;
	UpdateUtility utility;
	int ret;

	ret = sceSblUsVerifyPupHeader(fileName);
	if (ret != 0) {
		UPDATER_ERROR_FMT(406, "%s : %x", fileName, ret);
		switch (ret) {
		case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
			ret = PUP_ERROR_VERIFY_HEADER + 1;
			break;
		case SCE_SBL_US_ERROR_IO:
			ret = PUP_ERROR_VERIFY_HEADER + 2;
			break;
		case SCE_SBL_US_ERROR_TRY_AGAIN:
			ret = PUP_ERROR_VERIFY_HEADER + 3;
			break;
		case SCE_SBL_US_ERROR_NO_MEMORY:
			ret = PUP_ERROR_VERIFY_HEADER + 4;
			break;
		case SCE_SBL_US_ERROR_MISMATCH:
			ret = PUP_ERROR_VERIFY_HEADER + 5;
			break;
		case SCE_SBL_US_ERROR_BAD_MAGIC:
			ret = PUP_ERROR_VERIFY_HEADER + 6;
			break;
		case SCE_SBL_US_ERROR_NOT_FOUND:
			ret = PUP_ERROR_VERIFY_HEADER + 7;
			break;
		case SCE_SBL_US_ERROR_BAD_VERSION:
			ret = SCE_SBL_US_ERROR_BAD_VERSION;
			break;
		case SCE_SBL_US_ERROR_PLATFORM:
			ret = SCE_SBL_US_ERROR_PLATFORM;
			break;
		default:
			break;
		}
	} else {
		ret = utility.GetDeviceInfo(&deviceInfo);
		if (ret == 0 && deviceInfo.m_target == SCE_UPDATER_DEVICE_TYPE_CEX &&
		    deviceInfo.m_qaf != 0) {
			ret = GetPupInformation(fileName, &information);
			if (ret == 0 && (SceUInt32)information.version < deviceInfo.m_version)
				ret = SCE_SBL_US_ERROR_BAD_VERSION;
		}
	}
	return ret;
}

/* the update service's errors reading the installed system's version */
static inline int systemVersionError(int ret)
{
	switch (ret) {
	case SCE_SBL_US_ERROR_NOT_PERMITTED:
		return PUP_ERROR_SYSTEM_VERSION + 1;
	case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		return PUP_ERROR_SYSTEM_VERSION + 2;
	case SCE_SBL_US_ERROR_IO:
		return PUP_ERROR_SYSTEM_VERSION + 3;
	case SCE_SBL_US_ERROR_TRY_AGAIN:
		return PUP_ERROR_SYSTEM_VERSION + 4;
	case SCE_SBL_US_ERROR_NO_MEMORY:
		return PUP_ERROR_SYSTEM_VERSION + 5;
	default:
		return ret;
	}
}

/* whether a PUP of version may be downloaded: always but on a retail console that takes QA's only */
int PupController::NetworkPreCheckVersion(SceUInt32 version, int *result)
{
	SceSblUsSpkgInfo info;
	int ret = 0;

	if (vshSblAimgrIsCEX() == 1) {
		*result = 0;
		if (sceSblQafMgrIsAllowNonQAPup() != 0) {
			*result = 1;
		} else {
			ret = sceSblUsGetSpkgInfo(SCE_SBL_US_SPKG_TYPE_SYSTEM, &info);
			if (ret != 0) {
				UPDATER_ERROR_FMT(570, "%x", ret);
				return systemVersionError(ret);
			}
			*result = version >= info.version;
		}
	} else {
		*result = 1;
	}
	return ret;
}

/* whether version is the installed system's */
int PupController::NetworkCheckBaseVersion(SceUInt32 version, int *result)
{
	SceSblUsSpkgInfo info;
	int ret;

	*result = 0;
	ret = sceSblUsGetSpkgInfo(SCE_SBL_US_SPKG_TYPE_SYSTEM, &info);
	if (ret != 0) {
		UPDATER_ERROR_FMT(627, "%x", ret);
		return systemVersionError(ret);
	}
	*result = version == info.version;
	return ret;
}

/* whether version is newer than the installed system's */
int PupController::CheckNewerThanSystemVersion(SceUInt32 version, int *result)
{
	SceSblUsSpkgInfo info;
	int ret;

	*result = 0;
	ret = sceSblUsGetSpkgInfo(SCE_SBL_US_SPKG_TYPE_SYSTEM, &info);
	if (ret != 0) {
		UPDATER_ERROR_FMT(678, "%x", ret);
		return systemVersionError(ret);
	}
	*result = version > info.version;
	return ret;
}

SceUInt64 PupController::GetSegmentAnalysisSize(void)
{
	return PUP_SEGMENT_ANALYSIS_SIZE;
}
