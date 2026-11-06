/*
 * The user side of SceSblSsUpdateMgr (update_mgr's SceSblSsUpdateMgr library): the update
 * status a system updater reports, which the system shows on the display.
 *
 * A program reaches each system call through the stub library's wrapper of the same name
 * without the leading underscore (cui_setupper: 0x81015a10 to 0x81015ba8), as it reaches
 * SceVshBridge's. The header's name is ours.
 */
#ifndef UPDATE_MGR_UPDATE_MGR_H
#define UPDATE_MGR_UPDATE_MGR_H

#include <stdint.h>
#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* an installed package's information (sceSblUsGetSpkgInfo) */
typedef struct SceSblUsSpkgInfo {
	SceSize size;
	uint32_t version;
	uint8_t status[4];
	int reserved2;
} SceSblUsSpkgInfo;

/* Whole user request, including the uninterpreted words preserved on output. */
typedef struct SceKernelSpackageArgs {
	SceSize arg_size;
	uint32_t package_type;
	void *addr;
	SceSize size;
	uint32_t flags;
	uint32_t extension[2];
	uint32_t *seq_no;
	uint32_t *result;
	uint32_t *progress;
	uint32_t *written_rates;
} SceKernelSpackageArgs;

/* The package-info record is separate from the following chunk-info record. */
typedef struct SpkgPackageInfo {
	uint32_t version;
	uint32_t type;
	uint64_t hardwareInfo;
	uint64_t updateVersion;
	uint64_t finalSize;
	uint64_t decryptedSize;
	uint32_t requirements;
	uint32_t platform;
	uint32_t downgradeBarrier;
	uint32_t baseVersion;
	uint8_t description[8];
} SpkgPackageInfo;

/* Descriptions are serialized secure-service output, not kernel-owned fields. */
typedef struct SpkgChunkInfo {
	uint8_t description[16];
	uint64_t offset;
	uint64_t size;
	uint64_t chunkIndex;
	uint64_t chunkCount;
	uint8_t extension[16];
} SpkgChunkInfo;

/*
 * What sceSblUsGetExtractSpackage hands back past the certified-file header: the package's two
 * records, then the chunk's data (the name ours).
 */
typedef struct SceSblUsExtractedSpackage {
	SpkgPackageInfo info;
	SpkgChunkInfo chunk;
} SceSblUsExtractedSpackage;

/* the package types sceSblUsGetSpkgInfo takes (the name ours) */
#define SCE_SBL_US_SPKG_TYPE_SYSTEM	1
#define SCE_SBL_US_SPKG_TYPE_MANUFACTURING	23	/* the manufacturing image (cui_setupper's message) */

/* the update service's errors (the names ours, after ss_mgr's for the same codes) */
#define SCE_SBL_US_ERROR_NOT_PERMITTED		((int)0x800f0201)
#define SCE_SBL_US_ERROR_NOT_FOUND		((int)0x800f0202)
#define SCE_SBL_US_ERROR_IO			((int)0x800f0205)
#define SCE_SBL_US_ERROR_NO_MEMORY		((int)0x800f020c)
#define SCE_SBL_US_ERROR_INVALID_ARGUMENT	((int)0x800f0216)
#define SCE_SBL_US_ERROR_TRY_AGAIN		((int)0x800f0223)
#define SCE_SBL_US_ERROR_BAD_MAGIC		((int)0x800f0224)
#define SCE_SBL_US_ERROR_MISMATCH		((int)0x800f0227)
#define SCE_SBL_US_ERROR_PLATFORM		((int)0x800f0231)
#define SCE_SBL_US_ERROR_BAD_VERSION		((int)0x800f0237)

/* the system calls (sceSblUs*ForUser in update_mgr), weak: a system may not have them */
int _sceSblUsInformUpdateStarted(int number1, int number2, const char *str, SceSize len);
int _sceSblUsInformUpdateOngoing(int number1, int number2);
int _sceSblUsInformUpdateFinished(int number, const char *str, SceSize len);
int _sceSblUsSetSwInfoInt(const char *name, SceSize nameLength, SceUInt32 value);
int _sceSblUsSetSwInfoStr(const char *name, SceSize nameLength, const char *value,
			  SceSize valueLength);
int _sceSblUsSetSwInfoBin(const char *name, SceSize nameLength, const void *value,
			  SceSize valueLength);

int _sceSblUsVerifyPup(const char *path);
int _sceSblUsVerifyPupHeader(const char *path);
int _sceSblUsVerifyPupSegmentById(const char *path, SceUInt64 segmentId);
int _sceSblUsUpdateSpackage(int packageType, const SceKernelSpackageArgs *args,
			    uint32_t *requestId);
int _sceSblUsInspectSpackage(int packageType, const SceKernelSpackageArgs *args,
			     uint32_t *requestId);
int _sceSblUsExtractSpackage(int packageType, const SceKernelSpackageArgs *args,
			     uint32_t *requestId);
int _sceSblUsGetExtractSpackage(int nodeType, uint32_t requestId, SceKernelSpackageArgs *args);
int _sceSblUsAllocateBuffer(SceSize size, void **buffer);
int _sceSblUsReleaseBuffer(void *buffer);
int _sceSblUsGetStatus(int nodeType, uint32_t requestId, const SceKernelSpackageArgs *args);
int _sceSblUsGetSpkgInfo(int packageType, SceSblUsSpkgInfo *info);
int _sceSblUsGetUpdateMode(SceUInt8 *mode);
int _sceSblUsSetUpdateMode(SceUInt8 mode);
int _sceSblUsPowerControl(int operation, int flags);

/* the stub library's wrappers */
int sceSblUsVerifyPup(const char *path);
int sceSblUsVerifyPupHeader(const char *path);
int sceSblUsVerifyPupSegmentById(const char *path, SceUInt64 segmentId);
int sceSblUsUpdateSpackage(int packageType, void *addr, SceSize size, uint32_t flags,
			   uint32_t *requestId);
int sceSblUsInspectSpackage(int packageType, void *addr, SceSize size, uint32_t flags,
			    uint32_t *requestId);
int sceSblUsExtractSpackage(int packageType, void *addr, SceSize size, uint32_t flags,
			    uint32_t *requestId);
int sceSblUsGetExtractSpackage(int nodeType, uint32_t requestId, uint32_t extension,
			       SceSize bufferSize, void *buffer);
int sceSblUsAllocateBuffer(SceSize size, void **buffer);
int sceSblUsReleaseBuffer(void *buffer);
int sceSblUsGetStatus(int nodeType, uint32_t requestId, uint32_t *seqNo, uint32_t *result,
		      uint32_t *progress, uint32_t *writtenRates);
int sceSblUsGetSpkgInfo(int packageType, SceSblUsSpkgInfo *info);
int sceSblUsGetUpdateMode(SceUInt8 *mode);
int sceSblUsSetUpdateMode(SceUInt8 mode);
int sceSblUsPowerControl(int operation, int flags);
int sceSblUsInformUpdateStarted(int number1, int number2, const char *str, SceSize len);
int sceSblUsInformUpdateOngoing(int number1, int number2);
int sceSblUsInformUpdateFinished(int number, const char *str, SceSize len);
int sceSblUsSetSwInfoInt(const char *name, SceSize nameLength, SceUInt32 value);
int sceSblUsSetSwInfoStr(const char *name, SceSize nameLength, const char *value,
			 SceSize valueLength);
int sceSblUsSetSwInfoBin(const char *name, SceSize nameLength, const void *value,
			 SceSize valueLength);

#ifdef __cplusplus
}
#endif

#endif
