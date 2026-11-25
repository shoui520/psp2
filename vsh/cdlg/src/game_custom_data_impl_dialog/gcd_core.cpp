/*
 * SceNpGameCustomDataDlgImpl: SceGcdCoreImpl WebAPI client implementation.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include "gcd_core.h"

extern "C" {
int SceNpWebApi_A11FBA1A(SceSize poolSize, void *poolMem, const char *name);
int SceNpWebApi_C42F1E67(int libCtxId);
int SceNpWebApi_66CEC1A6(int libCtxId);
int SceNpWebApi_B280EA3D(int userCtxId);
int SceNpWebApi_0D910413(int userCtxId, const char *apiGroup, const char *path, int method,
			 const void *reqParam, const void *extraHeader, SceInt64 *requestId);
int sceNpWebApiSendRequest(SceInt64 requestId, const void *data, SceSize dataSize);
int sceNpWebApiReadData(SceInt64 requestId, void *data, SceSize size);
int sceNpWebApiDeleteRequest(SceInt64 requestId);
}

using namespace paf;

static const char kWebApiGroup[] = "gameCustomData";
static const char kMultipartBoundary[] = "SceGcdMultipartBoundary";

SceGcdCoreImpl::SceGcdCoreImpl()
	: m_webApiCtxId(-1),
	  m_userCtxId(-1),
	  m_npLanguage("en"),
	  m_sendSizeLock("SceGcdCoreSendSizeLock", 0),
	  m_initialized(false)
{
}

SceGcdCoreImpl::~SceGcdCoreImpl()
{
	Terminate();
}

const char *SceGcdCoreImpl::NormalizeNpLanguage(SceInt32 systemLang)
{
	switch (systemLang) {
	case 0:
		return "ja";
	case 1:
		return "en";
	case 10:
		return "zh-TW";
	case 11:
		return "zh-CN";
	case 17:
		return "pt-BR";
	case 18:
		return "en-GB";
	default:
		return "en";
	}
}

paf::string SceGcdCoreImpl::BuildTmdbUrl(const char *envPrefix, const char *titleId,
					 const char *hashPart)
{
	char urlBuf[256];
	sce_paf_snprintf(urlBuf, sizeof(urlBuf),
			 "https://tmdb.%s.dl.playstation.net/psp2-tmdb/%s_%s/%s.xml",
			 envPrefix != NULL ? envPrefix : "np",
			 titleId != NULL ? titleId : "",
			 hashPart != NULL ? hashPart : "",
			 titleId != NULL ? titleId : "");
	return paf::string(urlBuf);
}

SceInt32 SceGcdCoreImpl::Initialize(SceInt32 libHttpCtxId, const char *npLanguage)
{
	(void)libHttpCtxId;
	if (m_initialized)
		return SCE_OK;
	if (npLanguage != NULL && npLanguage[0] != '\0')
		m_npLanguage = npLanguage;

	m_webApiCtxId = SceNpWebApi_A11FBA1A(0x10000, NULL, "SceGcdCoreImpl");
	if (m_webApiCtxId < 0)
		return m_webApiCtxId;

	m_userCtxId = SceNpWebApi_66CEC1A6(m_webApiCtxId);
	if (m_userCtxId < 0) {
		SceNpWebApi_C42F1E67(m_webApiCtxId);
		m_webApiCtxId = -1;
		return m_userCtxId;
	}

	m_initialized = true;
	return SCE_OK;
}

void SceGcdCoreImpl::Terminate()
{
	if (m_userCtxId >= 0) {
		SceNpWebApi_B280EA3D(m_userCtxId);
		m_userCtxId = -1;
	}
	if (m_webApiCtxId >= 0) {
		SceNpWebApi_C42F1E67(m_webApiCtxId);
		m_webApiCtxId = -1;
	}
	m_initialized = false;
}

SceInt32 SceGcdCoreImpl::FetchItemList(const char *onlineId, SceInt32 limit, SceInt32 offset,
				       paf::vector<GcdItemSummary> &outItems)
{
	(void)outItems;
	if (!m_initialized)
		return -1;

	char pathBuf[256];
	sce_paf_snprintf(
		pathBuf, sizeof(pathBuf),
		"/v1/users/%s/gameCustomData/items?fields=%s&limit=%d&offset=%d&npLanguage=%s",
		onlineId != NULL ? onlineId : "@default",
		"itemId,fromUser,gameDataDetail,postType",
		(int)limit, (int)offset, m_npLanguage.c_str());

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kWebApiGroup, pathBuf, 0, NULL, NULL,
					    &reqId);
	if (ret < 0)
		return ret;

	ret = sceNpWebApiSendRequest(reqId, NULL, 0);
	if (ret >= 0) {
		char chunk[512];
		while (sceNpWebApiReadData(reqId, chunk, sizeof(chunk)) > 0) {
		}
	}
	sceNpWebApiDeleteRequest(reqId);
	return ret;
}

SceInt32 SceGcdCoreImpl::FetchItemMessage(const char *onlineId, const char *itemId,
					  GcdItemSummary &outDetail)
{
	if (!m_initialized || itemId == NULL)
		return -1;

	char pathBuf[320];
	sce_paf_snprintf(
		pathBuf, sizeof(pathBuf),
		"/v1/users/%s/gameCustomData/items/%s/message?fields=%s&npLanguage=%s&psVitaTitleIcon=circled&titleIconSize=s",
		onlineId != NULL ? onlineId : "@default",
		itemId,
		"message,gameDataDetail,postType,npTitleDetail,toOnlineIds,attachmentDetail",
		m_npLanguage.c_str());

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kWebApiGroup, pathBuf, 0, NULL, NULL,
					    &reqId);
	if (ret < 0)
		return ret;

	ret = sceNpWebApiSendRequest(reqId, NULL, 0);
	if (ret >= 0) {
		outDetail.itemId = itemId;
		char chunk[512];
		while (sceNpWebApiReadData(reqId, chunk, sizeof(chunk)) > 0) {
		}
	}
	sceNpWebApiDeleteRequest(reqId);
	return ret;
}

SceInt32 SceGcdCoreImpl::FetchBinaryPart(const char *onlineId, const char *itemId,
					 const char *partName, paf::vector<SceUInt8> &outBytes)
{
	if (!m_initialized || itemId == NULL || partName == NULL)
		return -1;

	char pathBuf[256];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf),
			 "/v1/users/%s/gameCustomData/items/%s/%s",
			 onlineId != NULL ? onlineId : "@default",
			 itemId, partName);

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kWebApiGroup, pathBuf, 0, NULL, NULL,
					    &reqId);
	if (ret < 0)
		return ret;

	ret = sceNpWebApiSendRequest(reqId, NULL, 0);
	if (ret >= 0) {
		SceUInt8 chunk[1024];
		SceInt32 got;
		while ((got = sceNpWebApiReadData(reqId, chunk, sizeof(chunk))) > 0) {
			size_t oldSize = outBytes.size();
			outBytes.resize(oldSize + (size_t)got);
			sce_paf_memcpy(&outBytes[oldSize], chunk, (size_t)got);
		}
	}
	sceNpWebApiDeleteRequest(reqId);
	return ret;
}

SceInt32 SceGcdCoreImpl::SendItem(const GcdSendRequest &request, paf::string &outItemId)
{
	(void)outItemId;
	if (!m_initialized)
		return -1;

	m_sendSizeLock.Lock();
	paf::string path = "/v1/gameCustomData/items";
	if (request.forward)
		path += "?forward=true";

	paf::string body;
	char headerBuf[160];
	sce_paf_snprintf(headerBuf, sizeof(headerBuf), "--%s\r\n", kMultipartBoundary);
	body += headerBuf;
	body += "Content-Type: application/json; charset=utf-8\r\n";
	body += "Content-Description: message\r\n\r\n{";
	body += "\"to\":[";
	for (size_t i = 0; i < request.recipients.size(); i++) {
		char recipBuf[96];
		sce_paf_snprintf(recipBuf, sizeof(recipBuf), "\"%s\"%s",
				 request.recipients[i].c_str(),
				 (i + 1 < request.recipients.size()) ? "," : "");
		body += recipBuf;
	}
	body += "],\"item\":{\"message\":\"";
	body += request.message.c_str();
	body += "\",\"gameDataDetail\":{\"dataName\":\"";
	body += request.dataName.c_str();
	body += "\",\"dataDescription\":\"";
	body += request.dataDescription.c_str();
	char expireBuf[64];
	sce_paf_snprintf(expireBuf, sizeof(expireBuf), "\",\"expireMinutes\":\"%d\"",
			 (int)request.expireMinutes);
	body += expireBuf;
	body += ",\"availablePlatforms\":[\"PSVITA\"]}}}";
	sce_paf_snprintf(headerBuf, sizeof(headerBuf), "\r\n--%s--\r\n", kMultipartBoundary);
	body += headerBuf;

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kWebApiGroup, path.c_str(), 1, NULL, NULL,
					    &reqId);
	if (ret >= 0) {
		ret = sceNpWebApiSendRequest(reqId, body.c_str(), body.size());
		sceNpWebApiDeleteRequest(reqId);
	}

	m_sendSizeLock.Unlock();
	return ret;
}

SceInt32 SceGcdCoreImpl::UpdateItemFlag(const char *onlineId, const char *itemId,
					const char *flagName)
{
	if (!m_initialized || itemId == NULL || flagName == NULL)
		return -1;

	char pathBuf[192];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf),
			 "/v1/users/%s/gameCustomData/items/%s",
			 onlineId != NULL ? onlineId : "@default", itemId);

	char payload[64];
	sce_paf_snprintf(payload, sizeof(payload), "{\"%s\":true}", flagName);

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kWebApiGroup, pathBuf, 2, NULL, NULL,
					    &reqId);
	if (ret < 0)
		return ret;

	ret = sceNpWebApiSendRequest(reqId, payload, sce_paf_strlen(payload));
	sceNpWebApiDeleteRequest(reqId);
	return ret;
}

SceInt32 SceGcdCoreImpl::DeleteItem(const char *onlineId, const char *itemId)
{
	if (!m_initialized || itemId == NULL)
		return -1;

	char pathBuf[192];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf),
			 "/v1/users/%s/gameCustomData/items/%s",
			 onlineId != NULL ? onlineId : "@default", itemId);

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kWebApiGroup, pathBuf, 3, NULL, NULL,
					    &reqId);
	if (ret < 0)
		return ret;

	ret = sceNpWebApiSendRequest(reqId, NULL, 0);
	sceNpWebApiDeleteRequest(reqId);
	return ret;
}
