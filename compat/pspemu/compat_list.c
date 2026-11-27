/*
 * compat_list.c: ScePspemu per-title compatibility list downloader, SPKG
 * extractor, and title rule lookup (0x8101feb0..0x81020627).
 */
#include "host_common.h"
#include "compat_list_data.h"

#include <modulemgr/certified_file.h>
#include <update_mgr/update_mgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPAT_LIST_MAX_SIZE           136016U
#define COMPAT_LIST_RECORD_SIZE        136U
#define COMPAT_LIST_HEADER_SIZE        16U
#define COMPAT_LIST_VERSION_EXPECTED   10U
#define COMPAT_HTTP_POOL_SIZE          0xA000U
#define COMPAT_NET_POOL_SIZE           0x8000U

#define COMPAT_SPKG_TYPE_EMULIST       27
#define COMPAT_SPKG_NODE_EXTRACT       3
#define COMPAT_SPKG_EXTRACT_FLAGS      9U
#define COMPAT_SPKG_PROGRESS_DONE      5U

#define COMPAT_ERR_FILE_TOO_LARGE      ((int)0x8001001B)
#define COMPAT_ERR_IO                  ((int)0x80010005)
#define COMPAT_ERR_HTTP_STATUS         ((int)0x80010022)

#define COMPAT_LIST_DIR_PATH           "ur0:emu_list"
#define COMPAT_LIST_PKG_PATH           "ur0:emu_list/compat_info.dat"
#define COMPAT_LIST_BIN_PATH           "ur0:emu_list/title_specific.bin"
#define COMPAT_LIST_URL                "http://vitacl.ww.dl.playstation.net/vitacl/ww/j/compat_info.dat"

typedef struct CompatFileHeader {
	char magic[4];
	uint32_t formatVersion;
	uint32_t listVersion;
	uint32_t reserved;
} CompatFileHeader;

typedef struct CompatFileRecord {
	uint32_t titleHash;
	uint32_t compatFlags;
	int32_t compatParams[32];
} CompatFileRecord;

_Static_assert(sizeof(CompatFileHeader) == COMPAT_LIST_HEADER_SIZE, "CompatFileHeader size");
_Static_assert(sizeof(CompatFileRecord) == COMPAT_LIST_RECORD_SIZE, "CompatFileRecord size");

static char g_compatListVersion[16] = "-";
static char g_compatTitleId[16];
static uint8_t g_compatNetPool[COMPAT_NET_POOL_SIZE];
static uint32_t g_compatFlags;
static int32_t g_compatParams[32];
static int g_compatRestrictedTitle;

