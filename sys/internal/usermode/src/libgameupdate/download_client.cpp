/*
 * download_client.cpp: SceDownload IPMI client helpers used by SceLibGameUpdate
 * to query background patch download tasks (0x810021F4-0x810028AB).
 */
#include "libgameupdate_internal.h"
#include <ipmi/client.h>

/* SceDownload IPMI method IDs */
#define DOWNLOAD_IPMI_METHOD_LIST_TASKS		0x1234000Bu
#define DOWNLOAD_IPMI_METHOD_GET_PACKAGE_STATUS	0x12340013u

/* IPMI error codes translated by the download client */
#define IPMI_ERROR_BUSY				(-2147351286)	/* 0x8002050A */
#define IPMI_ERROR_SERVER_RETURNED		(-2147351167)	/* 0x80020581 */
#define IPMI_ERROR_CONNECT_REFUSED		(-2147351165)	/* 0x80020583 */

/* Connection parameter sent to SceDownload IPMI service (20 bytes) */
struct DownloadConnectParam {
	SceUInt8 reserved[16];
	int budget;
};

/* Task identifier entry returned by DOWNLOAD_IPMI_METHOD_LIST_TASKS (8 bytes) */
struct DownloadTaskIdEntry {
	SceUInt32 taskId;
	SceUInt32 taskKind;
};

/* Raw package status returned by DOWNLOAD_IPMI_METHOD_GET_PACKAGE_STATUS (288 bytes) */
struct DownloadPackageRawStatus {
	SceUInt32 taskState;
	SceUInt32 taskType;
	SceInt32 errorCode;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	char titleId[14];
	char contentName[258];
};

typedef char DownloadRawStatusSizeCheck[sizeof(DownloadPackageRawStatus) == 288 ? 1 : -1];

struct DownloadIpmiClient {
	int state;
	IPMI::Client *ipmiClient;
	SceUInt8 *ipmiBuffer;
	SceUInt8 *workBuffer;

	int connect(const DownloadConnectParam *param);
	int disconnect();
	int listTasks(int category, int maxCount, SceUInt32 *outCount,
		      DownloadTaskIdEntry *outEntries);
	int getPackageStatus(SceUInt32 taskId, DownloadPackageRawStatus *outStatus);
};

int g_downloadClientBudget = 2;
DownloadIpmiClient *g_downloadClient = NULL;

static int downloadClientOpen(const DownloadConnectParam *param);
static int downloadClientClose();
static int downloadClientListTasks(int category, int maxCount, SceUInt32 *outCount,
				   DownloadTaskIdEntry *outEntries);
static int downloadClientGetPackageStatus(SceUInt32 taskId,
					  DownloadPackageRawStatus *outStatus);
static int downloadIpmiListTasks(IPMI::Client *client, int category, int maxCount,
				 SceUInt32 *outCount, DownloadTaskIdEntry *outEntries);
static int downloadIpmiGetPackageStatus(IPMI::Client *client, SceUInt32 taskId,
					DownloadPackageRawStatus *outStatus);

DownloadTaskStatus::DownloadTaskStatus()
{
	contentName[0] = '\0';
	titleId[0] = '\0';
	taskState = 0;
	taskType = 0;
	errorCode = 0;
	versionMajor = 0;
	versionMinor = 0;
}

DownloadTaskStatus::~DownloadTaskStatus()
{
}

__attribute__((noinline))
static int downloadListPatchTasks(SceUInt32 *outTaskIds, SceUInt32 *outCount)
{
	DownloadTaskIdEntry entries[32];
	DownloadConnectParam param;
	SceUInt32 totalCount;

	*outTaskIds = 0;
	sceClibMemset(outTaskIds, 0, sizeof(SceUInt32));
	sceClibMemset(&param, 0, sizeof(param));
	param.budget = g_downloadClientBudget;

	int ret = downloadClientOpen(&param);
	if (ret == 0) {
		totalCount = 0;
		ret = downloadClientListTasks(3, 32, &totalCount, entries);
		downloadClientClose();
		if (ret == 0) {
			*outCount = 0;
			for (SceUInt32 i = 0; i < totalCount; ++i) {
				if ((entries[i].taskKind & 0xF0u) == 0x80u) {
					SceUInt32 idx = (*outCount)++;
					outTaskIds[idx] = entries[i].taskId;
				}
			}
		}
	}
	return ret;
}

