/*
 * np_imps_svc_presence.cpp: SceNpImpsSvc local user presence manager,
 * presence data object, in-game data message object, and platform parser
 * (0x812FC140..0x812FDB1F, 0x812FE0AC..0x812FF2E7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/new>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/stdio.h>
#include <std/string>
#include <rtc/rtc.h>
#include "np_imps_svc.h"

/* AppMgr / RegMgr / LSDB / VSH Kernel imports used by NpImpsSvcMyPresence */
struct NpImpsAppMgrStatus {
	SceUInt8 header[12];
	char titleId[40];
	SceUID processId;
	SceUInt8 middle[57];
	SceUInt8 isBigAppFlag;
	SceUInt8 tail[14];
};

extern "C" {
extern int sceAppMgrGetStatusByAppId(int appId, NpImpsAppMgrStatus *outStatus);
extern int sceAppMgrAppParamGetString(SceUID pid, int param, char *buf, SceSize size);
extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *outValue);
extern int vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *outSdkVersion);
extern int sceAppMgrIsDevelopmentMode(void);
}

/* SceLsdb lightweight handle & query imports (0x812FCA0E) */
struct NpImpsLsdbHandle {
	SceUInt32 opaque;
};

struct NpImpsLsdbRecord;

extern "C" {
extern void SceLsdb_A6D8772C(NpImpsLsdbHandle *handle);
extern int SceLsdb_E90B81CD(NpImpsLsdbHandle *handle);
extern int SceLsdb_D239031A(NpImpsLsdbHandle *handle, const paf::string *titleId,
			    NpImpsLsdbRecord **outRecord);
extern void SceLsdb_E2101B03(NpImpsLsdbHandle *handle);
extern const char *sceLsdbGetCategory(NpImpsLsdbRecord **record);
extern const char *sceLsdbGetTitle(NpImpsLsdbRecord **record);
}

static const char s_defaultPlatformTitle[] = "PlayStation\xC2\xAEVita";
static const char s_platformTagPsp2[4] = { 'p', 's', 'p', '2' };
static const char s_platformTagPs3[4] = { 'p', 's', '3', '\0' };
static const char s_platformTagEmpty[4] = { '\0', '\0', '\0', '\0' };

/* Duplicate a string onto the SceNpImpsCache heap */
static int npImpsDupCacheString(char **slot, const char *src)
{
	if (*slot != NULL) {
		npImpsCacheFree(*slot);
		*slot = NULL;
	}
	if (src != NULL) {
		SceSize allocLen = sce_paf_strlen(src) + 1;
		char *copy = reinterpret_cast<char *>(npImpsCacheMalloc(allocLen));
		if (copy == NULL)
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		sce_paf_memset(copy, 0, allocLen);
		sce_paf_strncpy(copy, src, allocLen);
		copy[allocLen - 1] = '\0';
		*slot = copy;
	}
	return 0;
}

/* Duplicate a binary buffer onto the SceNpImpsCache heap */
static int npImpsDupCacheBuffer(void **slot, const void *src, SceSize size)
{
	if (*slot != NULL) {
		npImpsCacheFree(*slot);
		*slot = NULL;
	}
	if (src != NULL && size != 0) {
		void *copy = npImpsCacheMalloc(size);
		if (copy == NULL)
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		sce_paf_memset(copy, 0, size);
		sce_paf_memcpy(copy, src, size);
		*slot = copy;
	}
	return 0;
}

/* =========================================================================
 * NpImpsSvcInGameDataMessage (0x812FE122..0x812FE337)
 * ========================================================================= */

NpImpsSvcInGameDataMessage::NpImpsSvcInGameDataMessage()
	: m_payload(NULL),
	  m_payloadSize(0)
{
	sce_paf_memset(&m_senderNpId, 0, sizeof(m_senderNpId));
	sce_paf_memset(&m_commId, 0, sizeof(m_commId));
}

/* 0x812FE2CC, 0x812FE300 */
NpImpsSvcInGameDataMessage::~NpImpsSvcInGameDataMessage()
{
	if (m_payload != NULL) {
		npImpsCacheFree(m_payload);
		m_payload = NULL;
	}
}

/* 0x812FE330 */
const SceNpCommunicationId *NpImpsSvcInGameDataMessage::GetCommId() const
{
	return &m_commId;
}