/* 0x8101feb0 */
static int __attribute__((noinline)) compatExtractPackage(void *outBuf)
{
	SceIoStat stat;
	void *spkgBuf = NULL;
	uint32_t reqId = 0;
	uint32_t seqNo = 0;
	uint32_t statusResult = 0;
	uint32_t progress = 0;
	uint32_t writtenRates = 0;
	SceUID srcFd = 0;
	SceUID dstFd = 0;
	int ret;

	ret = sceIoGetstat(COMPAT_LIST_PKG_PATH, &stat);
	if (ret < 0)
		return ret;

	if (stat.st_size > (SceOff)COMPAT_LIST_MAX_SIZE) {
		ret = COMPAT_ERR_FILE_TOO_LARGE;
	} else {
		ret = sceSblUsAllocateBuffer((SceSize)stat.st_size, &spkgBuf);
		if (ret == 0) {
			srcFd = sceIoOpen(COMPAT_LIST_PKG_PATH, SCE_O_RDONLY, 0);
			if (srcFd >= 0) {
				SceSSize bytesRead = sceIoRead(srcFd, spkgBuf, (SceSize)stat.st_size);
				if ((uint64_t)(uint32_t)bytesRead == (uint64_t)stat.st_size) {
					ret = sceSblUsExtractSpackage(COMPAT_SPKG_TYPE_EMULIST,
					                              spkgBuf,
					                              (SceSize)bytesRead,
					                              COMPAT_SPKG_EXTRACT_FLAGS,
					                              &reqId);
					if (ret == 0) {
						for (;;) {
							ret = sceSblUsGetStatus(COMPAT_SPKG_NODE_EXTRACT,
							                        reqId,
							                        &seqNo,
							                        &statusResult,
							                        &progress,
							                        &writtenRates);
							if (ret != 0)
								break;
							if (progress == COMPAT_SPKG_PROGRESS_DONE)
								break;
							sceKernelDelayThread(500000U);
						}
						if (ret == 0) {
							ret = (int)statusResult;
							if (statusResult == 0U) {
								const uint8_t *spkgBytes = (const uint8_t *)spkgBuf;
								const cf_header *hdr = (const cf_header *)spkgBuf;
								const SceSblUsExtractedSpackage *spkg =
									(const SceSblUsExtractedSpackage *)(const void *)&spkgBytes[hdr->file_offset];

								ret = sceSblUsGetExtractSpackage(
									COMPAT_SPKG_NODE_EXTRACT,
									reqId,
									(uint32_t)spkg->info.decryptedSize,
									COMPAT_LIST_MAX_SIZE,
									outBuf);
								if (ret == 0) {
									dstFd = sceIoOpen(COMPAT_LIST_BIN_PATH,
									                  SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC,
									                  6);
									if (dstFd >= 0) {
										ret = sceIoWrite(dstFd, outBuf, (SceSize)spkg->chunk.size);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (spkgBuf != NULL)
		sceSblUsReleaseBuffer(spkgBuf);
	if (srcFd > 0)
		sceIoClose(srcFd);
	if (dstFd > 0)
		sceIoClose(dstFd);
	sceIoRemove(COMPAT_LIST_PKG_PATH);
	return ret;
}

/* 0x8102002c */
static int compatHttpRedirectFilter(int tmplId, int statusCode, int method,
                                    const char *location, void *userArg)
{
	(void)tmplId;
	(void)statusCode;
	(void)method;
	(void)userArg;
	return (sceClibStrncasecmp(location, "https:", 6U) == 0) ? -1 : 0;
}

/* 0x8102004c */
const char *pspemuCompatGetListVersion(void)
{
	return g_compatListVersion;
}

/* 0x81020058 */
int pspemuCompatHasFlag(uint32_t flagMask)
{
	return (flagMask & g_compatFlags) != 0;
}

/* 0x8102006c */
int pspemuCompatGetParam(int index)
{
	return g_compatParams[index];
}

/* 0x8102007c */
int pspemuCompatEnsureListDir(void)
{
	SceIoStat stat;

	if (sceIoGetstat(COMPAT_LIST_DIR_PATH, &stat) < 0) {
		int ret = sceIoMkdir(COMPAT_LIST_DIR_PATH, 6);
		return (ret < 0) ? ret : 0;
	}
	if ((stat.st_mode & 0xF000U) == 0x1000U)
		return 0;
	return COMPAT_ERR_IO;
}

/* 0x810200d8 */
int pspemuTitleInfoThread(SceSize args, void *argp)
{
	SceNetInitParam netParam;
	void *workBuf;
	int netState = 0;
	int ret;

	(void)args;
	(void)argp;

	ret = pspemuLoadSysmodules();
	if (ret != 0)
		return ret;

	netParam.memory = g_compatNetPool;
	netParam.size = sizeof(g_compatNetPool);
	netParam.flags = 0;
	ret = sceNetInit(&netParam);
	if (ret < 0)
		return ret;

	ret = sceNetCtlInit();
	if (ret < 0)
		return ret;

	workBuf = memalign(16U, COMPAT_LIST_MAX_SIZE);
	if (workBuf == NULL)
		return 0;

	memset(workBuf, 0, COMPAT_LIST_MAX_SIZE);

	if (compatExtractPackage(workBuf) < 0 &&
	    sceCompatGetUpdateState() == 1 &&
	    SceNetCtl_E8E8DF18(&netState) >= 0 &&
	    netState == 4 &&
	    sceHttpInit(COMPAT_HTTP_POOL_SIZE) >= 0) {
		char userAgent[32];
		uint64_t contentLength = 0;
		int statusCode = 0;
		int tmplId = 0;
		int connId = 0;
		int reqId = 0;
		int httpRet;

		snprintf(userAgent, sizeof(userAgent), "VITACompat/%x.%02x", 3, 0);
		tmplId = sceHttpCreateTemplate(userAgent, 2, 1);
		if (tmplId < 0) {
			httpRet = tmplId;
			tmplId = 0;
		} else {
			httpRet = sceHttpSetResolveTimeOut(tmplId, 1000000U);
			if (httpRet >= 0)
				httpRet = sceHttpSetResolveRetry(tmplId, 2);
			if (httpRet >= 0)
				httpRet = sceHttpSetConnectTimeOut(tmplId, 5000000U);
			if (httpRet >= 0)
				httpRet = sceHttpSetSendTimeOut(tmplId, 5000000U);
			if (httpRet >= 0)
				httpRet = sceHttpSetRecvTimeOut(tmplId, 5000000U);
			if (httpRet >= 0) {
				connId = sceHttpCreateConnectionWithURL(tmplId, COMPAT_LIST_URL, 1);
				if (connId < 0) {
					httpRet = connId;
					connId = 0;
				} else {
					reqId = sceHttpCreateRequestWithURL(connId, 0, COMPAT_LIST_URL, 0ULL);
					if (reqId < 0) {
						httpRet = reqId;
						reqId = 0;
					} else {
						httpRet = sceHttpSetRedirectCallback(reqId, compatHttpRedirectFilter, NULL);
						if (httpRet >= 0)
							httpRet = sceHttpSendRequest(reqId, NULL, 0);
						if (httpRet >= 0)
							httpRet = sceHttpGetStatusCode(reqId, &statusCode);
						if (httpRet >= 0) {
							if (statusCode == 200) {
								httpRet = sceHttpGetResponseContentLength(reqId, &contentLength);
								if (httpRet >= 0 && contentLength <= COMPAT_LIST_MAX_SIZE) {
									do {
										httpRet = sceHttpReadData(reqId, workBuf, COMPAT_LIST_MAX_SIZE);
									} while (httpRet > 0);
								}
							} else {
								httpRet = COMPAT_ERR_HTTP_STATUS;
							}
						}
					}
				}
			}
		}

		if (reqId != 0)
			sceHttpDeleteRequest(reqId);
		if (connId != 0)
			sceHttpDeleteConnection(connId);
		if (tmplId != 0)
			sceHttpDeleteTemplate(tmplId);
		sceHttpTerm();

		if (httpRet >= 0 && contentLength < COMPAT_LIST_MAX_SIZE) {
			int dirRet = pspemuCompatEnsureListDir();
			SceUID fd = dirRet;
			int readyToExtract = 0;

			if (dirRet == 0) {
				fd = sceIoOpen(COMPAT_LIST_PKG_PATH,
				               SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC,
				               6);
				if (fd > 0) {
					SceSSize written = sceIoWrite(fd, workBuf, (SceSize)contentLength);
					sceIoClose(fd);
					if ((uint64_t)(uint32_t)written == contentLength)
						readyToExtract = 1;
				} else if (fd >= 0) {
					readyToExtract = 1;
				}
			} else if (fd >= 0) {
				readyToExtract = 1;
			}

			if (readyToExtract) {
				int extractRet = compatExtractPackage(workBuf);
				if (extractRet >= 0)
					sceCompatSetUpdateState(extractRet);
			}
		}
	}

	free(workBuf);
	return 0;
}

/* 0x81020378 */
int pspemuCompatLoadTitleEntry(const char *discId, SceUID titleInfoThid)
{
	SceIoStat stat;
	int threadStat = 0;
	uint32_t titleHash;
	void *fileBuf;
	int ret;
	int foundExternal = 0;

	strncpy(g_compatTitleId, discId, sizeof(g_compatTitleId));
	if (g_compatTitleId[0] == 'N' &&
	    g_compatTitleId[1] == 'P' &&
	    (((uint8_t)g_compatTitleId[2] & 0xEFU) == 'E' || g_compatTitleId[2] == 'H') &&
	    ((uint8_t)g_compatTitleId[3] & 0xFDU) == 'X') {
		g_compatRestrictedTitle = 1;
	}

	if (pspemuIsPs1Mode() == 1) {
		ret = popsHostInitForTitle(g_compatTitleId);
		if (ret < 0)
			return ret;
	}

	memset(g_compatParams, 0xFF, sizeof(g_compatParams));
	g_compatFlags = 0U;

	{
		uint32_t prefixWord = ((uint32_t)(uint8_t)discId[0] << 24) |
		                      ((uint32_t)(uint8_t)discId[1] << 16) |
		                      ((uint32_t)(uint8_t)discId[2] << 8) |
		                      (uint32_t)(uint8_t)discId[3];
		uint32_t numPart = (uint32_t)strtol(discId + 4, NULL, 16);
		titleHash = (4097U * numPart) ^ prefixWord;
	}

	fileBuf = memalign(16U, COMPAT_LIST_MAX_SIZE);
	if (fileBuf == NULL)
		return 0;

	memset(fileBuf, 0, COMPAT_LIST_MAX_SIZE);
	ret = sceKernelWaitThreadEnd(titleInfoThid, &threadStat, NULL);
	if (ret < 0)
		return ret;
	if (threadStat != 0)
		return threadStat;

	if (sceIoGetstat(COMPAT_LIST_BIN_PATH, &stat) >= 0) {
		uint32_t fileSize = (uint32_t)stat.st_size;
		uint32_t bodySize = fileSize - COMPAT_LIST_HEADER_SIZE;
		uint32_t recordCount = bodySize / COMPAT_LIST_RECORD_SIZE;

		if (bodySize % COMPAT_LIST_RECORD_SIZE == 0U) {
			SceUID fd = sceIoOpen(COMPAT_LIST_BIN_PATH, SCE_O_RDONLY, 0);
			if (fd >= 0) {
				SceSSize bytesRead = sceIoRead(fd, fileBuf, fileSize);
				sceIoClose(fd);

				if ((uint32_t)bytesRead == fileSize) {
					const CompatFileHeader *hdr = (const CompatFileHeader *)fileBuf;
					if (memcmp(hdr->magic, "PSPC", 4U) == 0 &&
					    hdr->formatVersion == COMPAT_LIST_VERSION_EXPECTED) {
						snprintf(g_compatListVersion,
						         sizeof(g_compatListVersion),
						         "%d",
						         hdr->listVersion);
						if (recordCount != 0U) {
							const CompatFileRecord *records =
								(const CompatFileRecord *)(hdr + 1);
							uint32_t idx;
							for (idx = 0; idx < recordCount; idx++) {
								if (records[idx].titleHash == titleHash) {
									g_compatFlags = records[idx].compatFlags;
									memcpy(g_compatParams,
									       records[idx].compatParams,
									       sizeof(g_compatParams));
									foundExternal = 1;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	free(fileBuf);

	if (!foundExternal) {
		const CompatBuiltinTitle *entry = g_compatBuiltinTable;
		uint32_t flags = g_compatFlags;

		while (entry->titleHash != 0U) {
			if (entry->titleHash == titleHash && entry->paramCount > 0) {
				int32_t idx;
				for (idx = 0; idx < entry->paramCount; idx++) {
					int32_t paramIndex = entry->params[idx].paramIndex;
					uint32_t value = entry->params[idx].value;
					if (paramIndex < 0)
						flags = value;
					else
						g_compatParams[paramIndex] = (int32_t)value;
				}
			}
			entry++;
		}
		g_compatFlags = flags;
	}

	return 0;
}

/* 0x81020600 */
const char *pspemuCompatGetTitleId(void)
{
	return g_compatTitleId;
}

/* 0x8102060c */
int pspemuCompatIsRestrictedBootTitle(void)
{
	if ((g_compatFlags & 0x40000U) != 0U)
		return 0;
	return g_compatRestrictedTitle;
}