__attribute__((noinline))
static int downloadGetTaskStatus(SceUInt32 taskId, DownloadTaskStatus *outStatus)
{
	DownloadConnectParam param;
	DownloadPackageRawStatus raw;

	outStatus->contentName[0] = '\0';
	outStatus->titleId[0] = '\0';
	outStatus->taskState = 0;
	outStatus->taskType = 0;
	outStatus->errorCode = 0;
	outStatus->versionMajor = 0;
	outStatus->versionMinor = 0;

	sceClibMemset(&param, 0, sizeof(param));
	param.budget = g_downloadClientBudget;

	int ret = downloadClientOpen(&param);
	if (ret == 0) {
		sceClibMemset(&raw, 0, sizeof(raw));
		ret = downloadClientGetPackageStatus(taskId, &raw);
		downloadClientClose();
		if (ret == 0) {
			outStatus->versionMajor = raw.versionMajor;
			outStatus->versionMinor = raw.versionMinor;
			outStatus->taskState = raw.taskState;
			outStatus->taskType = raw.taskType;
			outStatus->errorCode = raw.errorCode;
			if (raw.contentName[0] != '\0')
				sceClibSnprintf(outStatus->contentName, 256, "%s", raw.contentName);
			if (raw.titleId[0] != '\0')
				sceClibSnprintf(outStatus->titleId, 31, "%s", raw.titleId);
		}
	}
	return ret;
}

__attribute__((noinline))
static int downloadQueryPatchTasks(const char *titleId, SceUInt32 *outTaskIds, int unusedMode)
{
	SceUInt32 count = 0;
	SceUInt32 allTaskIds[32];

	(void)unusedMode;
	sceClibMemset(allTaskIds, 0, sizeof(allTaskIds));
	if (titleId == NULL)
		return DOWNLOAD_CLIENT_ERROR_INVALID_ARG;

	int ret = downloadListPatchTasks(allTaskIds, &count);
	if (ret == 0 && count != 0) {
		DownloadTaskStatus status;
		sceClibMemset(outTaskIds, 0, sizeof(SceUInt32));
		for (SceUInt32 i = 0; i < count; ++i) {
			if (downloadGetTaskStatus(allTaskIds[i], &status) == 0 &&
			    (status.titleId[0] != '\0' ||
			     sceClibStrncmp(status.titleId, titleId, 32) == 0)) {
				*outTaskIds++ = allTaskIds[i];
			}
		}
	}
	return ret;
}

__attribute__((noinline))
int downloadFindPatchTask(const char *titleId, DownloadTaskStatus *outStatus)
{
	SceUInt32 taskIds[32];

	(void)outStatus;
	sceClibMemset(taskIds, 0, sizeof(taskIds));
	return downloadQueryPatchTasks(titleId, taskIds, 0);
}

__attribute__((noinline))
int downloadDeletePatchTask(const char *titleId)
{
	SceUInt32 taskIds[32];

	sceClibMemset(taskIds, 0, sizeof(taskIds));
	return downloadQueryPatchTasks(titleId, taskIds, 0);
}

__attribute__((noinline))
static int downloadClientOpen(const DownloadConnectParam *param)
{
	if (g_downloadClient != NULL)
		return DOWNLOAD_SVC_ERROR_ALREADY_OPEN;
	if (param == NULL)
		return DOWNLOAD_SVC_ERROR_INVALID_ARG;

	DownloadIpmiClient *client = new DownloadIpmiClient;
	if (client != NULL) {
		client->ipmiClient = NULL;
		client->ipmiBuffer = NULL;
		client->workBuffer = NULL;
	}
	g_downloadClient = client;
	if (client == NULL)
		return DOWNLOAD_SVC_ERROR_ALLOC_FAILED;

	int ret = client->connect(param);
	if (ret != 0) {
		if (g_downloadClient != NULL)
			delete g_downloadClient;
		g_downloadClient = NULL;
	}
	return ret;
}

__attribute__((noinline))
static int downloadClientClose()
{
	if (g_downloadClient == NULL)
		return DOWNLOAD_SVC_ERROR_NOT_INITIALIZED;

	int ret = g_downloadClient->disconnect();
	if (ret == 0) {
		if (g_downloadClient != NULL)
			delete g_downloadClient;
		g_downloadClient = NULL;
		return 0;
	}
	return ret;
}

__attribute__((noinline))
static int downloadClientListTasks(int category, int maxCount, SceUInt32 *outCount,
				   DownloadTaskIdEntry *outEntries)
{
	if (g_downloadClient == NULL)
		return DOWNLOAD_SVC_ERROR_NOT_INITIALIZED;
	if (outCount == NULL)
		return DOWNLOAD_SVC_ERROR_INVALID_ARG;
	return g_downloadClient->listTasks(category, maxCount, outCount, outEntries);
}

__attribute__((noinline))
static int downloadClientGetPackageStatus(SceUInt32 taskId,
					  DownloadPackageRawStatus *outStatus)
{
	if (g_downloadClient == NULL)
		return DOWNLOAD_SVC_ERROR_NOT_INITIALIZED;
	if (outStatus == NULL)
		return DOWNLOAD_SVC_ERROR_INVALID_ARG;
	return g_downloadClient->getPackageStatus(taskId, outStatus);
}

