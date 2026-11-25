/*
 * np_friend_list.cpp: C-style bridge between SceNpImpsUtil and NpFriendStorage
 * for the online play session log (0x812BCEDC - 0x812BD5A3).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/new>
#include <std/string.h>
#include <np/np_npid.h>
#include <np/np_common.h>
#include "shell.h"
#include "np_friend_storage.h"

#define NP_FRIEND_LIST_ERR_INVALID_KIND		-2141876735	/* 0x80558E01 */
#define NP_FRIEND_LIST_ERR_INVALID_ARG		-2141876734	/* 0x80558E02 */
#define NP_FRIEND_LIST_ERR_ALREADY_SET		-2141876732	/* 0x80558E04 */
#define NP_FRIEND_LIST_ERR_NO_MEMORY		-2141876730	/* 0x80558E06 */

/* App title metadata returned by SceNpImpsSvc (0x812F86D8) */
struct NpImpsAppTitleInfo {
	char titleId[10];
	char longTitleName[128];
	char shortTitleName[16];
};

/* Session log output record for list queries (696 bytes) */
struct NpFriendSessionLogEntry {
	paf::rtc::Tick date;			/* +0x000 */
	SceNpId npId;				/* +0x008 */
	SceNpCommunicationId communicationId;	/* +0x02C */
	char longTitleName[128];		/* +0x038 */
	char description[512];			/* +0x0B8 */
};

/* Session log output record for per-player queries (706 bytes) */
struct NpFriendPlayerSessionLogEntry {
	paf::rtc::Tick date;			/* +0x000 */
	SceNpId npId;				/* +0x008 */
	SceNpCommunicationId communicationId;	/* +0x02C */
	char longTitleName[128];		/* +0x038 */
	char description[512];			/* +0x0B8 */
	char titleId[10];			/* +0x2B8 */
};

extern int npImpsSvcGetAppTitleInfo(SceUID processId, NpImpsAppTitleInfo *outInfo);	/* 0x812F86D8 */

static bool s_allocatorLocked = false;				/* 0x81545B98 */
static void *(*s_listAlloc)(size_t size) = NULL;		/* 0x81545B9C */
static void (*s_listFree)(void *ptr) = NULL;			/* 0x81545BA0 */

/* 0x812BCEDC */
int npFriendListSetAllocator(const NpFriendAllocator *allocator)
{
	if (s_allocatorLocked)
		return NP_FRIEND_LIST_ERR_ALREADY_SET;
	if (allocator == NULL)
		return NP_FRIEND_LIST_ERR_INVALID_ARG;

	s_listAlloc = allocator->malloc;
	s_listFree = allocator->free;
	return 0;
}

/* 0x812BCF12 */
int npFriendSessionLogAdd(const paf::rtc::Tick *date, const SceNpId *npId,
			  const SceNpCommunicationId *commId, const char *description,
			  SceUID processId)
{
	NpFriendStorage *storage = NULL;
	NpFriendSessionLog logEntry;
	NpImpsAppTitleInfo titleInfo;

	int ret = npImpsSvcGetAppTitleInfo(processId, &titleInfo);
	if (ret < 0)
		return ret;

	sce_paf_memcpy(&logEntry.npId, npId, sizeof(SceNpId));
	logEntry.date = *date;
	logEntry.titleId.assign(titleInfo.titleId, sce_paf_strlen(titleInfo.titleId));
	sce_paf_memcpy(&logEntry.communicationId, commId, sizeof(SceNpCommunicationId));
	logEntry.shortTitleName.assign(titleInfo.shortTitleName,
				       sce_paf_strlen(titleInfo.shortTitleName));
	logEntry.longTitleName.assign(titleInfo.longTitleName,
				      sce_paf_strlen(titleInfo.longTitleName));
	SceSize descLen = (description != NULL) ? sce_paf_strlen(description) : 0;
	logEntry.description.assign(description, descLen);

	ret = NpFriendStorage::GetInstance(&storage);
	if (ret >= 0)
		ret = storage->AddSessionLog(&logEntry);

	return ret;
}

/* 0x812BD092 */
int npFriendSessionLogGetCount(int queryKind, const SceNpCommunicationId *commId,
			       unsigned int *outCount)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	int which = -1;
	if (queryKind == 0)
		which = NP_FRIEND_SESSION_LOG_ALL;
	else if (queryKind == 1)
		which = NP_FRIEND_SESSION_LOG_TITLE;

	return storage->GetSessionLogCount(which, commId, outCount);
}

