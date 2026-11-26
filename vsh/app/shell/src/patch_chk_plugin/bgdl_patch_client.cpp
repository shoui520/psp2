/*
 * bgdl_patch_client.cpp: "SceDownload" IPMI client used by patch_chk_plugin
 * to query, create, check, and delete background game-patch download tasks.
 * 3.65-CEX: 0x813C85D0..0x813C96E3
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/string.h>
#include <std/stdio.h>
#include <file/localfile.h>
#include <ipmi/client.h>
#include "patch_chk_internal.h"

#define BGDL_CLIENT_ERR_INVALID_ARGUMENT	(-2146420215)	/* 0x80103A09 */
#define BGDL_CLIENT_ERR_INTERNAL		(-2146419968)	/* 0x80103B00 */
#define BGDL_CLIENT_ERR_ALREADY_CONNECTED	(-2146419966)	/* 0x80103B02 */
#define BGDL_CLIENT_ERR_NOT_CONNECTED		(-2146419964)	/* 0x80103B04 */
#define BGDL_CLIENT_ERR_BAD_PARAM		(-2146419962)	/* 0x80103B06 */
#define BGDL_CLIENT_ERR_OUT_OF_MEMORY		(-2146419961)	/* 0x80103B07 */
#define BGDL_CLIENT_ERR_NOT_INITIALIZED		(-2146419960)	/* 0x80103B08 */
#define BGDL_CLIENT_ERR_CONNECT_REJECTED	(-2146419935)	/* 0x80103B21 */
#define BGDL_CLIENT_ERR_ALLOC_FAILED		(-2146419920)	/* 0x80103B30 */

#define BGDL_IPMI_ERR_BUSY			(-2147351286)	/* 0x8002050A */
#define BGDL_IPMI_ERR_SERVER_RESULT		(-2147351167)	/* 0x80020581 */
#define BGDL_IPMI_ERR_CONNECTION_REFUSED	(-2147351165)	/* 0x80020583 */

#define BGDL_METHOD_DELETE_TASK			0x12340002U
#define BGDL_METHOD_LIST_TASKS			0x1234000BU
#define BGDL_METHOD_ALLOC_PARAM_PATH		0x1234000EU
#define BGDL_METHOD_REGISTER_TASK		0x1234000FU
#define BGDL_METHOD_GET_TASK_INFO		0x12340013U
#define BGDL_METHOD_CHECK_TITLE_TASK		0x1234001BU

#define BGDL_TASK_CATEGORY_PATCH		3
#define BGDL_MAX_LIST_TASKS			32
#define BGDL_MAX_PACKAGES			100

struct BgdlPatchTaskStatus {
	SceUInt32 taskState;
	SceUInt32 taskType;
	SceInt32 errorCode;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	paf::string titleId;
	paf::string contentName;
};

struct BgdlConnectParam {
	SceUInt32 reserved[4];
	int budgetMode;
};

struct BgdlTaskListItem {
	SceUInt32 taskId;
	SceUInt32 statusFlags;
};

struct BgdlRawTaskInfo {
	SceUInt32 taskState;
	SceUInt32 taskType;
	SceInt32 errorCode;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	char titleId[14];
	char contentName[256];
	char padding[2];
};

struct BgdlPackageFileRecord {
	SceUInt32 reservedHeader[2];
	SceUInt64 fileSize;
	SceUInt8 sha1Digest[20];
	char url[260];
};

struct BgdlRegisterTaskRequest {
	SceUInt32 taskCategory;
	SceUInt32 flagBits;
	SceUInt32 packagePlatform;
	SceUInt32 packageCount;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	char titleId[14];
	char contentId[52];
	char titleName[257];
	char iconPath[256];
	char packageFilePath[256];
	char patchTitleId[41];
};

struct BgdlRegisterTaskResponse {
	SceUInt32 taskId;
	SceUInt32 reserved;
	SceUInt64 requiredFreeSpace;
};

struct BgdlCheckTitleRequest {
	SceUInt32 flags;
	char titleId[16];
};

