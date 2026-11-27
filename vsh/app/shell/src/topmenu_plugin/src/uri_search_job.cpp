/*
 * uri_search_job.cpp: UriSearchJob and psgm:/scheme URI parsing helpers
 * (0x8128D64C-0x8128E37B).
 * Resolves custom URI schemes via livespace_db, rewrites app launch parameters,
 * parses psgm:play?/psgm:open? query fields, and checks SUPPORT_URI whitelists.
 */
#include "topmenu_internal.h"

namespace {

struct UriSchemeMapping {
	const char *scheme;
	SceSize schemeLength;
	const char *titleId;
};

/* Built-in system URI scheme table initialized at module start (0x815458BC, 0x8128E148) */
static UriSchemeMapping s_builtinUriSchemes[] = {
	{ "psns:",		sce_paf_strlen("psns:"),		"NPXS10002" },
	{ "http://maps.google.", sce_paf_strlen("http://maps.google."),	"NPXS10005" },
	{ "http://g.co/maps/",	sce_paf_strlen("http://g.co/maps/"),	"NPXS10005" },
	{ "scemap:",		sce_paf_strlen("scemap:"),		"NPXS10005" },
	{ "http:",		sce_paf_strlen("http:"),		"NPXS10003" },
	{ "https:",		sce_paf_strlen("https:"),		"NPXS10003" },
	{ "photo:",		sce_paf_strlen("photo:"),		"NPXS10004" },
	{ "camera:",		sce_paf_strlen("camera:"),		"NPXS10004" },
	{ "video:",		sce_paf_strlen("video:"),		"NPXS10010" },
	{ "music:",		sce_paf_strlen("music:"),		"NPXS10009" },
	{ "psnmsg:",		sce_paf_strlen("psnmsg:"),		"NPXS10014" },
	{ "nearutil:",		sce_paf_strlen("nearutil:"),		"NPXS10032" },
	{ "photoexport:",	sce_paf_strlen("photoexport:"),		"NPXS10023" },
	{ "psnreg:",		sce_paf_strlen("psnreg:"),		"NPXS10018" },
	{ "avmedia_service:",	sce_paf_strlen("avmedia_service:"),	"NPXS10036" },
	{ "pstc:",		sce_paf_strlen("pstc:"),		"NPXS10008" },
	{ "near:",		sce_paf_strlen("near:"),		"NPXS10000" },
	{ "sceman:",		sce_paf_strlen("sceman:"),		"NPXS10003" },
	{ "search:",		sce_paf_strlen("search:"),		"NPXS10003" },
	{ "webcore_mini:",	sce_paf_strlen("webcore_mini:"),	"NPXS10037" },
	{ "pspy:",		sce_paf_strlen("pspy:"),		"NPXS10001" },
	{ "wcpark:",		sce_paf_strlen("wcpark:"),		"NPXS10007" },
	{ "wbapp0:",		sce_paf_strlen("wbapp0:"),		"NPXS10003" },
	{ "settings_dlg:",	sce_paf_strlen("settings_dlg:"),	"NPXS10016" },
	{ "wbapp1:",		sce_paf_strlen("wbapp1:"),		"NPXS10090" },
	{ "psgr:",		sce_paf_strlen("psgr:"),		"NPXS10065" },
	{ "psts:",		sce_paf_strlen("psts:"),		"NPXS10035" },
	{ "dtcpip:",		sce_paf_strlen("dtcpip:"),		"NPXS10080" },
	{ "psm_manual:",	sce_paf_strlen("psm_manual:"),		"NPXS10081" },
};

static const char *findSubstring(const char *haystack, const char *needle)
{
	SceSize needleLen = sce_paf_strlen(needle);
	if (needleLen == 0)
		return haystack;
	for (const char *cursor = haystack; *cursor != '\0'; ++cursor) {
		if (*cursor == *needle && sce_paf_strncmp(cursor, needle, needleLen) == 0)
			return cursor;
	}
	return NULL;
}

static int extractQueryValue(const char *uri, const char *keyPrefix, char *outBuf, SceSize bufSize)
{
	const char *query = sce_paf_strchr(uri, '?');
	if (query == NULL)
		return 0;

	const char *found = findSubstring(query, keyPrefix);
	if (found == NULL)
		return 0;

	const char *valueStart = found + sce_paf_strlen(keyPrefix);
	const char *valueEnd = sce_paf_strchr(valueStart, '&');
	if (valueEnd == NULL)
		valueEnd = valueStart + sce_paf_strlen(valueStart);

	SceSize len = static_cast<SceSize>(valueEnd - valueStart);
	if (bufSize < len + 1)
		return 0;

	sce_paf_strncpy(outBuf, valueStart, len);
	outBuf[len] = '\0';
	return 1;
}

class UriSearchJob : public paf::job::JobItem {
public:
	UriSearchJob(const char *uri, SceInt32 launchAppId);
	virtual ~UriSearchJob();
	virtual int32_t Run();
	virtual void Finish(int32_t result);

