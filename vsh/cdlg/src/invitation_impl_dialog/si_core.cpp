/*
 * SceInvitationDlgImplPlugin: SceSiCoreImpl WebAPI client implementation.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include "si_core.h"

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

static const char kSiWebApiGroup[] = "sessionInvitation";
static const char kSiMultipartBoundary[] = "SceSiMultipartBoundary";

SceSiCoreImpl::SceSiCoreImpl()
	: m_webApiCtxId(-1),
	  m_userCtxId(-1),
	  m_npLanguage("en"),
	  m_sendSizeLock("SceSessionInvitationCoreSendSizeLock", 0),
	  m_initialized(false)
{
}

SceSiCoreImpl::~SceSiCoreImpl()
{
	Terminate();
}

paf::string SceSiCoreImpl::BuildTmdbUrl(const char *envPrefix, const char *titleId,
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

SceInt32 SceSiCoreImpl::Initialize(SceInt32 libHttpCtxId, const char *npLanguage)
{
	(void)libHttpCtxId;
	if (m_initialized)
		return SCE_OK;
	if (npLanguage != NULL && npLanguage[0] != '\0')
		m_npLanguage = npLanguage;

	m_webApiCtxId = SceNpWebApi_A11FBA1A(0x10000, NULL, "SceSiCoreImpl");
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

void SceSiCoreImpl::Terminate()
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

SceInt32 SceSiCoreImpl::FetchInvitationList(const char *onlineId, SceInt32 limit, SceInt32 offset,
					    paf::vector<SessionInvitationSummary> &outList)
{
	(void)outList;
	if (!m_initialized)
		return -1;

	char pathBuf[256];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf),
			 "/v1/users/%s/invitations?limit=%d&offset=%d&npLanguage=%s",
			 onlineId != NULL ? onlineId : "me",
			 (int)limit, (int)offset, m_npLanguage.c_str());

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kSiWebApiGroup, pathBuf, 0, NULL, NULL,
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

SceInt32 SceSiCoreImpl::FetchSessionPart(const char *sessionId, const char *partName,
					 paf::vector<SceUInt8> &outBytes)
{
	if (!m_initialized || sessionId == NULL || partName == NULL)
		return -1;

	char pathBuf[256];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf), "/v1/sessions/%s/%s", sessionId, partName);

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kSiWebApiGroup, pathBuf, 0, NULL, NULL,
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

SceInt32 SceSiCoreImpl::FetchInvitationPart(const char *onlineId, const char *invitationId,
					    const char *partName, paf::vector<SceUInt8> &outBytes)
{
	if (!m_initialized || invitationId == NULL || partName == NULL)
		return -1;

	char pathBuf[256];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf), "/v1/users/%s/invitations/%s/%s",
			 onlineId != NULL ? onlineId : "me", invitationId, partName);

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kSiWebApiGroup, pathBuf, 0, NULL, NULL,
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

SceInt32 SceSiCoreImpl::CreateSession(const SessionCreateRequest &request, paf::string &outSessionId)
{
	(void)outSessionId;
	if (!m_initialized)
		return -1;

	m_sendSizeLock.Lock();
	paf::string body;
	char headerBuf[160];
	sce_paf_snprintf(headerBuf, sizeof(headerBuf), "--%s\r\n", kSiMultipartBoundary);
	body += headerBuf;
	body += "Content-Type: application/json; charset=utf-8\r\n";
	body += "Content-Description: session-request\r\n\r\n{";
	char metaBuf[256];
	sce_paf_snprintf(metaBuf, sizeof(metaBuf),
			 "\"sessionType\":\"%s\",\"sessionPrivacy\":\"%s\",\"sessionMaxUser\":%u,",
			 request.sessionType.empty() ? "owner-bind" : request.sessionType.c_str(),
			 request.sessionPrivacy.empty() ? "public" : request.sessionPrivacy.c_str(),
			 (unsigned int)request.sessionMaxUser);
	body += metaBuf;
	body += "\"sessionName\":\"";
	body += request.sessionName.c_str();
	body += "\",\"sessionStatus\":\"";
	body += request.sessionStatus.c_str();
	body += "\",\"availablePlatforms\":[\"PSVITA\"]}";
	sce_paf_snprintf(headerBuf, sizeof(headerBuf), "\r\n--%s--\r\n", kSiMultipartBoundary);
	body += headerBuf;

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kSiWebApiGroup, "/v1/sessions", 1, NULL,
					    NULL, &reqId);
	if (ret >= 0) {
		ret = sceNpWebApiSendRequest(reqId, body.c_str(), body.size());
		sceNpWebApiDeleteRequest(reqId);
	}
	m_sendSizeLock.Unlock();
	return ret;
}

SceInt32 SceSiCoreImpl::UpdateInvitationFlag(const char *onlineId, const char *invitationId,
					     const char *flagName)
{
	if (!m_initialized || invitationId == NULL || flagName == NULL)
		return -1;

	char pathBuf[192];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf), "/v1/users/%s/invitations/%s",
			 onlineId != NULL ? onlineId : "me", invitationId);

	char payload[64];
	sce_paf_snprintf(payload, sizeof(payload), "{\"%s\":true}", flagName);

	SceInt64 reqId = -1;
	SceInt32 ret = SceNpWebApi_0D910413(m_userCtxId, kSiWebApiGroup, pathBuf, 2, NULL, NULL,
					    &reqId);
	if (ret < 0)
		return ret;

	ret = sceNpWebApiSendRequest(reqId, payload, sce_paf_strlen(payload));
	sceNpWebApiDeleteRequest(reqId);
	return ret;
}