struct BgdlIpmiClientHandle {
	int handleSlots[3];
	IPMI::Client *ipmiClient;
	SceUInt8 *clientBuffer;
	SceUInt8 *scratchBuffer;
};

static int s_bgdlBudgetMode = 2;				/* 0x81549344 */
static BgdlIpmiClientHandle *s_bgdlClient = NULL;		/* 0x81549348 */
static volatile SceUInt32 s_bgdlClientRefCount = 0;		/* 0x8154934C */

static SceUInt32 AtomicIncrement(volatile SceUInt32 *target)
{
	return __sync_fetch_and_add(const_cast<SceUInt32 *>(target), 1U);
}

static SceUInt32 AtomicDecrement(volatile SceUInt32 *target)
{
	return __sync_fetch_and_sub(const_cast<SceUInt32 *>(target), 1U);
}

static void AtomicZero(volatile SceUInt32 *target)
{
	__sync_lock_test_and_set(const_cast<SceUInt32 *>(target), 0U);
}

/* ============================================================================
 * Raw IPMI Method Invokers (0x813C94C4..0x813C96E3)
 * ============================================================================ */

/* 0x813C94C4 */
static int BgdlIpmiInvokeDeleteTask(IPMI::Client *client, SceUInt32 taskId)
{
	int serverResult = 0;
	IPMI::DataInfo inInfo;
	inInfo.pData = &taskId;
	inInfo.dataSize = sizeof(taskId);
	int ret = client->invokeSyncMethod(BGDL_METHOD_DELETE_TASK, &inInfo, 1,
					   &serverResult, NULL, 0);
	return (ret == 0) ? serverResult : ret;
}

/* 0x813C9514 */
static int BgdlIpmiInvokeListTasks(IPMI::Client *client, SceUInt32 category,
				   SceUInt32 maxCount, SceUInt32 *outCount,
				   BgdlTaskListItem *outItems)
{
	int serverResult = 0;
	SceUInt32 params[2];
	params[0] = category;
	params[1] = maxCount;

	IPMI::DataInfo inInfo;
	inInfo.pData = params;
	inInfo.dataSize = sizeof(params);

	IPMI::BufferInfo outList[2];
	outList[0].pBuffer = outCount;
	outList[0].bufferSize = sizeof(*outCount);
	outList[0].outSize = 0;
	outList[1].pBuffer = outItems;
	outList[1].bufferSize = sizeof(BgdlTaskListItem) * maxCount;
	outList[1].outSize = 0;

	int ret = client->invokeSyncMethod(BGDL_METHOD_LIST_TASKS, &inInfo, 1,
					   &serverResult, outList, 2);
	return (ret == 0) ? serverResult : ret;
}

/* 0x813C957C */
static int BgdlIpmiInvokeAllocParamPath(IPMI::Client *client, const SceUInt32 *flagBits,
					char *outPath)
{
	int serverResult = 0;
	IPMI::DataInfo inInfo;
	inInfo.pData = flagBits;
	inInfo.dataSize = sizeof(*flagBits);

	IPMI::BufferInfo outInfo;
	outInfo.pBuffer = outPath;
	outInfo.bufferSize = 256;
	outInfo.outSize = 0;

	int ret = client->invokeSyncMethod(BGDL_METHOD_ALLOC_PARAM_PATH, &inInfo, 1,
					   &serverResult, &outInfo, 1);
	return (ret == 0) ? serverResult : ret;
}

/* 0x813C95D4 */
static int BgdlIpmiInvokeRegisterTask(IPMI::Client *client, const BgdlRegisterTaskRequest *req,
				      BgdlRegisterTaskResponse *outResp)
{
	int serverResult = 0;
	IPMI::DataInfo inInfo;
	inInfo.pData = req;
	inInfo.dataSize = sizeof(*req);

	IPMI::BufferInfo outInfo;
	outInfo.pBuffer = outResp;
	outInfo.bufferSize = sizeof(*outResp);
	outInfo.outSize = 0;

	int ret = client->invokeSyncMethod(BGDL_METHOD_REGISTER_TASK, &inInfo, 1,
					   &serverResult, &outInfo, 1);
	return (ret == 0) ? serverResult : ret;
}