/* 0x812FE122 */
int NpImpsSvcInGameDataMessage::Create(NpImpsSvcInGameDataMessage **outMsg,
				       const SceNpId *senderNpId,
				       const SceNpCommunicationId *commId,
				       const void *payload, SceSize payloadSize)
{
	if (senderNpId == NULL || commId == NULL || payload == NULL || payloadSize == 0)
		return NP_IMPS_SVC_ERROR_INTERNAL;

	void *mem = npImpsCacheMalloc(sizeof(NpImpsSvcInGameDataMessage));
	NpImpsSvcInGameDataMessage *obj = NULL;
	if (mem != NULL) {
		obj = new (mem) NpImpsSvcInGameDataMessage();
		obj->AddRef();
	}

	if (*outMsg != obj) {
		if (*outMsg != NULL)
			(*outMsg)->Release();
		*outMsg = obj;
		if (obj != NULL)
			obj->AddRef();
	}
	if (obj != NULL)
		obj->Release();

	if (*outMsg == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	NpImpsSvcInGameDataMessage *msg = *outMsg;
	sce_paf_memcpy(&msg->m_senderNpId, senderNpId, sizeof(SceNpId));
	sce_paf_memcpy(&msg->m_commId, commId, sizeof(SceNpCommunicationId));
	msg->m_payloadSize = payloadSize;
	msg->m_payload = npImpsCacheMalloc(payloadSize);
	if (msg->m_payload == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;

	sce_paf_memset(msg->m_payload, 0, payloadSize);
	sce_paf_memcpy(msg->m_payload, payload, payloadSize);
	return 0;
}

/* =========================================================================
 * NpImpsSvcPresenceData (0x812FE338..0x812FF2E7)
 * ========================================================================= */

/* 0x812FE338 */
NpImpsSvcPresenceData::NpImpsSvcPresenceData()
	: m_onlineStatus(0),
	  m_platform(0),
	  m_comment(NULL),
	  m_titlePlatform(0),
	  m_lastOnlineTick(0),
	  m_hasTitle(0),
	  m_titleIdStr(NULL),
	  m_titleName(NULL),
	  m_isSystemTitle(0),
	  m_contentId(NULL),
	  m_hasCommId(0),
	  m_hasGamePresence(0),
	  m_gamePresenceType(0),
	  m_gameStatus(NULL),
	  m_gameData(NULL),
	  m_gameDataSize(0),
	  m_hasPartyPresence(0),
	  m_partyPresenceType(0),
	  m_partyTitle(NULL),
	  m_partyStatus(NULL),
	  m_partyData(NULL),
	  m_partyDataSize(0)
{
	sce_paf_memset(&m_titleId, 0, sizeof(m_titleId));
	sce_paf_memset(&m_joinableCommId, 0, sizeof(m_joinableCommId));
	sce_paf_memset(&m_commId, 0, sizeof(m_commId));
}

/* 0x812FE55C, 0x812FE5AA */
NpImpsSvcPresenceData::~NpImpsSvcPresenceData()
{
	m_onlineStatus = 0;
	m_platform = 0;
	if (m_comment != NULL) {
		npImpsCacheFree(m_comment);
		m_comment = NULL;
	}
	m_titlePlatform = 0;
	m_lastOnlineTick = 0;
	ClearTitleAndGamePresence();
	ClearPartyPresence();
}

/* 0x812FE3F6 */
void NpImpsSvcPresenceData::ClearPartyPresence()
{
	m_partyPresenceType = 0;
	if (m_partyTitle != NULL) {
		npImpsCacheFree(m_partyTitle);
		m_partyTitle = NULL;
	}
	if (m_partyStatus != NULL) {
		npImpsCacheFree(m_partyStatus);
		m_partyStatus = NULL;
	}
	if (m_partyData != NULL) {
		npImpsCacheFree(m_partyData);
		m_partyData = NULL;
	}
	m_partyDataSize = 0;
	m_hasPartyPresence = 0;
}

/* 0x812FE42E */
void NpImpsSvcPresenceData::ClearGamePresenceBody()
{
	m_gamePresenceType = 0;
	if (m_gameStatus != NULL) {
		npImpsCacheFree(m_gameStatus);
		m_gameStatus = NULL;
	}
	if (m_gameData != NULL) {
		npImpsCacheFree(m_gameData);
		m_gameData = NULL;
	}
	m_gameDataSize = 0;
	m_hasGamePresence = 0;
}

/* 0x812FE458 */
void NpImpsSvcPresenceData::ClearGamePresenceAll()
{
	ClearGamePresenceBody();
	sce_paf_memset(&m_commId, 0, sizeof(m_commId));
	m_hasCommId = 0;
}

/* 0x812FE492 */
void NpImpsSvcPresenceData::ClearJoinableCommId()
{
	sce_paf_memset(&m_joinableCommId, 0, sizeof(m_joinableCommId));
}

/* 0x812FE4A0 */
void NpImpsSvcPresenceData::ClearContentId()
{
	if (m_contentId != NULL) {
		npImpsCacheFree(m_contentId);
		m_contentId = NULL;
	}
}

/* 0x812FE4B2 */
void NpImpsSvcPresenceData::ClearTitleAndGamePresence()
{
	m_gamePresenceType = 0;
	sce_paf_memset(&m_titleId, 0, sizeof(m_titleId));
	if (m_titleIdStr != NULL) {
		npImpsCacheFree(m_titleIdStr);
		m_titleIdStr = NULL;
	}
	if (m_titleName != NULL) {
		npImpsCacheFree(m_titleName);
		m_titleName = NULL;
	}
	m_isSystemTitle = 0;
	ClearContentId();
	ClearJoinableCommId();
	ClearGamePresenceBody();
	m_hasTitle = 0;
	ClearGamePresenceAll();
}

/* 0x812FE5FE */
void NpImpsSvcPresenceData::FormatLastOnlineTickForLog() const
{
	char rfcBuf[64];
	paf::rtc::FormatRFC3339(rfcBuf, &m_lastOnlineTick, 0);
}

/* 0x812FE634 */
int NpImpsSvcPresenceData::SetComment(const char *comment)
{
	return npImpsDupCacheString(&m_comment, comment);
}

/* 0x812FE686 */
void NpImpsSvcPresenceData::SetLastOnlineTick(const paf::rtc::Tick *tick)
{
	m_lastOnlineTick = *tick;
}

/* 0x812FE690 */
int NpImpsSvcPresenceData::SetTitleInfo(const SceNpCommunicationId *titleId, const char *titleIdStr,
					const char *titleName, SceUInt8 isSystemTitle)
{
	sce_paf_memcpy(&m_titleId, titleId, sizeof(SceNpCommunicationId));
	int ret = npImpsDupCacheString(&m_titleIdStr, titleIdStr);
	if (ret < 0)
		return ret;
	ret = npImpsDupCacheString(&m_titleName, titleName);
	if (ret >= 0) {
		m_isSystemTitle = isSystemTitle;
		m_hasTitle = 1;
	}
	return ret;
}

/* 0x812FE754 */
int NpImpsSvcPresenceData::SetCommId(const SceNpCommunicationId *commId)
{
	sce_paf_memcpy(&m_commId, commId, sizeof(SceNpCommunicationId));
	m_hasCommId = 1;
	return 0;
}

/* 0x812FE76C */
int NpImpsSvcPresenceData::SetGamePresence(int presenceType, const char *status,
					   const void *gameData, SceSize gameDataSize)
{
	m_gamePresenceType = presenceType;
	int ret = npImpsDupCacheString(&m_gameStatus, status);
	if (ret < 0)
		return ret;
	ret = npImpsDupCacheBuffer(&m_gameData, gameData, gameDataSize);
	if (ret >= 0) {
		m_gameDataSize = gameDataSize;
		m_hasGamePresence = 1;
	}
	return ret;
}

/* 0x812FE814 */
int NpImpsSvcPresenceData::SetGamePresenceFromInput(const NpImpsGamePresenceInput *input)
{
	return SetGamePresence(input->presenceType, input->status, input->gameData, input->gameDataSize);
}

/* 0x812FE832 */
int NpImpsSvcPresenceData::SetPartyPresence(int presenceType, const char *title, const char *status,
					    const void *partyData, SceSize partyDataSize)
{
	m_partyPresenceType = presenceType;
	int ret = npImpsDupCacheString(&m_partyTitle, title);
	if (ret < 0)
		return ret;
	ret = npImpsDupCacheString(&m_partyStatus, status);
	if (ret < 0)
		return ret;
	ret = npImpsDupCacheBuffer(&m_partyData, partyData, partyDataSize);
	if (ret >= 0) {
		m_partyDataSize = partyDataSize;
		m_hasPartyPresence = 1;
	}
	return ret;
}

/* 0x812FE930 */
int NpImpsSvcPresenceData::SetPartyPresenceFromStruct(const NpImpsPartyPresence *party)
{
	return SetPartyPresence(party->presenceType, party->title, party->status,
				party->partyData, party->partyDataSize);
}

/* 0x812FE954 */
void NpImpsSvcPresenceData::SetJoinableCommId(const SceNpCommunicationId *commId)
{
	sce_paf_memcpy(&m_joinableCommId, commId, sizeof(SceNpCommunicationId));
}

/* 0x812FE9B4 */
int NpImpsSvcPresenceData::SetContentId(const char *contentId)
{
	return npImpsDupCacheString(&m_contentId, contentId);
}

/* 0x812FEA08 */
bool NpImpsSvcPresenceData::HasCommId() const
{
	return m_hasCommId != 0;
}

/* 0x812FEA10 */
bool NpImpsSvcPresenceData::HasPartyPresence() const
{
	return m_hasPartyPresence != 0;
}

/* 0x812FEA16 */
int NpImpsSvcPresenceData::GetEffectiveOnlineStatus() const
{
	if (m_onlineStatus == 1)
		return 1;
	if (m_onlineStatus == 2)
		return (m_platform == 1) ? 2 : 3;
	if (m_onlineStatus == 3)
		return 3;
	if (m_onlineStatus == 4)
		return 2;
	return 0;
}

/* 0x812FEA4C */
const SceNpCommunicationId *NpImpsSvcPresenceData::GetTitleId() const
{
	return &m_titleId;
}

/* 0x812FEA50 */
SceUInt8 NpImpsSvcPresenceData::IsSystemTitle() const
{
	return m_isSystemTitle;
}

/* 0x812FEA56 */
const SceNpCommunicationId *NpImpsSvcPresenceData::GetJoinableCommId() const
{
	return &m_joinableCommId;
}

/* 0x812FEA5A */
int NpImpsSvcPresenceData::GetGamePresenceType() const
{
	return m_gamePresenceType;
}

/* 0x812FEA5E */
const SceNpCommunicationId *NpImpsSvcPresenceData::GetCommId() const
{
	return &m_commId;
}

/* 0x812FEA6A */
int NpImpsSvcPresenceData::CopyGameData(void *dst, SceSize maxLen) const
{
	if (maxLen != 0 && dst == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	SceSize copyLen = (maxLen < m_gameDataSize) ? maxLen : m_gameDataSize;
	if (copyLen != 0)
		sce_paf_memcpy(dst, m_gameData, copyLen);
	return 0;
}

/* 0x812FEAA2 */
int NpImpsSvcPresenceData::CopyPartyData(void *dst, SceSize maxLen) const
{
	if (maxLen != 0 && dst == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	SceSize copyLen = (maxLen < m_partyDataSize) ? maxLen : m_partyDataSize;
	if (copyLen != 0)
		sce_paf_memcpy(dst, m_partyData, copyLen);
	return 0;
}

/* 0x812FEACE */
int NpImpsSvcPresenceData::Compare(const NpImpsSvcPresenceData *other,
				   NpImpsSvcPresenceDiffFlags *outDiff) const
{
	if (other == NULL)
		return NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;

	if (GetEffectiveOnlineStatus() != other->GetEffectiveOnlineStatus())
		outDiff->effectiveStatusChanged = 1;
	if (m_onlineStatus != other->m_onlineStatus)
		outDiff->onlineStatusChanged = 1;
	if (m_platform != other->m_platform)
		outDiff->platformChanged = 1;
	if (sce_paf_strcmp(m_comment, other->m_comment) != 0)
		outDiff->commentChanged = 1;
	if (m_titlePlatform != other->m_titlePlatform)
		outDiff->titlePlatformChanged = 1;
	if (m_lastOnlineTick != other->m_lastOnlineTick)
		outDiff->lastOnlineChanged = 1;

	if (sce_paf_memcmp(&m_titleId, &other->m_titleId, sizeof(SceNpCommunicationId)) != 0 ||
	    sce_paf_strcmp(m_titleIdStr, other->m_titleIdStr) != 0 ||
	    sce_paf_strcmp(m_titleName, other->m_titleName) != 0 ||
	    m_isSystemTitle != other->m_isSystemTitle) {
		outDiff->titleChanged = 1;
	}

	if (sce_paf_memcmp(&m_commId, &other->m_commId, sizeof(SceNpCommunicationId)) != 0)
		outDiff->commIdChanged = 1;

	if (m_gamePresenceType != other->m_gamePresenceType ||
	    sce_paf_strcmp(m_gameStatus, other->m_gameStatus) != 0 ||
	    m_gameDataSize != other->m_gameDataSize ||
	    (other->m_gameDataSize != 0 &&
	     sce_paf_memcmp(m_gameData, other->m_gameData, other->m_gameDataSize) != 0)) {
		outDiff->gamePresenceChanged = 1;
	}

	if (m_partyPresenceType != other->m_partyPresenceType ||
	    sce_paf_strcmp(m_partyTitle, other->m_partyTitle) != 0 ||
	    sce_paf_strcmp(m_partyStatus, other->m_partyStatus) != 0 ||
	    m_partyDataSize != other->m_partyDataSize ||
	    (other->m_partyDataSize != 0 &&
	     sce_paf_memcmp(m_partyData, other->m_partyData, other->m_partyDataSize) != 0)) {
		outDiff->partyPresenceChanged = 1;
	}

	if (sce_paf_memcmp(&m_joinableCommId, &other->m_joinableCommId,
			   sizeof(SceNpCommunicationId)) != 0 ||
	    sce_paf_strcmp(m_contentId, other->m_contentId) != 0) {
		outDiff->contentInfoChanged = 1;
	}

	return 0;
}

/* 0x812FEC6C */
static void npImpsApplyCorePlatform(NpImpsSvcPresenceData **target, int rawPlatform)
{
	NpImpsSvcPresenceData *dst = *target;
	switch (rawPlatform) {
	case 0:
		dst->SetPlatform(0);
		break;
	case 1:
		dst->SetPlatform(1);
		break;
	case 2:
		dst->SetPlatform(2);
		break;
	case 3:
		dst->SetPlatform(3);
		break;
	case 4:
		dst->SetPlatform(4);
		break;
	default:
		dst->SetPlatform(-1);
		break;
	}
}

/* 0x812FECB0 */
static void npImpsApplyCoreOnlineStatus(NpImpsSvcPresenceData **target, int rawStatus)
{
	NpImpsSvcPresenceData *dst = *target;
	if (rawStatus == 0)
		dst->SetOnlineStatus(2);
	else if (rawStatus == 1)
		dst->SetOnlineStatus(3);
	else if (rawStatus == 2)
		dst->SetOnlineStatus(4);
	else if (rawStatus == 3)
		dst->SetOnlineStatus(1);
	else
		dst->SetOnlineStatus(0);
}

static int npImpsMapCorePlatform(int rawPlatform)
{
	if (rawPlatform >= 0 && rawPlatform <= 4)
		return rawPlatform;
	return -1;
}

/* 0x812FECF4 */
static int npImpsPopulateGamePresenceFromCore(NpImpsSvcPresenceData *target,
					      const NpImpsCorePresenceUpdate *update)
{
	if (update == NULL)
		return 0;

	const SceNpCommunicationId *partyTitleId = npImpsCacheGetCore()->GetPartyTitleId();
	const NpImpsCoreTitleNode *gameTitle = update->titleList;
	while (gameTitle != NULL) {
		if (sce_paf_memcmp(&gameTitle->titleId, partyTitleId,
				   sizeof(SceNpCommunicationId)) != 0)
			break;
		gameTitle = gameTitle->next;
	}

	int gamePresenceType = 0;
	if (gameTitle != NULL) {
		target->SetTitlePlatform(npImpsMapCorePlatform(gameTitle->platform));
		int ret = target->SetTitleInfo(&gameTitle->titleId, gameTitle->titleIdStr,
					       gameTitle->titleName, gameTitle->isSystemTitle);
		if (ret < 0)
			return ret;

		gamePresenceType = npImpsMapCorePlatform(gameTitle->presenceType);
		ret = target->SetContentId(gameTitle->contentId);
		if (ret < 0)
			return ret;

		target->SetJoinableCommId(&gameTitle->joinableCommId);
	}

	const NpImpsCoreDataNode *gameData = update->dataList;
	while (gameData != NULL) {
		if (sce_paf_memcmp(&gameData->commId, partyTitleId,
				   sizeof(SceNpCommunicationId)) != 0)
			break;
		gameData = gameData->next;
	}

	if (gameData != NULL)
		target->SetCommId(&gameData->commId);

	if (gameTitle != NULL) {
		if (gameData != NULL) {
			return target->SetGamePresence(gamePresenceType, gameTitle->status,
						       gameData->dataPtr, gameData->dataSize);
		}
		return target->SetGamePresence(gamePresenceType, gameTitle->status, NULL, 0);
	}
	if (gameData != NULL)
		return NP_IMPS_SVC_ERROR_INTERNAL;
	return 0;
}

/* 0x812FEE6A */
int NpImpsSvcPresenceData::CreateFromCoreUpdate(NpImpsSvcPresenceData **outData,
						const NpImpsCorePresenceUpdate *update)
{
	void *mem = npImpsCacheMalloc(sizeof(NpImpsSvcPresenceData));
	NpImpsSvcPresenceData *obj = NULL;
	if (mem != NULL) {
		obj = new (mem) NpImpsSvcPresenceData();
		obj->AddRef();
	}

	if (*outData != obj) {
		if (*outData != NULL)
			(*outData)->Release();
		*outData = obj;
		if (obj != NULL)
			obj->AddRef();
	}
	if (obj != NULL)
		obj->Release();

	if (*outData == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	if (update == NULL)
		return 0;

	NpImpsSvcPresenceData *dst = *outData;
	npImpsApplyCoreOnlineStatus(outData, update->onlineStatus);
	npImpsApplyCorePlatform(outData, update->platform);

	int ret = dst->SetComment(update->comment);
	if (ret < 0)
		return ret;

	ret = npImpsPopulateGamePresenceFromCore(dst, update);
	if (ret < 0)
		return ret;

	const SceNpCommunicationId *partyTitleId = npImpsCacheGetCore()->GetPartyTitleId();
	const NpImpsCoreTitleNode *partyTitle = update->titleList;
	while (partyTitle != NULL) {
		if (sce_paf_memcmp(&partyTitle->titleId, partyTitleId,
				   sizeof(SceNpCommunicationId)) == 0)
			break;
		partyTitle = partyTitle->next;
	}

	const NpImpsCoreDataNode *partyData = update->dataList;
	while (partyData != NULL) {
		if (sce_paf_memcmp(&partyData->commId, partyTitleId,
				   sizeof(SceNpCommunicationId)) == 0)
			break;
		partyData = partyData->next;
	}

	if (partyTitle != NULL) {
		int partyType = (partyTitle->presenceType >= 0 && partyTitle->presenceType <= 2)
			? partyTitle->presenceType : -1;
		if (partyData != NULL) {
			ret = dst->SetPartyPresence(partyType, partyTitle->titleName,
						    partyTitle->status,
						    partyData->dataPtr, partyData->dataSize);
		} else {
			ret = dst->SetPartyPresence(partyType, partyTitle->titleName,
						    partyTitle->status, NULL, 0);
		}
	} else if (partyData != NULL) {
		ret = NP_IMPS_SVC_ERROR_INTERNAL;
	}

	if (ret >= 0 && update->lastOnlineTick != NULL)
		dst->SetLastOnlineTick(update->lastOnlineTick);

	return ret;
}

/* 0x812FF050 */
int NpImpsSvcPresenceData::CopyFrom(const NpImpsSvcPresenceData *src)
{
	SetOnlineStatus(src->GetOnlineStatus());
	SetPlatform(src->GetPlatform());
	SetTitlePlatform(src->GetTitlePlatform());
	SetLastOnlineTick(src->GetLastOnlineTick());

	int ret = SetComment(src->GetComment());
	if (ret < 0)
		return ret;

	if (src->HasTitle()) {
		ret = SetTitleInfo(src->GetTitleId(), src->GetTitleIdStr(),
				   src->GetTitleName(), src->IsSystemTitle());
		if (ret < 0)
			return ret;
	}

	if (src->HasCommId()) {
		ret = SetCommId(src->GetCommId());
		if (ret < 0)
			return ret;
	}

	if (src->HasGamePresence()) {
		SceSize gameLen = src->GetGameDataSize();
		void *gameCopy = NULL;
		if (gameLen != 0) {
			gameCopy = npImpsCacheMalloc(gameLen);
			if (gameCopy == NULL)
				return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			sce_paf_memset(gameCopy, 0, gameLen);
			ret = src->CopyGameData(gameCopy, gameLen);
		}
		if (ret >= 0) {
			ret = SetGamePresence(src->GetGamePresenceType(), src->GetGameStatus(),
					      gameCopy, gameLen);
		}
		if (gameCopy != NULL)
			npImpsCacheFree(gameCopy);
		if (ret < 0)
			return ret;
	}

	if (src->HasPartyPresence()) {
		SceSize partyLen = src->GetPartyDataSize();
		void *partyCopy = NULL;
		if (partyLen != 0) {
			partyCopy = npImpsCacheMalloc(partyLen);
			if (partyCopy == NULL)
				return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
			sce_paf_memset(partyCopy, 0, partyLen);
			ret = src->CopyPartyData(partyCopy, partyLen);
		}
		if (ret >= 0) {
			ret = SetPartyPresence(src->GetPartyPresenceType(), src->GetPartyTitle(),
					       src->GetPartyStatus(), partyCopy, partyLen);
		}
		if (partyCopy != NULL)
			npImpsCacheFree(partyCopy);
		if (ret < 0)
			return ret;
	}

	ret = SetContentId(src->GetContentId());
	if (ret >= 0)
		SetJoinableCommId(src->GetJoinableCommId());
	return ret;
}

/* 0x812FF23C, 0x812FF292 */
int NpImpsSvcPresenceData::Clone(NpImpsSvcPresenceData **outData,
				 const NpImpsSvcPresenceData *src)
{
	void *mem = npImpsCacheMalloc(sizeof(NpImpsSvcPresenceData));
	NpImpsSvcPresenceData *obj = NULL;
	if (mem != NULL) {
		obj = new (mem) NpImpsSvcPresenceData();
		obj->AddRef();
	}

	if (*outData != obj) {
		if (*outData != NULL)
			(*outData)->Release();
		*outData = obj;
		if (obj != NULL)
			obj->AddRef();
	}
	if (obj != NULL)
		obj->Release();

	if (*outData == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	return (*outData)->CopyFrom(src);
}

/* 0x812FE0D0: platform tag classifier */
struct NpImpsPlatformTagRecord {
	SceUInt8 header[24];
	char platformTag[4];
};

int npImpsSvcParsePlatformTag(const void *record)
{
	const NpImpsPlatformTagRecord *rec =
		reinterpret_cast<const NpImpsPlatformTagRecord *>(record);
	if (sce_paf_memcmp(rec->platformTag, s_platformTagPsp2, 4) == 0)
		return NP_IMPS_SVC_PLATFORM_PSP2;
	if (sce_paf_memcmp(rec->platformTag, s_platformTagPs3, 4) == 0)
		return NP_IMPS_SVC_PLATFORM_PS3;
	if (sce_paf_memcmp(rec->platformTag, s_platformTagEmpty, 4) == 0)
		return NP_IMPS_SVC_PLATFORM_UNSPECIFIED;
	return -1;
}

/* =========================================================================
 * NpImpsSvcMyPresence (0x812FC140..0x812FDAFF)
 * ========================================================================= */

/* 0x812FC140 */
NpImpsSvcMyPresence::NpImpsSvcMyPresence(NpImpsSvcMyPresenceObserver *observer)
	: m_mutex("SceNpImpsSvcMyPresence", 0),
	  m_processId(0),
	  m_appId(-1),
	  m_titleLoaded(0),
	  m_presence(),
	  m_observer(observer)
{
}

/* 0x812FC184 */
NpImpsSvcMyPresence::~NpImpsSvcMyPresence()
{
}

/* 0x812FC1D6 */
int NpImpsSvcMyPresence::GetSnapshot(NpImpsSvcPresenceSnapshot *outSnapshot)
{
	m_mutex.Lock();
	int ret = 0;
	if (outSnapshot == NULL) {
		ret = NP_IMPS_SVC_ERROR_INVALID_ARGUMENT;
	} else {
		outSnapshot->onlineStatus = m_presence.GetOnlineStatus();
		outSnapshot->titlePlatform = m_presence.GetTitlePlatform();
		if (m_presence.GetTitleName() != NULL) {
			sce_paf_strncpy(outSnapshot->titleName, m_presence.GetTitleName(),
					sizeof(outSnapshot->titleName));
		}
		outSnapshot->titleName[sizeof(outSnapshot->titleName) - 1] = '\0';

		sce_paf_memcpy(&outSnapshot->titleId, m_presence.GetTitleId(),
			       sizeof(SceNpCommunicationId));
		if (m_presence.GetContentId() != NULL) {
			sce_paf_memcpy(outSnapshot->contentId, m_presence.GetContentId(),
				       sizeof(outSnapshot->contentId));
		}
		sce_paf_memcpy(&outSnapshot->joinableCommId, m_presence.GetJoinableCommId(),
			       sizeof(SceNpCommunicationId));
		sce_paf_memcpy(&outSnapshot->commId, m_presence.GetCommId(),
			       sizeof(SceNpCommunicationId));

		outSnapshot->gamePresence.size = 0;
		outSnapshot->gamePresence.presenceType = m_presence.GetGamePresenceType();
		if (m_presence.GetGameStatus() != NULL) {
			sce_paf_strncpy(outSnapshot->gamePresence.status,
					m_presence.GetGameStatus(),
					sizeof(outSnapshot->gamePresence.status));
		} else {
			sce_paf_strncpy(outSnapshot->gamePresence.status, "",
					sizeof(outSnapshot->gamePresence.status));
		}
		outSnapshot->gamePresence.status[sizeof(outSnapshot->gamePresence.status) - 1] = '\0';

		ret = m_presence.CopyGameData(outSnapshot->gamePresence.gameData,
					      sizeof(outSnapshot->gamePresence.gameData));
		if (ret >= 0) {
			outSnapshot->gamePresence.gameDataSize = m_presence.GetGameDataSize();
			outSnapshot->partyPresence.size = sizeof(NpImpsPartyPresence);
			outSnapshot->partyPresence.presenceType = m_presence.GetPartyPresenceType();
			if (m_presence.GetPartyTitle() != NULL) {
				sce_paf_strncpy(outSnapshot->partyPresence.title,
						m_presence.GetPartyTitle(),
						sizeof(outSnapshot->partyPresence.title));
			}
			outSnapshot->partyPresence.title[sizeof(outSnapshot->partyPresence.title) - 1] = '\0';
			if (m_presence.GetPartyStatus() != NULL) {
				sce_paf_strncpy(outSnapshot->partyPresence.status,
						m_presence.GetPartyStatus(),
						sizeof(outSnapshot->partyPresence.status));
			}
			outSnapshot->partyPresence.status[sizeof(outSnapshot->partyPresence.status) - 1] = '\0';
			ret = m_presence.CopyPartyData(outSnapshot->partyPresence.partyData,
						       sizeof(outSnapshot->partyPresence.partyData));
			if (ret >= 0)
				outSnapshot->partyPresence.partyDataSize = m_presence.GetPartyDataSize();
		}
	}
	m_mutex.Unlock();
	return ret;
}

/* 0x812FC43C */
int NpImpsSvcMyPresence::BuildPartyPresenceNodes(NpImpsCorePresenceUpdate *update,
						 SceUInt8 prioritize)
{
	if (!m_presence.HasPartyPresence())
		return 0;

	NpImpsCoreTitleNode **titleLink = &update->titleList;
	while (*titleLink != NULL)
		titleLink = &(*titleLink)->next;

	NpImpsCoreTitleNode *titleNode = reinterpret_cast<NpImpsCoreTitleNode *>(
		npImpsSvcMalloc(sizeof(NpImpsCoreTitleNode)));
	if (titleNode == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	sce_paf_memset(titleNode, 0, sizeof(NpImpsCoreTitleNode));
	*titleLink = titleNode;

	const SceNpCommunicationId *partyTitleId = npImpsCacheGetCore()->GetPartyTitleId();
	sce_paf_memcpy(&titleNode->titleId, partyTitleId, sizeof(SceNpCommunicationId));
	if (m_presence.GetPartyTitle() != NULL)
		sce_paf_strncpy(titleNode->titleName, m_presence.GetPartyTitle(),
				sizeof(titleNode->titleName));
	if (m_presence.GetPartyStatus() != NULL)
		sce_paf_strncpy(titleNode->status, m_presence.GetPartyStatus(),
				sizeof(titleNode->status));

	int ptype = m_presence.GetPartyPresenceType();
	titleNode->presenceType = (ptype >= 0 && ptype <= 2) ? ptype : -1;
	titleNode->platform = NP_IMPS_SVC_PLATFORM_PSP2;
	if (prioritize != 0)
		titleNode->updatePriority = 2;

	NpImpsCoreDataNode **dataLink = &update->dataList;
	while (*dataLink != NULL)
		dataLink = &(*dataLink)->next;

	NpImpsCoreDataNode *dataNode = reinterpret_cast<NpImpsCoreDataNode *>(
		npImpsSvcMalloc(sizeof(NpImpsCoreDataNode)));
	if (dataNode == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	sce_paf_memset(dataNode, 0, sizeof(NpImpsCoreDataNode));
	*dataLink = dataNode;

	sce_paf_memcpy(&dataNode->commId, partyTitleId, sizeof(SceNpCommunicationId));
	sce_paf_memcpy(&dataNode->targetCommId, partyTitleId, sizeof(SceNpCommunicationId));

	SceSize partyLen = m_presence.GetPartyDataSize();
	if (partyLen != 0) {
		dataNode->dataPtr = npImpsSvcMalloc(partyLen);
		if (dataNode->dataPtr == NULL)
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		sce_paf_memset(dataNode->dataPtr, 0, partyLen);
	}
	int ret = m_presence.CopyPartyData(dataNode->dataPtr, partyLen);
	if (ret >= 0) {
		dataNode->dataSize = partyLen;
		dataNode->valid = 1;
	}
	return ret;
}

/* 0x812FC5EA */
int NpImpsSvcMyPresence::BuildGamePresenceNodes(NpImpsCorePresenceUpdate *update,
						SceUInt8 prioritize)
{
	if (!m_presence.HasTitle() || !m_presence.HasCommId())
		return 0;

	NpImpsCoreDataNode **dataLink = &update->dataList;
	while (*dataLink != NULL)
		dataLink = &(*dataLink)->next;

	NpImpsCoreDataNode *dataNode = reinterpret_cast<NpImpsCoreDataNode *>(
		npImpsSvcMalloc(sizeof(NpImpsCoreDataNode)));
	if (dataNode == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	sce_paf_memset(dataNode, 0, sizeof(NpImpsCoreDataNode));
	*dataLink = dataNode;

	sce_paf_memcpy(&dataNode->commId, m_presence.GetCommId(), sizeof(SceNpCommunicationId));
	sce_paf_memcpy(&dataNode->targetCommId, m_presence.GetCommId(), sizeof(SceNpCommunicationId));

	SceSize gameLen = m_presence.GetGameDataSize();
	if (gameLen != 0) {
		dataNode->dataPtr = npImpsSvcMalloc(gameLen);
		if (dataNode->dataPtr == NULL)
			return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
		sce_paf_memset(dataNode->dataPtr, 0, gameLen);
	}

	int ret = m_presence.CopyGameData(dataNode->dataPtr, gameLen);
	if (ret < 0)
		return ret;

	dataNode->dataSize = gameLen;
	dataNode->valid = 1;

	NpImpsCoreTitleNode *titleNode = update->titleList;
	if (titleNode == NULL ||
	    sce_paf_memcmp(&titleNode->titleId, m_presence.GetTitleId(),
			   sizeof(SceNpCommunicationId)) != 0) {
		return NP_IMPS_SVC_ERROR_INTERNAL;
	}

	if (m_presence.GetGameStatus() != NULL)
		sce_paf_strncpy(titleNode->status, m_presence.GetGameStatus(),
				sizeof(titleNode->status));

	int gtype = m_presence.GetGamePresenceType();
	titleNode->presenceType = (gtype >= 0 && gtype <= 4) ? gtype : -1;

	if (gtype == 2 || gtype == 4) {
		sce_paf_memcpy(&titleNode->joinableCommId, m_presence.GetJoinableCommId(),
			       sizeof(SceNpCommunicationId));
	}
	if (m_presence.GetContentId() != NULL && sce_paf_strlen(m_presence.GetContentId()) != 0) {
		sce_paf_strncpy(titleNode->contentId, m_presence.GetContentId(),
				sizeof(titleNode->contentId));
		titleNode->contentId[sizeof(titleNode->contentId) - 1] = '\0';
	}
	if (prioritize != 0)
		titleNode->updatePriority = 2;

	return 0;
}

/* 0x812FC82C */
int NpImpsSvcMyPresence::BuildTitleNode(NpImpsCorePresenceUpdate *update)
{
	if (!m_presence.HasTitle())
		return 0;

	NpImpsCoreTitleNode **titleLink = &update->titleList;
	while (*titleLink != NULL)
		titleLink = &(*titleLink)->next;

	NpImpsCoreTitleNode *titleNode = reinterpret_cast<NpImpsCoreTitleNode *>(
		npImpsSvcMalloc(sizeof(NpImpsCoreTitleNode)));
	if (titleNode == NULL)
		return NP_IMPS_SVC_ERROR_OUT_OF_MEMORY;
	sce_paf_memset(titleNode, 0, sizeof(NpImpsCoreTitleNode));
	*titleLink = titleNode;

	sce_paf_memcpy(&titleNode->titleId, m_presence.GetTitleId(), sizeof(SceNpCommunicationId));
	if (m_presence.GetTitleName() != NULL)
		sce_paf_strncpy(titleNode->titleName, m_presence.GetTitleName(),
				sizeof(titleNode->titleName));
	titleNode->platform = NP_IMPS_SVC_PLATFORM_PSP2;
	return 0;
}

/* 0x812FC8AC */
int NpImpsSvcMyPresence::BuildCoreUpdate(NpImpsCorePresenceUpdate *update, int priorityMode)
{
	int ret = BuildTitleNode(update);
	if (ret < 0)
		return ret;

	if (priorityMode == 0) {
		ret = BuildGamePresenceNodes(update, 1);
		if (ret >= 0)
			ret = BuildPartyPresenceNodes(update, 0);
	} else if (priorityMode == 1) {
		ret = BuildGamePresenceNodes(update, 0);
		if (ret >= 0)
			ret = BuildPartyPresenceNodes(update, 1);
	}
	return ret;
}

void NpImpsSvcMyPresence::FreeCoreUpdate(NpImpsCorePresenceUpdate *update)
{
	NpImpsCoreTitleNode *titleNode = update->titleList;
	while (titleNode != NULL) {
		NpImpsCoreTitleNode *next = titleNode->next;
		npImpsSvcFree(titleNode);
		titleNode = next;
	}
	update->titleList = NULL;

	NpImpsCoreDataNode *dataNode = update->dataList;
	while (dataNode != NULL) {
		NpImpsCoreDataNode *next = dataNode->next;
		if (dataNode->dataPtr != NULL)
			npImpsSvcFree(dataNode->dataPtr);
		npImpsSvcFree(dataNode);
		dataNode = next;
	}
	update->dataList = NULL;
}

void NpImpsSvcMyPresence::NotifyObserver(NpImpsSvcPresenceData *curPresence,
					 NpImpsSvcPresenceData *prevPresence)
{
	if (curPresence != NULL)
		curPresence->AddRef();
	if (prevPresence != NULL)
		prevPresence->AddRef();

	NpImpsSvcPresenceData *curArg = curPresence;
	NpImpsSvcPresenceData *prevArg = prevPresence;
	if (curArg != NULL)
		curArg->AddRef();
	if (prevArg != NULL)
		prevArg->AddRef();

	m_observer->OnMyPresenceUpdated(&curArg, &prevArg);

	if (prevArg != NULL)
		prevArg->Release();
	if (curArg != NULL)
		curArg->Release();
	if (prevPresence != NULL)
		prevPresence->Release();
	if (curPresence != NULL)
		curPresence->Release();
}

/* 0x812FC900 */
int NpImpsSvcMyPresence::PublishCurrentPresence()
{
	m_mutex.Lock();
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	int ret = BuildCoreUpdate(&update, 0);
	if (ret >= 0)
		ret = npImpsCoreUpdatePresence(&update);

	FreeCoreUpdate(&update);
	m_mutex.Unlock();
	return ret;
}

/* 0x812FC98C */
int NpImpsSvcMyPresence::CheckAppStatusIsBigAppGame(const void *appStatus)
{
	const NpImpsAppMgrStatus *status =
		reinterpret_cast<const NpImpsAppMgrStatus *>(appStatus);
	char titleId[10];
	char category[4];

	sce_paf_memset(titleId, 0, sizeof(titleId));
	if (sceAppMgrAppParamGetString(status->processId, 12, titleId, sizeof(titleId)) >= 0 &&
	    sce_paf_strcmp("NPXS10028", titleId) == 0) {
		return 1;
	}
	if (sceAppMgrAppParamGetString(status->processId, 8, category, sizeof(category)) >= 0 &&
	    sce_paf_strcmp("gd", category) == 0 &&
	    status->isBigAppFlag == 1) {
		return 1;
	}
	return 0;
}

/* 0x812FCA0E */
int NpImpsSvcMyPresence::GetAppTitleInfo(int appId, NpImpsAppTitleInfo *outInfo)
{
	NpImpsLsdbHandle lsdb;
	NpImpsLsdbRecord *record = NULL;
	NpImpsAppMgrStatus status;
	int language = 0;

	SceLsdb_A6D8772C(&lsdb);
	sce_paf_memset(outInfo, 0, sizeof(NpImpsAppTitleInfo));
	sce_paf_memset(&status, 0, sizeof(status));

	int ret = sceAppMgrGetStatusByAppId(appId, &status);
	if (ret >= 0) {
		sce_paf_memcpy(outInfo->titleId, status.titleId, 9);
		outInfo->titleId[9] = '\0';

		ret = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", &language);
		if (ret >= 0) {
			ret = SceLsdb_E90B81CD(&lsdb);
			if (ret >= 0) {
				if (sce_paf_strcmp("NPXS10062", outInfo->titleId) != 0) {
					paf::string titleIdStr(outInfo->titleId);
					ret = SceLsdb_D239031A(&lsdb, &titleIdStr, &record);
				}

				const char *category = sceLsdbGetCategory(&record);
				char paramTitleId[10];
				sce_paf_memset(paramTitleId, 0, sizeof(paramTitleId));

				bool useLsdbTitle =
					(category != NULL &&
					 (sce_paf_strcmp("mb", category) == 0 ||
					  sce_paf_strcmp("mba", category) == 0)) ||
					(sceAppMgrAppParamGetString(status.processId, 12,
								    paramTitleId,
								    sizeof(paramTitleId)) >= 0 &&
					 sce_paf_strcmp("NPXS10028", paramTitleId) == 0);

				if (useLsdbTitle) {
					const char *lsdbTitle = sceLsdbGetTitle(&record);
					sce_paf_snprintf(outInfo->longTitleName,
							 sizeof(outInfo->longTitleName),
							 "%s", lsdbTitle != NULL ? lsdbTitle : "");
				} else {
					ret = sceAppMgrAppParamGetString(status.processId, 9,
									 outInfo->longTitleName,
									 sizeof(outInfo->longTitleName));
					if (ret < 0) {
						sce_paf_strncpy(outInfo->longTitleName,
								s_defaultPlatformTitle,
								sizeof(outInfo->longTitleName));
						ret = 0;
					}
				}

				if (sce_paf_strcmp("", outInfo->longTitleName) == 0) {
					sce_paf_strncpy(outInfo->longTitleName,
							s_defaultPlatformTitle,
							sizeof(outInfo->longTitleName));
				}
				outInfo->isBigAppGame =
					static_cast<SceUInt8>(CheckAppStatusIsBigAppGame(&status));
			}
		}
	}

	SceLsdb_E2101B03(&lsdb);
	return ret;
}

/* 0x812FCCB0 */
void NpImpsSvcMyPresence::EnsureTitleLoaded()
{
	NpImpsAppTitleInfo titleInfo;
	char contentId[48];

	sce_paf_memset(&titleInfo, 0, sizeof(titleInfo));
	sce_paf_memset(contentId, 0, sizeof(contentId));

	if (m_processId != 0 && m_titleLoaded == 0) {
		SceNpCommunicationId titleCommId;
		sce_paf_memset(&titleCommId, 0, sizeof(titleCommId));

		if (GetAppTitleInfo(m_appId, &titleInfo) >= 0) {
			sce_paf_memcpy(titleCommId.data, titleInfo.titleId, 9);
			if (m_presence.SetTitleInfo(&titleCommId, "", titleInfo.longTitleName, 0) >= 0) {
				sceAppMgrAppParamGetString(m_processId, 6, contentId, sizeof(contentId));
				if (sce_paf_strlen(contentId) != 0)
					m_presence.SetContentId(contentId);
				m_titleLoaded = 1;
			}
		}
	}
}

/* 0x812FCD88 */
int NpImpsSvcMyPresence::SetCommId(SceUID processId, const SceNpCommunicationId *commId)
{
	m_mutex.Lock();
	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;
	int ret;

	if (m_processId != processId) {
		ret = NP_IMPS_SVC_ERROR_WRONG_PROCESS;
	} else {
		ret = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
		if (ret >= 0) {
			EnsureTitleLoaded();
			ret = m_presence.SetCommId(commId);
			if (ret >= 0) {
				ret = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
				if (ret >= 0)
					NotifyObserver(curPresence, prevPresence);
			}
		}
	}

	if (prevPresence != NULL)
		prevPresence->Release();
	if (curPresence != NULL)
		curPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FCE96 */
int NpImpsSvcMyPresence::ClearCommId()
{
	m_mutex.Lock();
	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;

	int ret = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
	m_presence.ClearGamePresenceAll();
	int curRet = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
	if (curRet < 0 && ret == 0)
		ret = curRet;

	if (ret == 0)
		NotifyObserver(curPresence, prevPresence);

	if (prevPresence != NULL)
		prevPresence->Release();
	if (curPresence != NULL)
		curPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FCF80 */
bool NpImpsSvcMyPresence::HasCommId() const
{
	return m_presence.HasCommId();
}

/* 0x812FCF8A */
void NpImpsSvcMyPresence::SetActiveProcess(SceUID processId, int appId)
{
	if (m_processId == 0) {
		m_processId = processId;
		m_appId = appId;
	}
}

/* 0x812FCF96 */
int NpImpsSvcMyPresence::RepublishWithoutCommId()
{
	m_mutex.Lock();
	SceNpCommunicationId savedCommId = *m_presence.GetCommId();
	m_presence.ClearGamePresenceAll();

	int ret = 0;
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	int buildRet = BuildCoreUpdate(&update, 0);
	if (buildRet >= 0) {
		int pubRet = npImpsCoreUpdatePresence(&update);
		if (pubRet < 0)
			ret = pubRet;
	} else {
		ret = buildRet;
	}

	int restoreRet = m_presence.SetCommId(&savedCommId);
	if (restoreRet < 0 && ret == 0)
		ret = restoreRet;

	NpImpsCoreTitleNode *titleNode = update.titleList;
	while (titleNode != NULL) {
		NpImpsCoreTitleNode *nextTitle = titleNode->next;
		npImpsSvcFree(titleNode);
		titleNode = nextTitle;
	}

	NpImpsCoreDataNode *dataNode = update.dataList;
	while (dataNode != NULL) {
		NpImpsCoreDataNode *nextData = dataNode->next;
		if (dataNode->dataPtr != NULL)
			npImpsSvcFree(dataNode->dataPtr);
		npImpsSvcFree(dataNode);
		dataNode = nextData;
	}

	m_mutex.Unlock();
	return ret;
}

/* 0x812FD0A8 */
int NpImpsSvcMyPresence::SetGameTitle(int appId)
{
	m_mutex.Lock();
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;

	int ret = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
	if (ret >= 0) {
		NpImpsAppMgrStatus status;
		sce_paf_memset(&status, 0, sizeof(status));
		ret = sceAppMgrGetStatusByAppId(appId, &status);
		if (ret >= 0) {
			if (m_processId != status.processId) {
				ret = NP_IMPS_SVC_ERROR_WRONG_PROCESS;
			} else if (m_processId == 0 || m_titleLoaded == 0) {
				EnsureTitleLoaded();
				ret = BuildCoreUpdate(&update, 0);
				if (ret >= 0) {
					ret = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
					if (ret >= 0) {
						ret = npImpsCoreUpdatePresence(&update);
						NotifyObserver(curPresence, prevPresence);
					}
				}
			}
		}
	}

	FreeCoreUpdate(&update);
	if (curPresence != NULL)
		curPresence->Release();
	if (prevPresence != NULL)
		prevPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FD21E */
int NpImpsSvcMyPresence::ClearGameTitle()
{
	m_mutex.Lock();
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;

	int ret = 0;
	int stepRet = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
	if (stepRet < 0)
		ret = stepRet;

	m_presence.ClearTitleAndGamePresence();

	stepRet = BuildCoreUpdate(&update, 0);
	if (stepRet < 0 && ret == 0)
		ret = stepRet;

	stepRet = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
	if (stepRet < 0 && ret == 0)
		ret = stepRet;

	stepRet = npImpsCoreUpdatePresence(&update);
	if (stepRet < 0 && ret == 0)
		ret = stepRet;

	m_processId = 0;
	m_appId = -1;
	m_titleLoaded = 0;

	if (ret == 0)
		NotifyObserver(curPresence, prevPresence);

	FreeCoreUpdate(&update);
	if (prevPresence != NULL)
		prevPresence->Release();
	if (curPresence != NULL)
		curPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FD486 */
int NpImpsSvcMyPresence::SetGamePresence(SceUID processId, const NpImpsGamePresenceInput *input)
{
	m_mutex.Lock();
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;
	int ret = 0;

	if (m_processId != processId) {
		ret = NP_IMPS_SVC_ERROR_WRONG_PROCESS;
	} else {
		ret = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
		if (ret >= 0) {
			if (!m_presence.HasCommId()) {
				ret = NP_IMPS_SVC_ERROR_NO_COMM_ID;
			} else {
				int ptype = input->presenceType;
				if (ptype == 2 || ptype == 3 || ptype == 4) {
					SceNpCommunicationId joinableId;
					SceNpCommunicationId emptyId;
					char rawCommId[13];
					char numStr[4];

					sce_paf_memset(&joinableId, 0, sizeof(joinableId));
					sce_paf_memset(&emptyId, 0, sizeof(emptyId));
					sce_paf_memset(rawCommId, 0, sizeof(rawCommId));
					sce_paf_memset(numStr, 0, sizeof(numStr));

					if (sceAppMgrAppParamGetString(processId, 7, rawCommId,
								       sizeof(rawCommId)) >= 0) {
						sce_paf_memcpy(joinableId.data, rawCommId, 9);
						sce_paf_strncpy(numStr, &rawCommId[10], 3);
						joinableId.num = static_cast<char>(sce_paf_atoi(numStr));
					}

					if (sce_paf_memcmp(&joinableId, &emptyId,
							   sizeof(SceNpCommunicationId)) != 0) {
						m_presence.SetJoinableCommId(&joinableId);
					} else {
						SceUInt32 sdkVersion = 0;
						ret = vshKernelGetCompiledSdkVersionByPid(processId,
											  &sdkVersion);
						if (ret >= 0) {
							if (sdkVersion >= 0x01500000U &&
							    sceAppMgrIsDevelopmentMode() == 0) {
								ret = NP_IMPS_SVC_ERROR_NO_JOINABLE_COMM_ID;
							} else {
								m_presence.SetJoinableCommId(
									m_presence.GetCommId());
							}
						}
					}
				} else {
					m_presence.ClearJoinableCommId();
				}

				if (ret >= 0)
					ret = m_presence.SetGamePresenceFromInput(input);
				if (ret >= 0)
					ret = BuildCoreUpdate(&update, 0);
				if (ret >= 0)
					ret = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
				if (ret >= 0) {
					ret = npImpsCoreUpdatePresence(&update);
					if (ret == 0)
						NotifyObserver(curPresence, prevPresence);
				}
			}
		}
	}

	FreeCoreUpdate(&update);
	if (curPresence != NULL)
		curPresence->Release();
	if (prevPresence != NULL)
		prevPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FD6CE */
int NpImpsSvcMyPresence::ClearGamePresence()
{
	m_mutex.Lock();
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;

	int ret = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
	if (ret >= 0) {
		m_presence.ClearGamePresenceBody();
		m_presence.ClearContentId();
		m_presence.ClearJoinableCommId();
		ret = BuildCoreUpdate(&update, 0);
		if (ret >= 0) {
			ret = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
			if (ret >= 0) {
				ret = npImpsCoreUpdatePresence(&update);
				NotifyObserver(curPresence, prevPresence);
			}
		}
	}

	FreeCoreUpdate(&update);
	if (curPresence != NULL)
		curPresence->Release();
	if (prevPresence != NULL)
		prevPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FD806 */
int NpImpsSvcMyPresence::SetPartyPresence(const NpImpsPartyPresence *party)
{
	m_mutex.Lock();
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;

	int ret = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
	if (ret >= 0) {
		ret = m_presence.SetPartyPresenceFromStruct(party);
		if (ret >= 0) {
			ret = BuildCoreUpdate(&update, 1);
			if (ret >= 0) {
				ret = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
				if (ret >= 0) {
					ret = npImpsCoreUpdatePresence(&update);
					if (ret == 0)
						NotifyObserver(curPresence, prevPresence);
				}
			}
		}
	}

	FreeCoreUpdate(&update);
	if (curPresence != NULL)
		curPresence->Release();
	if (prevPresence != NULL)
		prevPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FD93E */
int NpImpsSvcMyPresence::ClearPartyPresence()
{
	m_mutex.Lock();
	NpImpsCorePresenceUpdate update;
	sce_paf_memset(&update, 0, sizeof(update));

	NpImpsSvcPresenceData *prevPresence = NULL;
	NpImpsSvcPresenceData *curPresence = NULL;

	int ret = NpImpsSvcPresenceData::Clone(&prevPresence, &m_presence);
	if (ret >= 0) {
		m_presence.ClearPartyPresence();
		ret = BuildCoreUpdate(&update, 1);
		if (ret >= 0) {
			ret = NpImpsSvcPresenceData::Clone(&curPresence, &m_presence);
			if (ret >= 0) {
				ret = npImpsCoreUpdatePresence(&update);
				NotifyObserver(curPresence, prevPresence);
			}
		}
	}

	FreeCoreUpdate(&update);
	if (curPresence != NULL)
		curPresence->Release();
	if (prevPresence != NULL)
		prevPresence->Release();
	m_mutex.Unlock();
	return ret;
}

/* 0x812FDA6A */
int NpImpsSvcMyPresence::IsBigAppGame(int appId)
{
	NpImpsAppMgrStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByAppId(appId, &status) >= 0)
		return CheckAppStatusIsBigAppGame(&status);
	return 0;
}
