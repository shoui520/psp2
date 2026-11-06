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

/* the package types sceSblUsGetSpkgInfo takes (the name ours) */
#define SCE_SBL_US_SPKG_TYPE_SYSTEM	1

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
int _sceSblUsGetSpkgInfo(int packageType, SceSblUsSpkgInfo *info);

/* the stub library's wrappers */
int sceSblUsVerifyPup(const char *path);
int sceSblUsVerifyPupHeader(const char *path);
int sceSblUsVerifyPupSegmentById(const char *path, SceUInt64 segmentId);
int sceSblUsGetSpkgInfo(int packageType, SceSblUsSpkgInfo *info);
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