/* 0x813C962E */
static int BgdlIpmiInvokeGetTaskInfo(IPMI::Client *client, SceUInt32 taskId,
				     BgdlRawTaskInfo *outInfo)
{
	int serverResult = 0;
	IPMI::DataInfo inInfo;
	inInfo.pData = &taskId;
	inInfo.dataSize = sizeof(taskId);

	IPMI::BufferInfo outBuf;
	outBuf.pBuffer = outInfo;
	outBuf.bufferSize = sizeof(*outInfo);
	outBuf.outSize = 0;

	int ret = client->invokeSyncMethod(BGDL_METHOD_GET_TASK_INFO, &inInfo, 1,
					   &serverResult, &outBuf, 1);
	return (ret == 0) ? serverResult : ret;
}

/* 0x813C968C */
static int BgdlIpmiInvokeCheckTitle(IPMI::Client *client, const BgdlCheckTitleRequest *req,
				    SceUInt8 *outFound)
{
	int serverResult = 0;
	IPMI::DataInfo inInfo;
	inInfo.pData = req;
	inInfo.dataSize = sizeof(*req);

	IPMI::BufferInfo outBuf;
	outBuf.pBuffer = outFound;
	outBuf.bufferSize = sizeof(*outFound);
	outBuf.outSize = 0;

	int ret = client->invokeSyncMethod(BGDL_METHOD_CHECK_TITLE_TASK, &inInfo, 1,
					   &serverResult, &outBuf, 1);
	return (ret == 0) ? serverResult : ret;
}

/* ============================================================================
 * Singleton IPMI Connection Management (0x813C900C..0x813C94C3)
 * ============================================================================ */

/* 0x813C928C */
static int BgdlIpmiConnect(BgdlIpmiClientHandle *handle, const BgdlConnectParam *param)
{
	if (handle->ipmiClient != NULL)
		return BGDL_CLIENT_ERR_ALREADY_CONNECTED;

	IPMI::Client::Config cfg;
	sce_paf_strncpy(cfg.serviceName, "SceDownload", sizeof(cfg.serviceName));
	cfg.maxSyncCallInSize = 7680;
	cfg.maxSyncCallOutSize = 7680;

	int mode = param->budgetMode;
	if (mode == 0)
		cfg.budget = 0;
	else if (mode == 1)
		cfg.budget = 1;
	else if (mode == 3)
		cfg.budget = 3;
	else
		cfg.budget = 2;

	SceSize bufSize = IPMI::Client::getBufferSize(&cfg);
	handle->clientBuffer = new SceUInt8[bufSize];
	if (handle->clientBuffer == NULL)
		return BGDL_CLIENT_ERR_ALLOC_FAILED;

	handle->scratchBuffer = new SceUInt8[4096];
	if (handle->scratchBuffer == NULL) {
		delete[] handle->clientBuffer;
		handle->clientBuffer = NULL;
		return BGDL_CLIENT_ERR_ALLOC_FAILED;
	}

	int ret = IPMI::Client::create(&handle->ipmiClient, &cfg, handle, handle->clientBuffer);
	if (ret != 0) {
		delete[] handle->clientBuffer;
		delete[] handle->scratchBuffer;
		handle->clientBuffer = NULL;
		handle->scratchBuffer = NULL;
		return ret;
	}

	int serverResult = 0;
	ret = handle->ipmiClient->connect(param, sizeof(*param), &serverResult);
	if (ret != 0) {
		int mapped = BGDL_CLIENT_ERR_INTERNAL;
		if (ret == BGDL_IPMI_ERR_CONNECTION_REFUSED)
			mapped = BGDL_CLIENT_ERR_CONNECT_REJECTED;
		else if (ret == BGDL_IPMI_ERR_SERVER_RESULT)
			mapped = serverResult;

		handle->ipmiClient->destroy();
		handle->ipmiClient = NULL;
		delete[] handle->clientBuffer;
		delete[] handle->scratchBuffer;
		handle->clientBuffer = NULL;
		handle->scratchBuffer = NULL;
		return mapped;
	}

	handle->handleSlots[0] = -1;
	handle->handleSlots[1] = -1;
	handle->handleSlots[2] = -1;
	return 0;
}