	paf::string m_uri;		/* +0x1C */
	SceInt32 m_launchAppId;		/* +0x28 */
	paf::string m_resolvedTitleId;	/* +0x2C */
};

typedef char UriSearchJobSizeCheck[sizeof(UriSearchJob) == 56 ? 1 : -1];

UriSearchJob::UriSearchJob(const char *uri, SceInt32 launchAppId)
	: paf::job::JobItem("UriSearchJob", NULL),
	  m_uri(uri != NULL ? uri : ""),
	  m_launchAppId(launchAppId),
	  m_resolvedTitleId()
{
}

/* (0x8128D71E / 0x8128D76C) */
UriSearchJob::~UriSearchJob()
{
}

/* (0x8128D7C0) */
int32_t UriSearchJob::Run()
{
	int ret = SceLsdb_E2838878(g_shellLsdbClient->client, m_uri.c_str(), &m_resolvedTitleId, 0);
	if (ret != 0)
		return ret;

	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	if (appInfo.get() != NULL) {
		ret = SceLsdb_87EE0526(g_shellLsdbClient->client, &m_resolvedTitleId, &appInfo);
		if (ret == 0 && !SceLsdb_41019D89(&appInfo)) {
			char launchParam[1024];
			sce_paf_snprintf(launchParam, sizeof(launchParam),
					 "type=LAUNCH_APP_BY_URI&uri=%s", m_uri.c_str());
			ret = sceAppMgrOverwriteLaunchParamForShell(m_launchAppId, launchParam);
		}
	}
	return ret;
}

/* (0x8128D922) */
void UriSearchJob::Finish(int32_t result)
{
	topmenuOnUriSearchDone(result, m_resolvedTitleId.c_str(), m_launchAppId);
}

} /* namespace */

/* (0x8128D64C) */
int topmenuStartUriSearch(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			  const char *uri, SceInt32 launchAppId)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new UriSearchJob(uri, launchAppId));
	*outJob = job;
	return g_topWindow->m_jobQueue->Enqueue(job);
}

/* (0x8128D932) */
int topmenuParsePsgmUriScheme(const char *uri)
{
	if (sce_paf_strncmp(uri, "psgm:play?", sce_paf_strlen("psgm:play?")) == 0)
		return 1;
	if (sce_paf_strncmp(uri, "psgm:open?", sce_paf_strlen("psgm:open?")) == 0)
		return 2;
	return 0;
}

/* (0x8128D984) */
int topmenuParsePsgmTitleId(const char *uri, char *outBuf, SceSize bufSize)
{
	return extractQueryValue(uri, "titleid=", outBuf, bufSize);
}

/* (0x8128D9FC) */
int topmenuParsePsgmNpCommId(const char *uri, char *outBuf, SceSize bufSize)
{
	return extractQueryValue(uri, "npcommid=", outBuf, bufSize);
}

/* (0x8128DA74) */
int topmenuParsePsgmContent(const char *uri, char *outBuf, SceSize bufSize)
{
	return extractQueryValue(uri, "content=", outBuf, bufSize);
}

