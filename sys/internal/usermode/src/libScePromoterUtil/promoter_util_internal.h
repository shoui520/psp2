#ifndef LIBSCEPROMOTERUTIL_PROMOTER_UTIL_INTERNAL_H
#define LIBSCEPROMOTERUTIL_PROMOTER_UTIL_INTERNAL_H

/*
 * ScePromoterUtil (vs0:sys/external/libScePromoterUtil.suprx): the application installer's client.
 * Each call is a request to the shell's SceShellSvc IPMI service (SceShellSvc_B31E7F1C returns its
 * client); the installing calls start the work, poll its state until it is done and read the
 * result. No Sony record names the library's files, calls or methods; the names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "../../../../vsh/lowlayer/src/ipmi/client.h"

#define PROMOTER_UTIL_ERROR_PARAM		-2136014705	/* 0x80AF008F */
#define PROMOTER_UTIL_ERROR_BUSY		-2136014815	/* 0x80AF0021 */

/* the service's methods */
#define PROMOTER_UTIL_METHOD_ADD_PATH		0x130000
#define PROMOTER_UTIL_METHOD_ADD_PKG		0x130001
#define PROMOTER_UTIL_METHOD_ADD_RIF		0x130002
#define PROMOTER_UTIL_METHOD_GET_STATE		0x130003
#define PROMOTER_UTIL_METHOD_GET_RESULT		0x130004
#define PROMOTER_UTIL_METHOD_GET_SIZE		0x130005
#define PROMOTER_UTIL_METHOD_GET_STATUS		0x130006
#define PROMOTER_UTIL_METHOD_GET_INFO		0x130007
#define PROMOTER_UTIL_METHOD_GET_VALUE		0x130008
#define PROMOTER_UTIL_METHOD_DELETE_PKG		0x130009
#define PROMOTER_UTIL_METHOD_SET_FLAG		0x13000A
#define PROMOTER_UTIL_METHOD_START_GET_SIZE	0x13000B
#define PROMOTER_UTIL_METHOD_UPDATE_UPGRADABLE	0x13000C
#define PROMOTER_UTIL_METHOD_PROMOTE_IMPORT	0x13000D
#define PROMOTER_UTIL_METHOD_UPDATE_LIVE_AREA	0x13000E
#define PROMOTER_UTIL_METHOD_INIT		0x13000F
#define PROMOTER_UTIL_METHOD_EXIT		0x130010
#define PROMOTER_UTIL_METHOD_GET_FLAGS		0x130011
#define PROMOTER_UTIL_METHOD_GET_RESULTS	0x130012
#define PROMOTER_UTIL_METHOD_CHECK_EXIST	0x130013
#define PROMOTER_UTIL_METHOD_START_STATUS	0x130014
#define PROMOTER_UTIL_METHOD_END_STATUS		0x130015
#define PROMOTER_UTIL_METHOD_START_NAME		0x130016
#define PROMOTER_UTIL_METHOD_END_NAME		0x130017
#define PROMOTER_UTIL_METHOD_GET_PKG_INFO	0x130018
#define PROMOTER_UTIL_METHOD_GET_PKG_STATUS	0x130019
#define PROMOTER_UTIL_METHOD_START_INFO		0x13001A
#define PROMOTER_UTIL_METHOD_END_INFO		0x13001B
#define PROMOTER_UTIL_METHOD_START_PROGRESS	0x13001C
#define PROMOTER_UTIL_METHOD_END_PROGRESS	0x13001D
#define PROMOTER_UTIL_METHOD_REMOVE_SAVEDATA	0x13001E

/* the data of the requests */
#define PROMOTER_UTIL_PKG_REQUEST_SIZE		0x120
#define PROMOTER_UTIL_RIF_REQUEST_SIZE		0x8C
#define PROMOTER_UTIL_IMPORT_PARAM_SIZE		176
#define PROMOTER_UTIL_PKG_INFO_SIZE		232
#define PROMOTER_UTIL_PKG_STATUS_SIZE		72

extern "C" IPMI::Client *SceShellSvc_B31E7F1C(void);
extern "C" int SceShellSvc_A0B067AC(IPMI::EventFlagInfo *pEventFlag, int event);
extern "C" int SceShellSvc_8D4F9F2E(int event, SceUInt32 *pTimeout);
extern "C" void *sce_paf_memset(void *dst, int ch, SceSize len);
extern "C" char *sce_paf_strncpy(char *dst, const char *src, SceSize len);
extern "C" int sceKernelDelayThread(SceUInt32 usec);

/* the application's request to be promoted, from a path */
typedef struct PromoterUtilPkgRequest {
	char path[0x80];
	SceUInt8 enable;
	SceUInt8 reserved81;
	SceUInt8 sync;
	SceUInt8 flag;
	SceInt32 mode;
	SceUInt8 reserved88[0x98];
} PromoterUtilPkgRequest;