/* 0x813C942A */
static int BgdlIpmiDisconnect(BgdlIpmiClientHandle *handle)
{
	if (handle->ipmiClient == NULL)
		return BGDL_CLIENT_ERR_NOT_CONNECTED;
	if (handle->ipmiClient->disconnect() == BGDL_IPMI_ERR_BUSY)
		return BGDL_CLIENT_ERR_ALREADY_CONNECTED;

	handle->ipmiClient->destroy();
	handle->ipmiClient = NULL;
	delete[] handle->clientBuffer;
	handle->clientBuffer = NULL;
	delete[] handle->scratchBuffer;
	handle->scratchBuffer = NULL;
	return 0;
}

/* 0x813C900C */
static int BgdlClientAcquire(const BgdlConnectParam *param)
{
	if (param == NULL)
		return BGDL_CLIENT_ERR_BAD_PARAM;

	SceUInt32 prevRef = AtomicIncrement(&s_bgdlClientRefCount);
	if (prevRef != 0) {
		if (s_bgdlClient != NULL)
			return 0;
		AtomicZero(&s_bgdlClientRefCount);
		return BGDL_CLIENT_ERR_INTERNAL;
	}

	BgdlIpmiClientHandle *handle = new BgdlIpmiClientHandle;
	if (handle == NULL) {
		AtomicZero(&s_bgdlClientRefCount);
		return BGDL_CLIENT_ERR_OUT_OF_MEMORY;
	}
	handle->ipmiClient = NULL;
	handle->clientBuffer = NULL;
	handle->scratchBuffer = NULL;
	s_bgdlClient = handle;

	int ret = BgdlIpmiConnect(handle, param);
	if (ret != 0) {
		delete s_bgdlClient;
		s_bgdlClient = NULL;
		AtomicZero(&s_bgdlClientRefCount);
	}
	return ret;
}

/* 0x813C9104 */
static int BgdlClientRelease(void)
{
	if (s_bgdlClient == NULL)
		return BGDL_CLIENT_ERR_NOT_INITIALIZED;

	AtomicDecrement(&s_bgdlClientRefCount);
	if (s_bgdlClientRefCount != 0)
		return 0;

	int ret = BgdlIpmiDisconnect(s_bgdlClient);
	if (ret == 0) {
		delete s_bgdlClient;
		s_bgdlClient = NULL;
	}
	return ret;
}

/* 0x813C916A */
static int BgdlClientDeleteTask(SceUInt32 taskId)
{
	if (s_bgdlClient == NULL)
		return BGDL_CLIENT_ERR_NOT_INITIALIZED;
	return BgdlIpmiInvokeDeleteTask(s_bgdlClient->ipmiClient, taskId);
}

/* 0x813C918A */
static int BgdlClientListTasks(SceUInt32 category, SceUInt32 maxCount, SceUInt32 *outCount,
			       BgdlTaskListItem *outItems)
{
	if (s_bgdlClient == NULL)
		return BGDL_CLIENT_ERR_NOT_INITIALIZED;
	if (outCount == NULL)
		return BGDL_CLIENT_ERR_BAD_PARAM;
	return BgdlIpmiInvokeListTasks(s_bgdlClient->ipmiClient, category, maxCount, outCount,
				       outItems);
}