/* (0x8128DAEC) */
int topmenuParsePsgmType(const char *uri, char *outBuf, SceSize bufSize)
{
	return extractQueryValue(uri, "type=", outBuf, bufSize);
}

/* (0x8128DB64) */
int topmenuParsePsgmReqVer(const char *uri, char *outBuf, SceSize bufSize)
{
	return extractQueryValue(uri, "reqver=", outBuf, bufSize);
}

/* (0x8128DBDC) */
int topmenuParsePsgmTitleName(const char *uri, char *outBuf, SceSize bufSize)
{
	if (topmenuParsePsgmUriScheme(uri) == 0)
		return 0;

	char escapedTitle[1024];
	if (!extractQueryValue(uri, "titlename=", escapedTitle, sizeof(escapedTitle)))
		return 0;

	SceSize required = 0;
	return (sceHttpUriUnescape(outBuf, &required, bufSize, escapedTitle) == 0) ? 1 : 0;
}

/* (0x8128DCD8) */
int topmenuParsePsgmAltTitleIds(const char *uri, char *outBuf, SceSize bufSize)
{
	char launchType[32];
	if (!topmenuParsePsgmType(uri, launchType, sizeof(launchType)))
		return 0;
	if (sce_paf_strcmp("SESSION_INVITATION", launchType) != 0 &&
	    sce_paf_strcmp("GAME_CUSTOM_DATA", launchType) != 0) {
		return 0;
	}
	return extractQueryValue(uri, "alttitleids=", outBuf, bufSize);
}

/* (0x8128DDA8) */
int topmenuParsePsgmCombinedTitleIds(const char *uri, char *outBuf, SceSize bufSize)
{
	char launchType[32];
	char mainTitleId[16];
	if (!topmenuParsePsgmType(uri, launchType, sizeof(launchType)))
		return 0;
	if (sce_paf_strcmp("SESSION_INVITATION", launchType) != 0 &&
	    sce_paf_strcmp("GAME_CUSTOM_DATA", launchType) != 0) {
		return 0;
	}
	if (!topmenuParsePsgmTitleId(uri, mainTitleId, sizeof(mainTitleId)))
		return 0;

	if (extractQueryValue(uri, "alttitleids=", outBuf, bufSize)) {
		paf::string altCopy(outBuf);
		sce_paf_snprintf(outBuf, bufSize, "%s,%s", mainTitleId, altCopy.c_str());
	} else {
		sce_paf_snprintf(outBuf, bufSize, "%s", mainTitleId);
	}
	return 1;
}

/* (0x8128DED8) */
int topmenuUriMatchesSupportList(const char *uri, const LsdbAppInfoPtr *appInfo)
{
	const char *supportUri = SceLsdb_AE60C3A5(appInfo);
	if (supportUri == NULL || uri == NULL)
		return 0;

	paf::list<paf::string> patterns;
	paf::string rawList(supportUri);
	paf::common::string_util::tokenize(patterns, rawList, 0, '\n');

	for (paf::list<paf::string>::iterator it = patterns.begin();
	     it != patterns.end(); ++it) {
		size_t openBracket = it->find("<");
		if (openBracket != paf::string::npos)
			it->erase(openBracket, paf::string::npos);

		size_t closeBracket = it->find(">");
		if (closeBracket != paf::string::npos)
			it->erase(closeBracket, paf::string::npos);

		if (sce_paf_strncmp(it->c_str(), uri, it->length()) == 0)
			return 1;
	}
	return 0;
}

/* (0x8128E0F6) */
int topmenuStartUriSearchOrFastPath(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				    const char *uri, SceInt32 launchAppId)
{
	TopTab *debugTab = topmenuFindDebuggerTab(g_topWindow);
	if (debugTab != NULL && debugTab->m_appData != NULL &&
	    topmenuUriMatchesSupportList(uri, &debugTab->m_appData->m_process.m_appInfo)) {
		topmenuOnUriSearchDone(0, "NPXS10999", launchAppId);
		outJob->reset();
		return 0;
	}
	return topmenuStartUriSearch(outJob, uri, launchAppId);
}