typedef struct PromoterUtilRifRequest {
	char path[0x80];
	SceUInt8 reserved80[0xC];
} PromoterUtilRifRequest;

/* ScePromoterUtilityLAUpdate */
typedef struct PromoterUtilLiveAreaUpdate {
	char titleId[0xC];
	char path[1];
} PromoterUtilLiveAreaUpdate;

/* the length of the string with its terminator */
static inline SceSize promoterUtilSize(const char *str)
{
	SceSize len = 0;

	if (*str != '\0') {
		do {
			len++;
		} while (*++str != '\0');
	}
	return len + 1;
}

/* the 32-bit FNV-1a hashes of the property names, written out for each length */
__attribute__((noinline)) SceUInt32 promoterUtilHash7(const unsigned char *a);
__attribute__((noinline)) SceUInt32 promoterUtilHash8(const unsigned char *a);
__attribute__((noinline)) SceUInt32 promoterUtilHash11(const unsigned char *a);
__attribute__((noinline)) SceUInt32 promoterUtilHash12(const unsigned char *a);
__attribute__((noinline)) SceUInt32 promoterUtilHash14(const unsigned char *a);
__attribute__((noinline)) SceUInt32 promoterUtilHash15(const unsigned char *a);

/* the ids of the properties of the installed application */
typedef struct PromoterUtilPropertyIds {
	SceUInt32 liveAreaPath;
	SceUInt32 ctime;
	SceUInt32 lltime;
	SceUInt32 promoteAttr;
	SceUInt32 upStatus;
	SceUInt32 libLocation;
	SceUInt32 appconf;
} PromoterUtilPropertyIds;

/* the service's methods, one function each (the library calls them out of line) */
int promoterUtilIpcAddPath(IPMI::Client *client, const char *path);
int promoterUtilIpcAddPkg(IPMI::Client *client, const PromoterUtilPkgRequest *request);
int promoterUtilIpcAddRif(IPMI::Client *client, const PromoterUtilRifRequest *request);
int promoterUtilIpcGetState(IPMI::Client *client, int *pState);
int promoterUtilIpcGetResult(IPMI::Client *client, int *pResult);
int promoterUtilIpcGetSize(IPMI::Client *client, int *pResult, void *pSize);
int promoterUtilIpcGetStatus(IPMI::Client *client, int *pResult, void *pStatus);
int promoterUtilIpcGetInfo(IPMI::Client *client, int *pResult, void *pInfo);
int promoterUtilIpcGetValue(IPMI::Client *client, int *pValue);
int promoterUtilIpcDeletePkg(IPMI::Client *client, const char *titleId);
int promoterUtilIpcSetFlag(IPMI::Client *client, const char *titleId, int flag);
int promoterUtilIpcStartGetSize(IPMI::Client *client, const char *titleId, void *pSize);
int promoterUtilIpcUpdateUpgradable(IPMI::Client *client, const char *titleId);
int promoterUtilIpcPromoteImport(IPMI::Client *client, const void *params);
int promoterUtilIpcUpdateLiveArea(IPMI::Client *client, const char *titleId, const char *path);
int promoterUtilIpcInit(IPMI::Client *client);
int promoterUtilIpcExit(IPMI::Client *client);
int promoterUtilIpcGetFlags(IPMI::Client *client, int *pValue);
int promoterUtilIpcGetResults(IPMI::Client *client, void *in0, void *out, int size);
int promoterUtilIpcCheckExist(IPMI::Client *client, const char *titleId, int *pExist);
int promoterUtilIpcStartStatus(IPMI::Client *client, int *pRequestId,
				      const IPMI::EventFlagInfo *pEventFlag, const char *titleId, int arg);
int promoterUtilIpcEndStatus(IPMI::Client *client, int requestId);
int promoterUtilIpcStartName(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, const char *titleId);
int promoterUtilIpcEndName(IPMI::Client *client, int requestId);
int promoterUtilIpcGetPkgInfo(IPMI::Client *client, const char *titleId, void *pInfo, int size);
int promoterUtilIpcGetPkgStatus(IPMI::Client *client, const char *titleId, void *pStatus, int size);
int promoterUtilIpcStartInfo(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, const char *titleId, int arg);
int promoterUtilIpcEndInfo(IPMI::Client *client, int requestId);
int promoterUtilIpcStartProgress(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, const char *titleId, int arg);
int promoterUtilIpcEndProgress(IPMI::Client *client, int requestId);
int promoterUtilIpcRemoveSavedata(IPMI::Client *client, const char *titleId, int arg1, int arg2);

#endif