/* 0x813C91CA */
static int BgdlClientAllocParamPath(const SceUInt32 *flagBits, char *outPath)
{
	if (s_bgdlClient == NULL)
		return BGDL_CLIENT_ERR_NOT_INITIALIZED;
	if (flagBits == NULL || outPath == NULL)
		return BGDL_CLIENT_ERR_BAD_PARAM;
	return BgdlIpmiInvokeAllocParamPath(s_bgdlClient->ipmiClient, flagBits, outPath);
}

/* 0x813C91FA */
static int BgdlClientRegisterTask(const BgdlRegisterTaskRequest *req,
				  BgdlRegisterTaskResponse *outResp)
{
	if (s_bgdlClient == NULL)
		return BGDL_CLIENT_ERR_NOT_INITIALIZED;
	if (req == NULL || outResp == NULL)
		return BGDL_CLIENT_ERR_BAD_PARAM;
	return BgdlIpmiInvokeRegisterTask(s_bgdlClient->ipmiClient, req, outResp);
}

/* 0x813C922C */
static int BgdlClientGetTaskInfo(SceUInt32 taskId, BgdlRawTaskInfo *outInfo)
{
	if (s_bgdlClient == NULL)
		return BGDL_CLIENT_ERR_NOT_INITIALIZED;
	if (outInfo == NULL)
		return BGDL_CLIENT_ERR_BAD_PARAM;
	return BgdlIpmiInvokeGetTaskInfo(s_bgdlClient->ipmiClient, taskId, outInfo);
}

/* 0x813C925A */
static int BgdlClientCheckTitle(const BgdlCheckTitleRequest *req, SceUInt8 *outFound)
{
	if (s_bgdlClient == NULL)
		return BGDL_CLIENT_ERR_NOT_INITIALIZED;
	if (req == NULL || outFound == NULL)
		return BGDL_CLIENT_ERR_BAD_PARAM;
	return BgdlIpmiInvokeCheckTitle(s_bgdlClient->ipmiClient, req, outFound);
}

/* ============================================================================
 * Public Patch Client API (0x813C85D0..0x813C900B)
 * ============================================================================ */

/* 0x813C85D0 */
void shellBgdlPatchTaskStatusInit(BgdlPatchTaskStatus *status)
{
	status->taskState = 0;
	status->taskType = 0;
	status->errorCode = 0;
	status->versionMajor = 0;
	status->versionMinor = 0;
	status->titleId.clear();
	status->contentName.clear();
}

/* 0x813C85FA */
void shellBgdlPatchPackageEntryClear(BgdlPatchPackageEntry *entry)
{
	entry->url.clear();
	entry->size = 0;
	sce_paf_memset(entry->sha1Digest, 0, sizeof(entry->sha1Digest));
}

/* 0x813C861C */
int shellBgdlPatchDownloadTaskParamClear(BgdlPatchDownloadTaskParam *param)
{
	param->flags = 0;
	param->versionMajor = 0;
	param->versionMinor = 0;
	param->titleId.clear();
	param->titleName.clear();
	param->patchTitleId.clear();
	param->iconPath.clear();
	param->packages.clear();
	param->reservedWords[0] = 0;
	param->reservedWords[1] = 0;
	param->packagePlatform = 0;
	param->requiredFreeSpace = 0;
	return 0;
}

/* 0x813C86D0 */
static int ListPatchTaskIds(paf::vector<SceUInt32> *outIds)
{
	outIds->clear();

	BgdlConnectParam connParam;
	sce_paf_memset(&connParam, 0, sizeof(connParam));
	connParam.budgetMode = s_bgdlBudgetMode;
	int ret = BgdlClientAcquire(&connParam);
	if (ret != 0)
		return ret;

	SceUInt32 count = 0;
	BgdlTaskListItem items[BGDL_MAX_LIST_TASKS];
	ret = BgdlClientListTasks(BGDL_TASK_CATEGORY_PATCH, BGDL_MAX_LIST_TASKS, &count, items);
	BgdlClientRelease();

	if (ret == 0) {
		for (SceUInt32 index = 0; index < count; ++index) {
			if ((items[index].statusFlags & 0xF0U) == 0x80U)
				outIds->push_back(items[index].taskId);
		}
	}
	return ret;
}