/* 0x812BD0FE */
int npFriendSessionLogGetList(int queryKind, const SceNpCommunicationId *commId,
			      unsigned int startIndex, NpFriendSessionLogEntry *outEntries,
			      unsigned int maxCount, unsigned int *outCount)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	int which;
	if (queryKind == 0)
		which = NP_FRIEND_SESSION_LOG_ALL;
	else if (queryKind == 1)
		which = NP_FRIEND_SESSION_LOG_TITLE;
	else
		return NP_FRIEND_LIST_ERR_INVALID_KIND;

	NpFriendSessionLog **logPtrs =
		reinterpret_cast<NpFriendSessionLog **>(s_listAlloc(sizeof(NpFriendSessionLog *) * maxCount));
	if (logPtrs == NULL)
		return NP_FRIEND_LIST_ERR_NO_MEMORY;

	bool allocFailed = false;
	for (unsigned int i = 0; i < maxCount; ++i) {
		void *mem = s_listAlloc(sizeof(NpFriendSessionLog));
		if (mem == NULL) {
			logPtrs[i] = NULL;
			for (unsigned int j = i; j < maxCount; ++j)
				logPtrs[j] = NULL;
			allocFailed = true;
			ret = NP_FRIEND_LIST_ERR_NO_MEMORY;
			break;
		}
		logPtrs[i] = new (mem) NpFriendSessionLog();
	}

	if (!allocFailed) {
		ret = storage->GetSessionLogs(which, commId, startIndex, maxCount, logPtrs, outCount);
		if (ret >= 0 && *outCount != 0) {
			for (unsigned int i = 0; i < *outCount; ++i) {
				NpFriendSessionLog *src = logPtrs[i];
				NpFriendSessionLogEntry *dst = &outEntries[i];
				dst->date = src->date;
				sce_paf_memcpy(&dst->npId, &src->npId, sizeof(SceNpId));
				sce_paf_memcpy(&dst->communicationId, &src->communicationId,
					       sizeof(SceNpCommunicationId));
				sce_paf_strncpy(dst->longTitleName, src->longTitleName.c_str(),
						sizeof(dst->longTitleName));
				sce_paf_strncpy(dst->description, src->description.c_str(),
						sizeof(dst->description));
			}
		}
	}

	for (unsigned int i = 0; i < maxCount; ++i) {
		if (logPtrs[i] != NULL) {
			logPtrs[i]->~NpFriendSessionLog();
			s_listFree(logPtrs[i]);
		}
	}
	s_listFree(logPtrs);
	return ret;
}

/* 0x812BD344 */
int npFriendSessionLogDelete(SceInt64 logId)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	return storage->DeleteSessionLog(logId);
}

/* 0x812BD38A */
int npFriendSessionLogDeleteAll(void)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	return storage->DeleteSessionLogs();
}

/* 0x812BD3C0 */
int npFriendPlayerSessionLogGetCount(const char *onlineId, unsigned int *outCount)
{
	NpFriendStorage *storage = NULL;
	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret < 0)
		return ret;

	return storage->GetPlayerSessionLogCount(onlineId, outCount);
}

/* 0x812BD3FE */
int npFriendPlayerSessionLogGet(const char *onlineId, unsigned int index,
				NpFriendPlayerSessionLogEntry *outEntry)
{
	NpFriendStorage *storage = NULL;
	NpFriendSessionLog logEntry;

	int ret = NpFriendStorage::GetInstance(&storage);
	if (ret >= 0) {
		ret = storage->GetPlayerSessionLog(onlineId, index, &logEntry);
		if (ret >= 0) {
			outEntry->date = logEntry.date;
			sce_paf_memcpy(&outEntry->npId, &logEntry.npId, sizeof(SceNpId));
			sce_paf_memcpy(&outEntry->communicationId, &logEntry.communicationId,
				       sizeof(SceNpCommunicationId));
			sce_paf_strncpy(outEntry->longTitleName, logEntry.longTitleName.c_str(),
					sizeof(outEntry->longTitleName));
			outEntry->longTitleName[sizeof(outEntry->longTitleName) - 1] = '\0';
			sce_paf_strncpy(outEntry->description, logEntry.description.c_str(),
					sizeof(outEntry->description));
			outEntry->description[sizeof(outEntry->description) - 1] = '\0';
			sce_paf_strncpy(outEntry->titleId, logEntry.titleId.c_str(),
					sizeof(outEntry->titleId));
			outEntry->titleId[sizeof(outEntry->titleId) - 1] = '\0';
		}
	}
	return ret;
}