__attribute__((noinline))
int DownloadIpmiClient::connect(const DownloadConnectParam *param)
{
	if (ipmiClient != NULL)
		return DOWNLOAD_SVC_ERROR_ALREADY_OPEN;

	IPMI::Client::Config config;
	switch (param->budget) {
	case 0:
		config.budget = 0;
		break;
	case 1:
		config.budget = 1;
		break;
	case 3:
		config.budget = 3;
		break;
	default:
		config.budget = 2;
		break;
	}

	sceClibSnprintf(config.serviceName, sizeof(config.serviceName), "%s", "SceDownload");
	config.maxSyncCallInSize = 7680;
	config.maxSyncCallOutSize = 7680;

	SceSize bufSize = IPMI::Client::getBufferSize(&config);
	ipmiBuffer = new SceUInt8[bufSize];
	if (ipmiBuffer == NULL)
		return DOWNLOAD_SVC_ERROR_NO_MEMORY;

	workBuffer = new SceUInt8[4096];
	if (workBuffer == NULL) {
		delete[] ipmiBuffer;
		return DOWNLOAD_SVC_ERROR_NO_MEMORY;
	}

	int ret = IPMI::Client::create(&ipmiClient, &config, this, ipmiBuffer);
	if (ret != 0) {
		delete[] ipmiBuffer;
		delete[] workBuffer;
		return ret;
	}

	int connectResult = 0;
	ret = ipmiClient->connect(param, sizeof(*param), &connectResult);
	if (ret != 0) {
		int err;
		switch (ret) {
		case IPMI_ERROR_SERVER_RETURNED:
			err = connectResult;
			break;
		case IPMI_ERROR_CONNECT_REFUSED:
			err = DOWNLOAD_SVC_ERROR_CONNECT_REFUSED;
			break;
		default:
			err = DOWNLOAD_SVC_ERROR_INTERNAL;
			break;
		}
		ipmiClient->destroy();
		delete[] ipmiBuffer;
		delete[] workBuffer;
		return err;
	}

	state = -1;
	return 0;
}

__attribute__((noinline))
int DownloadIpmiClient::disconnect()
{
	if (ipmiClient == NULL)
		return DOWNLOAD_SVC_ERROR_NOT_OPEN;
	if (ipmiClient->disconnect() == IPMI_ERROR_BUSY)
		return DOWNLOAD_SVC_ERROR_ALREADY_OPEN;

	ipmiClient->destroy();
	ipmiClient = NULL;
	delete[] ipmiBuffer;
	ipmiBuffer = NULL;
	delete[] workBuffer;
	workBuffer = NULL;
	return 0;
}

__attribute__((noinline))
int DownloadIpmiClient::listTasks(int category, int maxCount, SceUInt32 *outCount,
				  DownloadTaskIdEntry *outEntries)
{
	return downloadIpmiListTasks(ipmiClient, category, maxCount, outCount, outEntries);
}

__attribute__((noinline))
int DownloadIpmiClient::getPackageStatus(SceUInt32 taskId,
					 DownloadPackageRawStatus *outStatus)
{
	return downloadIpmiGetPackageStatus(ipmiClient, taskId, outStatus);
}

__attribute__((noinline))
static int downloadIpmiListTasks(IPMI::Client *client, int category, int maxCount,
				 SceUInt32 *outCount, DownloadTaskIdEntry *outEntries)
{
	int methodResult = 0;
	int inArgs[2];
	IPMI::DataInfo inList[1];
	IPMI::BufferInfo outList[2];

	inArgs[0] = category;
	inArgs[1] = maxCount;
	inList[0].pData = inArgs;
	inList[0].dataSize = sizeof(inArgs);

	outList[0].pBuffer = outCount;
	outList[0].bufferSize = sizeof(*outCount);
	outList[1].pBuffer = outEntries;
	outList[1].bufferSize = (SceSize)maxCount * sizeof(*outEntries);

	int ret = client->invokeSyncMethod(DOWNLOAD_IPMI_METHOD_LIST_TASKS,
					   inList, 1, &methodResult, outList, 2);
	if (ret == 0)
		ret = methodResult;
	return ret;
}

__attribute__((noinline))
static int downloadIpmiGetPackageStatus(IPMI::Client *client, SceUInt32 taskId,
					DownloadPackageRawStatus *outStatus)
{
	int methodResult = 0;
	SceUInt32 id = taskId;
	IPMI::DataInfo inList[1];
	IPMI::BufferInfo outList[1];

	inList[0].pData = &id;
	inList[0].dataSize = sizeof(id);

	outList[0].pBuffer = outStatus;
	outList[0].bufferSize = sizeof(*outStatus);

	int ret = client->invokeSyncMethod(DOWNLOAD_IPMI_METHOD_GET_PACKAGE_STATUS,
					   inList, 1, &methodResult, outList, 1);
	if (ret == 0)
		ret = methodResult;
	return ret;
}