/* 0x813C87A8 */
static int GetPatchTaskStatus(SceUInt32 taskId, BgdlPatchTaskStatus *outStatus)
{
	shellBgdlPatchTaskStatusInit(outStatus);

	BgdlConnectParam connParam;
	sce_paf_memset(&connParam, 0, sizeof(connParam));
	connParam.budgetMode = s_bgdlBudgetMode;
	int ret = BgdlClientAcquire(&connParam);
	if (ret != 0)
		return ret;

	BgdlRawTaskInfo rawInfo;
	sce_paf_memset(&rawInfo, 0, sizeof(rawInfo));
	ret = BgdlClientGetTaskInfo(taskId, &rawInfo);
	BgdlClientRelease();

	if (ret == 0) {
		outStatus->taskState = rawInfo.taskState;
		outStatus->taskType = rawInfo.taskType;
		outStatus->errorCode = rawInfo.errorCode;
		outStatus->versionMajor = rawInfo.versionMajor;
		outStatus->versionMinor = rawInfo.versionMinor;
		if (rawInfo.contentName[0] != '\0')
			outStatus->contentName = rawInfo.contentName;
		if (rawInfo.titleId[0] != '\0')
			outStatus->titleId = rawInfo.titleId;
	}
	return ret;
}

/* 0x813C8894 */
static int FindPatchTaskIdsByTitleId(const char *titleId, paf::vector<SceUInt32> *outIds)
{
	if (titleId == NULL)
		return BGDL_CLIENT_ERR_INVALID_ARGUMENT;

	outIds->clear();
	paf::vector<SceUInt32> allIds;
	int ret = ListPatchTaskIds(&allIds);
	if (ret == 0 && !allIds.empty()) {
		BgdlPatchTaskStatus status;
		shellBgdlPatchTaskStatusInit(&status);
		for (size_t index = 0; index < allIds.size(); ++index) {
			if (GetPatchTaskStatus(allIds[index], &status) == 0 &&
			    !status.titleId.empty() &&
			    sce_paf_strcmp(status.titleId.c_str(), titleId) == 0) {
				outIds->push_back(allIds[index]);
			}
		}
	}
	return ret;
}

/* 0x813C89FC */
int shellBgdlFindPatchTaskByTitleId(const char *titleId, BgdlPatchTaskStatus *outStatus)
{
	paf::vector<SceUInt32> ids;
	int ret = FindPatchTaskIdsByTitleId(titleId, &ids);
	if (ret == 0 && !ids.empty())
		ret = GetPatchTaskStatus(ids[0], outStatus);
	return ret;
}

/* 0x813C8A4C */
int shellBgdlDeletePatchTasksByTitleId(const char *titleId)
{
	paf::vector<SceUInt32> ids;
	int ret = FindPatchTaskIdsByTitleId(titleId, &ids);
	if (ret != 0 || ids.empty())
		return ret;

	BgdlConnectParam connParam;
	sce_paf_memset(&connParam, 0, sizeof(connParam));
	connParam.budgetMode = s_bgdlBudgetMode;
	ret = BgdlClientAcquire(&connParam);
	if (ret != 0)
		return ret;

	for (size_t index = 0; index < ids.size(); ++index)
		ret = BgdlClientDeleteTask(ids[index]);

	BgdlClientRelease();
	return ret;
}

/* 0x813C8ADC */
int shellBgdlCreatePatchDownloadTask(BgdlPatchDownloadTaskParam *param, SceUInt32 *outTaskId)
{
	size_t pkgCount = param->packages.size();
	if (pkgCount == 0 || pkgCount > BGDL_MAX_PACKAGES)
		return BGDL_CLIENT_ERR_INVALID_ARGUMENT;

	BgdlConnectParam connParam;
	sce_paf_memset(&connParam, 0, sizeof(connParam));
	connParam.budgetMode = s_bgdlBudgetMode;
	int ret = BgdlClientAcquire(&connParam);
	if (ret != 0)
		return ret;

	char pkgFilePath[260];
	sce_paf_memset(pkgFilePath, 0, sizeof(pkgFilePath));
	SceUInt32 flagBits = ((param->flags & 1U) << 4) | (((param->flags >> 1) & 1U) << 3);

	ret = BgdlClientAllocParamPath(&flagBits, pkgFilePath);
	if (ret == 0) {
		int openErr = 0;
		paf::common::SharedPtr<paf::File> file =
			paf::LocalFile::Open(pkgFilePath, 0x10602U, 6U, &openErr);
		ret = openErr;
		if (ret == 0) {
			for (size_t index = 0; index < pkgCount; ++index) {
				BgdlPackageFileRecord rec;
				sce_paf_memset(&rec, 0, sizeof(rec));
				rec.fileSize = param->packages[index].size;
				sce_paf_memcpy(rec.sha1Digest, param->packages[index].sha1Digest,
					       sizeof(rec.sha1Digest));
				sce_paf_snprintf(rec.url, 0xFFU, "%s",
						 param->packages[index].url.c_str());
				int written = file->Write(&rec, sizeof(rec));
				if (written != static_cast<int>(sizeof(rec))) {
					ret = written;
					break;
				}
			}
			file->Close();
		}

		if (ret == 0) {
			BgdlRegisterTaskRequest req;
			sce_paf_memset(&req, 0, sizeof(req));
			req.taskCategory = BGDL_TASK_CATEGORY_PATCH;
			req.flagBits = flagBits;
			req.packagePlatform = param->packagePlatform;
			req.packageCount = static_cast<SceUInt32>(pkgCount);
			req.versionMajor = param->versionMajor;
			req.versionMinor = param->versionMinor;
			sce_paf_memcpy(req.packageFilePath, pkgFilePath, 256);

			if (!param->titleId.empty())
				sce_paf_snprintf(req.titleId, 13U, "%s", param->titleId.c_str());
			if (!param->patchTitleId.empty())
				sce_paf_snprintf(req.patchTitleId, 13U, "%s",
						 param->patchTitleId.c_str());
			if (!param->contentId.empty())
				sce_paf_snprintf(req.contentId, 51U, "%s",
						 param->contentId.c_str());
			if (!param->titleName.empty()) {
				sce_paf_snprintf(req.titleName, 256U, "%s",
						 param->titleName.c_str());
				for (char *ch = req.titleName; *ch != '\0'; ++ch) {
					if (*ch == '\n')
						*ch = ' ';
				}
			}
			if (!param->iconPath.empty())
				sce_paf_snprintf(req.iconPath, 255U, "%s", param->iconPath.c_str());

			BgdlRegisterTaskResponse resp;
			sce_paf_memset(&resp, 0, sizeof(resp));
			ret = BgdlClientRegisterTask(&req, &resp);
			if (ret != 0)
				param->requiredFreeSpace = resp.requiredFreeSpace;
			else
				*outTaskId = resp.taskId;
		}
	}

	BgdlClientRelease();
	return ret;
}

/* 0x813C8F76 */
int shellBgdlIsTitleDownloadingOrInstalled(const char *titleId, bool *outFound)
{
	if (titleId == NULL)
		return BGDL_CLIENT_ERR_INVALID_ARGUMENT;

	BgdlConnectParam connParam;
	sce_paf_memset(&connParam, 0, sizeof(connParam));
	connParam.budgetMode = s_bgdlBudgetMode;
	int ret = BgdlClientAcquire(&connParam);
	if (ret != 0)
		return ret;

	BgdlCheckTitleRequest req;
	sce_paf_memset(&req, 0, sizeof(req));
	sce_paf_snprintf(req.titleId, 15U, "%s", titleId);

	SceUInt8 found = 0;
	ret = BgdlClientCheckTitle(&req, &found);
	if (ret == 0)
		*outFound = (found != 0);

	BgdlClientRelease();
	return ret;
}
