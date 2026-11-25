/*
 * game_update.cpp: SceGameUpdate HTTP/HMAC/XML patch check client linked into
 * SceShell (0x812A9708-0x812AB957).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libhmac.h>
#include <rtc.h>
#include <regmgr/regmgr.h>
#include <np_common/np_common.h>
#include <std/string>
#include "game_update.h"

extern "C" {
int sceHttpCreateTemplate(const char *userAgent, int httpVer, int autoProxyConf);
int sceHttpDeleteTemplate(int tmplId);
int sceHttpSetConnectTimeOut(int id, SceUInt32 usec);
int sceHttpSetRecvTimeOut(int id, SceUInt32 usec);
int sceHttpSetSendTimeOut(int id, SceUInt32 usec);
int sceHttpRemoveRequestHeader(int id, const char *name);
int sceHttpsEnableOptionPrivate(int id, SceUInt32 option);
int sceHttpsDisableOptionPrivate(int id, SceUInt32 option);
int SceHttp_53B722F2(int tmplId, int enable, int proxyType, const char *host, SceUInt16 port);
typedef int (*GameUpdateHttpAuthCallback)(int request, int authType, const char *realm,
					  char *username, char *password, bool isProxy,
					  char *reservedA, SceSize *reservedB,
					  bool *save, void *userArg);
int sceHttpSetAuthInfoCallback(int id, GameUpdateHttpAuthCallback cbFunc, void *userArg);
int sceHttpCreateConnectionWithURL(int tmplId, const char *url, int enableKeepalive);
int sceHttpDeleteConnection(int connId);
int sceHttpCreateRequestWithURL(int connId, int method, const char *url, SceUInt64 contentLength);
int sceHttpDeleteRequest(int reqId);
int sceHttpSendRequest(int reqId, const void *postData, SceSize size);
int sceHttpGetStatusCode(int reqId, int *statusCode);
int sceHttpGetResponseContentLength(int reqId, SceUInt64 *contentLength);
int sceHttpReadData(int reqId, void *data, SceSize size);
int sceHttpAbortRequest(int reqId);

int SceNpManager_9554802B(const char *category, const char *tagName, int option);
}

/* BGDL task status query & patch cache helpers (0x813C85D0-0x813C9916) */
struct BgdlPatchTaskStatus {
	SceUInt32 taskState;
	SceUInt32 taskType;
	SceInt32 errorCode;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	paf::string titleId;
	paf::string contentName;
};

struct GameUpdateCacheEntry {
	SceUInt64 networkTick;
	SceInt32 status;
	SceUInt32 flags;
};

extern void shellBgdlPatchTaskStatusInit(BgdlPatchTaskStatus *status);		/* 0x813C85D0 */
extern int shellBgdlFindPatchTaskByTitleId(const char *titleId,
					   BgdlPatchTaskStatus *outStatus);	/* 0x813C89FC */
extern int shellBgdlDeletePatchTasksByTitleId(const char *titleId);		/* 0x813C8A4C */
extern int shellGameUpdateEnqueueCacheWrite(const char *titleId,
					    const GameUpdateCacheEntry *entry,
					    int updateMask);			/* 0x813C9916 */

/* System registry keys read by SceGameUpdate */
#define REG_KEY_NP_ENV			210968		/* /CONFIG/NP/env */
#define REG_KEY_SYSTEM_LANGUAGE		226562		/* /CONFIG/SYSTEM/language */
#define REG_KEY_HTTP_PROXY_PASS		1477531		/* /CONFIG/NET/http_proxy_pass */
#define REG_KEY_HTTP_PROXY_PORT		2273681		/* /CONFIG/NET/http_proxy_port */
#define REG_KEY_NP_DEBUG_UPGRADABLE	3225861		/* /CONFIG/NP/debug_upgradable */
#define REG_KEY_HTTP_PROXY_SERVER	6580878		/* /CONFIG/NET/http_proxy_server */
#define REG_KEY_HTTP_PROXY_FLAG		7338025		/* /CONFIG/NET/http_proxy_flag */
#define REG_KEY_HTTP_PROXY_USER		9352296		/* /CONFIG/NET/http_proxy_user */
#define REG_KEY_NP_COUNTRY		9409785		/* /CONFIG/NP/country */

/* Context lifecycle states stored in GameUpdateCore::lifecycleState */
enum GameUpdateLifecycleState {
	GAMEUPDATE_LIFECYCLE_UNINIT   = 0,
	GAMEUPDATE_LIFECYCLE_READY    = 1,
	GAMEUPDATE_LIFECYCLE_RUNNING  = 2,
	GAMEUPDATE_LIFECYCLE_ABORTING = 3,
	GAMEUPDATE_LIFECYCLE_DONE     = 4
};

/* XML parser state machine values stored in GameUpdateXmlWork::parseState */
enum GameUpdateXmlParseState {
	XML_STATE_ROOT           = 0,
	XML_STATE_TITLEPATCH     = 1,
	XML_STATE_TAG            = 2,
	XML_STATE_PACKAGE        = 3,
	XML_STATE_HYBRID_PACKAGE = 4,
	XML_STATE_PARAMSFO       = 5,
	XML_STATE_TITLE          = 6,
	XML_STATE_TITLE_LANG     = 7,
	XML_STATE_CHANGEINFO     = 8,
	XML_STATE_CHANGEINFO_LANG = 9,
	XML_STATE_URL            = 10
};

/* Parsed attribute presence flags in GameUpdateXmlWork::attrMask */
#define XML_ATTR_TITLEID		0x00000001u
#define XML_ATTR_STATUS			0x00000002u
#define XML_ATTR_TAG_NAME		0x00000004u
#define XML_ATTR_TAG_SIGNOFF		0x00000008u
#define XML_ATTR_TAG_MIN_SYSVER		0x00000010u
#define XML_ATTR_PKG_VERSION		0x00000020u
#define XML_ATTR_PKG_SIZE		0x00000040u
#define XML_ATTR_PKG_SHA1		0x00000080u
#define XML_ATTR_PKG_URL		0x00000100u
#define XML_ATTR_PKG_CONTENT_ID		0x00000200u
#define XML_ATTR_PKG_PSP2_SYSVER	0x00000400u
#define XML_ATTR_PKG_PSP_SYSVER		0x00000800u
#define XML_ATTR_PKG_TYPE		0x00001000u
#define XML_ATTR_CHANGEINFO_URL		0x00002000u
#define XML_ATTR_CHANGEINFO_LANG_URL	0x00004000u
#define XML_ATTR_URL_SIZE		0x00008000u
#define XML_ATTR_URL_SHA1		0x00010000u
#define XML_ATTR_URL_HREF		0x00020000u
#define XML_ATTR_URL_CONTENT_ID		0x00040000u
#define XML_ATTR_HYBRID_SIZE		0x00080000u
#define XML_ATTR_HYBRID_SHA1		0x00100000u
#define XML_ATTR_HYBRID_URL		0x00200000u
#define XML_ATTR_HYBRID_CONTENT_ID	0x00400000u
#define XML_ATTR_TITLE_DEFAULT		0x00800000u
#define XML_ATTR_TITLE_LANG		0x01000000u

/* Version string format flags for parseVersionString */
#define VERSION_FMT_MAJOR2_MINOR2	0x01	/* "XX.YY" (5 chars) */
#define VERSION_FMT_MAJOR2_MINOR4	0x02	/* "XX.YYYY" (7 chars) */
#define VERSION_FMT_MAJOR1_MINOR2	0x04	/* "X.YY" (4 chars) */

/* Single parsed <package> linked-list node (736 = 0x2E0 bytes) */
struct GameUpdatePackageNode {
	GameUpdatePackageNode *next;		/* +0x000 */
	SceUInt16 versionMajor;			/* +0x004 */
	SceUInt16 versionMinor;			/* +0x006 */
	SceUInt64 packageSize;			/* +0x008 */
	SceUInt8 sha1Digest[20];		/* +0x010 */
	char packageUrl[256];			/* +0x024 */
	char changeInfoUrl[256];		/* +0x124 */
	char contentId[48];			/* +0x224 */
	SceInt32 packageType;			/* +0x254 */
	char titleName[128];			/* +0x258 */
	SceUInt32 requiredPsp2SysVer;		/* +0x2D8 */
	SceUInt16 requiredPspSysVerMajor;	/* +0x2DC */
	SceUInt16 requiredPspSysVerMinor;	/* +0x2DE */
};

/* Temporary override parsed from <url> or <hybrid_package> (344 = 0x158 bytes) */
struct GameUpdateAltPackage {
	SceUInt16 versionMajor;			/* +0x000 */
	SceUInt16 versionMinor;			/* +0x002 */
	SceUInt32 alignmentPad;			/* +0x004 */
	SceUInt64 packageSize;			/* +0x008 */
	SceUInt8 sha1Digest[20];		/* +0x010 */
	char packageUrl[256];			/* +0x024 */
	char contentId[48];			/* +0x124 */
	SceUInt32 trailingPad;			/* +0x154 */
};

/* Core SceGameUpdate state (200 = 0xC8 bytes) */
struct GameUpdateCore {
	SceInt32 lifecycleState;		/* +0x00 */
	SceUInt32 alignmentPad;			/* +0x04 */
	SceKernelLwMutexWork mutex;		/* +0x08 (32 bytes) */
	void *(*allocFunc)(SceSize size);	/* +0x28 */
	void (*freeFunc)(void *ptr);		/* +0x2C */
	SceInt32 targetPlatform;		/* +0x30 */
	char queryTitleId[16];			/* +0x34 */
	SceUInt16 currentVerMajor;		/* +0x44 */
	SceUInt16 currentVerMinor;		/* +0x46 */
	SceUInt32 currentPsp2SysVer;		/* +0x48 */
	SceUInt16 currentPspSysVerMajor;	/* +0x4C */
	SceUInt16 currentPspSysVerMinor;	/* +0x4E */
	char contentId[48];			/* +0x50 */
	SceInt32 titleStatusNotAlive;		/* +0x80 */
	char tagName[20];			/* +0x84 */
	SceUInt32 infoFlags;			/* +0x98 */
	SceUInt16 latestVerMajor;		/* +0x9C */
	SceUInt16 latestVerMinor;		/* +0x9E */
	SceUInt32 minPsp2SysVer;		/* +0xA0 */
	SceUInt16 minPspSysVerMajor;		/* +0xA4 */
	SceUInt16 minPspSysVerMinor;		/* +0xA6 */
	SceUInt32 packageCount;			/* +0xA8 */
	GameUpdatePackageNode *packageListHead;	/* +0xAC */
	char countryCode[3];			/* +0xB0 */
	char originalTitleId[16];		/* +0xB3 */
	SceUInt8 titleIdPad;			/* +0xC3 */
	SceUInt32 initFlags;			/* +0xC4 */
};

/* HTTP work buffer (2224 = 0x8B0 bytes) */
struct GameUpdateHttpWork {
	SceInt32 templateId;			/* +0x000 */
	SceInt32 connectionId;			/* +0x004 */
	SceInt32 requestId;			/* +0x008 */
	SceInt32 statusCode;			/* +0x00C */
	SceUInt64 contentLength;		/* +0x010 */
	char readBuffer[2048];			/* +0x018 */
	char proxyUser[65];			/* +0x818 */
	char proxyPass[65];			/* +0x859 */
	char npEnv[17];				/* +0x89A */
	char trailingPad[5];			/* +0x8AB */
};

/* Streaming XML parser state (2032 = 0x7F0 bytes) */
struct GameUpdateXmlWork {
	NpXmlParser parser;			/* +0x000 (584 bytes) */
	SceInt32 parseState;			/* +0x248 */
	SceUInt32 attrMask;			/* +0x24C */
	SceUInt64 headerPad;			/* +0x250 */
	GameUpdatePackageNode currentPkg;	/* +0x258 (736 bytes) */
	GameUpdateAltPackage altUrlPkg;		/* +0x538 (344 bytes) */
	GameUpdateAltPackage hybridPkg;		/* +0x690 (344 bytes) */
	SceInt32 hybridIsNewer;			/* +0x7E8 */
	SceInt32 hasExplicitPkgType;		/* +0x7EC */
};

/* Compile-time size verification matching original binary allocations */
typedef char GameUpdatePackageNodeSizeCheck[sizeof(GameUpdatePackageNode) == 0x2E0 ? 1 : -1];
typedef char GameUpdateAltPackageSizeCheck[sizeof(GameUpdateAltPackage) == 0x158 ? 1 : -1];
typedef char GameUpdateCoreSizeCheck[sizeof(GameUpdateCore) == 0xC8 ? 1 : -1];
typedef char GameUpdateHttpWorkSizeCheck[sizeof(GameUpdateHttpWork) == 0x8B0 ? 1 : -1];
typedef char GameUpdateXmlWorkSizeCheck[sizeof(GameUpdateXmlWork) == 0x7F0 ? 1 : -1];
typedef char GameUpdateInitParamSizeCheck[sizeof(SceGameUpdateInitParam) == 112 ? 1 : -1];
typedef char GameUpdateResultSizeCheck[sizeof(SceGameUpdateResult) == 16 ? 1 : -1];
typedef char GameUpdateInfoSizeCheck[sizeof(SceGameUpdateInfo) == 412 ? 1 : -1];
typedef char GameUpdatePackageInfoSizeCheck[sizeof(SceGameUpdatePackageInfo) == 304 ? 1 : -1];

/* XOR-0x7F obfuscated HMAC-SHA256 key for Vita titlepatch URL signing (0x81517DFC) */
static const SceUInt8 s_hmacKeyObfuscated[32] = {
	0x9A, 0x9D, 0x07, 0xD5, 0x61, 0x9C, 0x3F, 0xFD,
	0xDF, 0xF7, 0x58, 0xE3, 0xFC, 0x86, 0xC4, 0xB7,
	0x79, 0xFD, 0x63, 0x2D, 0x8D, 0xD4, 0x22, 0x54,
	0x35, 0xC2, 0xE6, 0x2B, 0x2F, 0x4A, 0x2E, 0x6B
};

/* Valid NP environment names permitted to perform titlepatch lookups (0x81517DE4) */
static const char *const s_allowedNpEnvs[] = {
	"np",
	"prod-qa",
	"sp-int",
	"rc",
	"e1-np",
	NULL
};

/* Dynamic language-suffixed tag names updated on each init (0x81545B34, 0x81545B44) */
static char s_changeinfoLangTag[16] = "changeinfo_01";
static char s_titleLangTag[16] = "title_01";

/* SceNpCommon streaming XML schema for *-ver.xml (0x81517F4C-0x81517FDC) */
static const NpXmlElement s_xmlLevelTitlePatch[] = {
	{ "titlepatch", 0 },
	{ "", 1 }
};

static const NpXmlElement s_xmlLevelTag[] = {
	{ "tag", 0 },
	{ "", 1 }
};

static const NpXmlElement s_xmlLevelPackage[] = {
	{ "package", 0 },
	{ "", 1 }
};

static const NpXmlElement s_xmlLevelPackageChild[] = {
	{ "hybrid_package", 0 },
	{ "paramsfo", 0 },
	{ "changeinfo", 0 },
	{ s_changeinfoLangTag, 0 },
	{ "url", 0 },
	{ "", 1 }
};

static const NpXmlElement s_xmlLevelTitle[] = {
	{ "title", 1 },
	{ s_titleLangTag, 1 },
	{ "", 1 }
};

static const NpXmlElement *const s_xmlLevels[] = {
	s_xmlLevelTitlePatch,
	s_xmlLevelTag,
	s_xmlLevelPackage,
	s_xmlLevelPackageChild,
	s_xmlLevelTitle
};

static const NpXmlSchema s_xmlSchema = {
	s_xmlLevels,
	5
};

static const char s_hexDigits[] = "0123456789abcdef";

/* Parses a 40-char hexadecimal SHA-1 string into 20 raw bytes */
static bool parseSha1Hex(const char *hexStr, SceUInt8 *outDigest)
{
	if (sceClibStrnlen(hexStr, 41) != 40)
		return false;

	for (int i = 0; i < 20; ++i) {
		char pair[3];
		char *endPtr = NULL;
		pair[0] = hexStr[i * 2];
		pair[1] = hexStr[i * 2 + 1];
		pair[2] = '\0';
		outDigest[i] = (SceUInt8)sceClibStrtoll(pair, &endPtr, 16);
		if (endPtr == NULL || *endPtr != '\0')
			return false;
	}
	return true;
}

/* Parses an ASCII decimal string into an unsigned 32-bit integer */
static bool parseDecimalU32(const char *str, SceUInt32 *outValue)
{
	if (str == NULL)
		return false;
	char *endPtr = NULL;
	SceUInt32 parsed = (SceUInt32)sceClibStrtoll(str, &endPtr, 10);
	if (endPtr == NULL || *endPtr != '\0')
		return false;
	*outValue = parsed;
	return true;
}

/* Parses "XX.YY", "XX.YYYY", or "X.YY" version strings (0x812AA6B8) */
static int parseVersionString(const char *verStr, SceUInt16 *outMajor,
			      SceUInt16 *outMinor, SceUInt8 formatMask)
{
	bool validLen = false;
	if (sceClibStrnlen(verStr, 6) == 5 && (formatMask & VERSION_FMT_MAJOR2_MINOR2))
		validLen = true;
	else if (sceClibStrnlen(verStr, 8) == 7 && (formatMask & VERSION_FMT_MAJOR2_MINOR4))
		validLen = true;
	else if (sceClibStrnlen(verStr, 5) == 4 && (formatMask & VERSION_FMT_MAJOR1_MINOR2))
		validLen = true;

	if (!validLen)
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;

	const char *strEnd = verStr + sceClibStrnlen(verStr, 8);
	const char *minorPtr = NULL;
	SceUInt16 majorVal = 0;

	if (((formatMask & VERSION_FMT_MAJOR2_MINOR2) ||
	     (formatMask & VERSION_FMT_MAJOR2_MINOR4)) &&
	    (sceClibLookCtypeTable((unsigned char)verStr[0]) & 4) != 0 &&
	    (sceClibLookCtypeTable((unsigned char)verStr[1]) & 4) != 0 &&
	    verStr[2] == '.') {
		majorVal = (SceUInt16)((verStr[0] - '0') * 10 + (verStr[1] - '0'));
		minorPtr = verStr + 3;
	} else if ((formatMask & VERSION_FMT_MAJOR1_MINOR2) &&
		   (sceClibLookCtypeTable((unsigned char)verStr[0]) & 4) != 0 &&
		   verStr[1] == '.') {
		majorVal = (SceUInt16)(verStr[0] - '0');
		minorPtr = verStr + 2;
	} else {
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
	}

	if ((sceClibLookCtypeTable((unsigned char)minorPtr[0]) & 4) == 0 ||
	    (sceClibLookCtypeTable((unsigned char)minorPtr[1]) & 4) == 0)
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;

	SceUInt16 minorVal = (SceUInt16)((minorPtr[0] - '0') * 10 + (minorPtr[1] - '0'));
	if (formatMask & VERSION_FMT_MAJOR2_MINOR4) {
		minorVal = (SceUInt16)(minorVal * 100);
		if (minorPtr + 2 < strEnd) {
			if ((sceClibLookCtypeTable((unsigned char)minorPtr[2]) & 4) == 0 ||
			    (sceClibLookCtypeTable((unsigned char)minorPtr[3]) & 4) == 0)
				return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
			minorVal = (SceUInt16)(minorVal + (minorPtr[2] - '0') * 10 +
					       (minorPtr[3] - '0'));
		}
	}

	*outMajor = majorVal;
	*outMinor = minorVal;
	return 0;
}

/* Computes lowercase hex HMAC-SHA256("<env>_<titleId>") for Vita patch URLs (0x812AA000) */
static int computeTitlePatchHmacHex(const char *npEnv, const char *titleId,
				    char *outHex, SceSize maxHexLen)
{
	SceUInt8 key[32];
	SceUInt8 digest[32];
	char message[32];

	sceClibMemcpy_safe(key, s_hmacKeyObfuscated, sizeof(key));
	sceClibMemset(digest, 0, sizeof(digest));
	sceClibMemset(message, 0, 30);

	for (int i = 0; i < 32; ++i)
		key[i] ^= 0x7Fu;

	sceClibStrlcat(message, npEnv, 30);
	sceClibStrlcat(message, "_", 30);
	sceClibStrlcat(message, titleId, 30);

	int ret = sceHmacSha256Digest(key, sizeof(key), message,
				     (SceUInt32)sceClibStrnlen(message, 30), digest);
	if (ret >= 0) {
		SceSize written = 0;
		for (SceSize i = 0; i < sizeof(digest); ++i) {
			if (written + 2 > maxHexLen)
				break;
			outHex[written] = s_hexDigits[digest[i] >> 4];
			outHex[written + 1] = s_hexDigits[digest[i] & 0x0Fu];
			written += 2;
		}
	}
	return ret;
}

/* HTTP proxy credentials callback (0x812AA694) */
static int onHttpProxyAuth(int request, int authType, const char *realm,
			   char *username, char *password, bool isProxy,
			   char *reservedA, SceSize *reservedB,
			   bool *save, void *userArg)
{
	(void)request;
	(void)authType;
	(void)realm;
	(void)isProxy;
	(void)reservedA;
	(void)reservedB;
	(void)save;
	const GameUpdateHttpWork *http = (const GameUpdateHttpWork *)userArg;
	sceClibMemcpy_safe(username, http->proxyUser, 64);
	sceClibMemcpy_safe(password, http->proxyPass, 64);
	return 0;
}

/* Clears HTTP work handles to -1 (0x812AA0FA) */
static int gameUpdateHttpReset(SceGameUpdateContext *context)
{
	GameUpdateHttpWork *http = context->http;
	sceClibMemset(http, 0, sizeof(*http));
	http->templateId = -1;
	http->connectionId = -1;
	http->requestId = -1;
	return 0;
}

/* Creates HTTP template with timeouts, HTTPS flags, and optional proxy (0x812AA118) */
static int gameUpdateHttpCreateTemplate(SceGameUpdateContext *context)
{
	GameUpdateHttpWork *http = context->http;
	int tmpl = sceHttpCreateTemplate("PSP2GameUpdate-agent/1.0.0 libhttp/1.0.0", 2, 1);
	if (tmpl < 0) {
		if (http->templateId >= 0) {
			sceHttpDeleteTemplate(http->templateId);
			http->templateId = -1;
		}
		return tmpl;
	}

	http->templateId = tmpl;
	sceHttpSetConnectTimeOut(tmpl, 30000000u);
	sceHttpSetRecvTimeOut(tmpl, 60000000u);
	sceHttpSetSendTimeOut(tmpl, 60000000u);
	sceHttpRemoveRequestHeader(tmpl, "Accept");
	sceHttpRemoveRequestHeader(tmpl, "Accept-Encoding");
	int ret = sceHttpRemoveRequestHeader(tmpl, "Accept-Charset");
	if (ret < 0)
		ret = 0;

	sceHttpsEnableOptionPrivate(tmpl, 357);
	sceHttpsDisableOptionPrivate(tmpl, 24);

	int proxyEnabled = 0;
	if (sceRegMgrSystemParamGetInt(REG_KEY_HTTP_PROXY_FLAG, &proxyEnabled) >= 0 &&
	    proxyEnabled != 0) {
		char proxyHost[256];
		sceClibMemset(proxyHost, 0, sizeof(proxyHost));
		int proxyPort = 0;
		if (sceRegMgrSystemParamGetStr(REG_KEY_HTTP_PROXY_SERVER,
						      proxyHost, sizeof(proxyHost)) >= 0 &&
		    proxyHost[0] != '\0' &&
		    sceRegMgrSystemParamGetInt(REG_KEY_HTTP_PROXY_PORT, &proxyPort) >= 0 &&
		    proxyPort > 0 && proxyPort <= 0xFFFF) {
			if (sceRegMgrSystemParamGetStr(REG_KEY_HTTP_PROXY_USER,
							      http->proxyUser,
							      sizeof(http->proxyUser)) >= 0 &&
			    sceRegMgrSystemParamGetStr(REG_KEY_HTTP_PROXY_PASS,
							      http->proxyPass,
							      sizeof(http->proxyPass)) >= 0) {
				SceHttp_53B722F2(tmpl, 1, 1, proxyHost, (SceUInt16)proxyPort);
				sceHttpSetAuthInfoCallback(tmpl, onHttpProxyAuth, http);
			}
		}
	}
	return ret;
}

/* Destroys the HTTP template handle (0x812AA28A) */
static int gameUpdateHttpDeleteTemplate(SceGameUpdateContext *context)
{
	GameUpdateHttpWork *http = context->http;
	int ret = 0;
	if (http->templateId >= 0) {
		ret = sceHttpDeleteTemplate(http->templateId);
		if (ret < 0)
			ret = 0;
		http->templateId = -1;
	}
	return ret;
}

/* Builds the titlepatch *-ver.xml URL and opens connection + GET request (0x812AA2AE) */
static int gameUpdateHttpCreateRequest(SceGameUpdateContext *context)
{
	GameUpdateCore *core = context->core;
	GameUpdateHttpWork *http = context->http;
	char hmacHex[68];
	char url[256];

	sceClibMemset(url, 0, sizeof(url));
	sceClibMemset(hmacHex, 0, 65);

	int ret = sceRegMgrSystemParamGetStr(REG_KEY_NP_ENV, http->npEnv, 17);
	if (ret < 0)
		return ret;

	if (sceClibStrncasecmp(http->npEnv, "mgmt", 5) == 0)
		sceClibStrlcpy(http->npEnv, "prod-qa", 17);

	if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP2) {
		sceClibStrlcat(url, "https://gs-sec.ww.", sizeof(url));
		sceClibStrlcat(url, http->npEnv, sizeof(url));
		sceClibStrlcat(url, ".dl.playstation.net/pl/", sizeof(url));
		sceClibStrlcat(url, http->npEnv, sizeof(url));
		sceClibStrlcat(url, "/", sizeof(url));
		sceClibStrlcat(url, core->queryTitleId, sizeof(url));
		sceClibStrlcat(url, "/", sizeof(url));
		computeTitlePatchHmacHex(http->npEnv, core->queryTitleId, hmacHex, 64);
		sceClibStrlcat(url, hmacHex, sizeof(url));
		sceClibStrlcat(url, "/", sizeof(url));
		sceClibStrlcat(url, core->queryTitleId, sizeof(url));
		sceClibStrlcat(url, "-ver.xml", sizeof(url));
	} else if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP) {
		sceClibStrlcat(url, "https://a0.ww.", sizeof(url));
		sceClibStrlcat(url, http->npEnv, sizeof(url));
		sceClibStrlcat(url, ".dl.playstation.net/tpl/", sizeof(url));
		sceClibStrlcat(url, http->npEnv, sizeof(url));
		sceClibStrlcat(url, "/", sizeof(url));
		sceClibStrlcat(url, core->queryTitleId, sizeof(url));
		sceClibStrlcat(url, "/", sizeof(url));
		sceClibStrlcat(url, core->queryTitleId, sizeof(url));
		sceClibStrlcat(url, "-ver.xml", sizeof(url));
	}

	ret = sceHttpCreateConnectionWithURL(http->templateId, url, 0);
	if (ret >= 0) {
		http->connectionId = ret;
		ret = sceHttpCreateRequestWithURL(ret, 0, url, 0);
		if (ret >= 0)
			http->requestId = ret;
	}
	return ret;
}

/* Destroys the HTTP request and connection handles (0x812AA4B4) */
static int gameUpdateHttpDeleteRequest(SceGameUpdateContext *context)
{
	GameUpdateHttpWork *http = context->http;
	int ret = 0;
	if (http->requestId >= 0) {
		ret = sceHttpDeleteRequest(http->requestId);
		if (ret < 0)
			ret = 0;
		http->requestId = -1;
	}
	if (http->connectionId >= 0) {
		ret = sceHttpDeleteConnection(http->connectionId);
		if (ret < 0)
			ret = 0;
		http->connectionId = -1;
	}
	return ret;
}

/* Aborts any in-flight HTTP request (0x812AA67E) */
static int gameUpdateHttpAbort(SceGameUpdateContext *context)
{
	GameUpdateHttpWork *http = context->http;
	if (http->requestId >= 0)
		return sceHttpAbortRequest(http->requestId);
	return 0;
}

/* Feeds a chunk of response XML into SceNpCommon's streaming parser (0x812AA8B8) */
static int gameUpdateXmlFeed(SceGameUpdateContext *context, const char *chunk, int length)
{
	return SceNpCommon_88B18A5B(&context->xml->parser, chunk, length);
}

/* Finalizes parsed packages: assigns incremental flags and substitutes hybrid_package if smaller (0x812AA8C2) */
static int gameUpdateXmlFinalize(SceGameUpdateContext *context)
{
	GameUpdateCore *core = context->core;
	GameUpdateXmlWork *xml = context->xml;

	if (xml->hasExplicitPkgType == 0) {
		GameUpdatePackageNode *node = core->packageListHead;
		if (node != NULL) {
			node->packageType = SCE_GAMEUPDATE_PKG_CUMULATIVE;
			node = node->next;
			while (node != NULL) {
				node->packageType = SCE_GAMEUPDATE_PKG_INCREMENTAL;
				node = node->next;
			}
		}
	}

	if ((xml->hybridPkg.versionMajor != 0 || xml->hybridPkg.versionMinor != 0) &&
	    sceClibStrncasecmp(xml->hybridPkg.contentId, core->contentId, 48) == 0 &&
	    core->latestVerMajor == xml->hybridPkg.versionMajor &&
	    core->latestVerMinor == xml->hybridPkg.versionMinor) {
		SceUInt64 totalIncrementalSize = 0;
		for (GameUpdatePackageNode *it = core->packageListHead; it != NULL; it = it->next)
			totalIncrementalSize += it->packageSize;

		if (totalIncrementalSize >= xml->hybridPkg.packageSize) {
			GameUpdatePackageNode *latestNode = core->packageListHead;
			while (latestNode != NULL) {
				if (latestNode->versionMajor == core->latestVerMajor &&
				    latestNode->versionMinor == core->latestVerMinor)
					break;
				latestNode = latestNode->next;
			}
			if (latestNode != NULL) {
				if (core->allocFunc == NULL || core->freeFunc == NULL)
					return SCE_GAMEUPDATE_ERROR_NO_MEMORY;

				GameUpdatePackageNode *replacement =
					(GameUpdatePackageNode *)core->allocFunc(sizeof(GameUpdatePackageNode));
				if (replacement == NULL)
					return SCE_GAMEUPDATE_ERROR_NO_MEMORY;

				sceClibMemcpy_safe(replacement, latestNode, sizeof(GameUpdatePackageNode));
				replacement->packageSize = xml->hybridPkg.packageSize;
				sceClibMemcpy_safe(replacement->sha1Digest, xml->hybridPkg.sha1Digest, 20);
				sceClibMemcpy_safe(replacement->packageUrl, xml->hybridPkg.packageUrl, 256);
				sceClibMemcpy_safe(replacement->contentId, xml->hybridPkg.contentId, 48);
				replacement->next = NULL;
				replacement->packageType = SCE_GAMEUPDATE_PKG_CUMULATIVE;

				GameUpdatePackageNode *cur = core->packageListHead;
				while (cur != NULL) {
					GameUpdatePackageNode *nextNode = cur->next;
					core->freeFunc(cur);
					cur = nextNode;
				}
				core->packageListHead = replacement;
				core->packageCount = 1;
			}
		}
	}

	return 0;
}

/* SceNpCommon XML callback parsing <titlepatch>/<tag>/<package>/... (0x812AAA26) */
static int onGameUpdateXmlEvent(NpXmlParser *parser, int event, SceUInt32 depth,
				SceUInt32 index, const char *name, const char *value,
				void *userArg)
{
	(void)parser;
	SceGameUpdateContext *context = (SceGameUpdateContext *)userArg;
	GameUpdateCore *core = context->core;
	GameUpdateXmlWork *xml = context->xml;
	SceUInt16 parsedMajor = 0;
	SceUInt16 parsedMinor = 0;

	if (event == NP_XML_EVENT_START) {
		if (depth == 0 && index == 0) {
			if (xml->parseState != XML_STATE_ROOT)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_TITLEPATCH;
		} else if (depth == 1 && index == 0) {
			if (xml->parseState != XML_STATE_TITLEPATCH)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_TAG;
		} else if (depth == 2 && index == 0) {
			if (xml->parseState != XML_STATE_TAG)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			sceClibMemset(&xml->currentPkg, 0, sizeof(xml->currentPkg));
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 0) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (xml->hybridPkg.versionMajor < xml->currentPkg.versionMajor ||
			    (xml->hybridPkg.versionMajor == xml->currentPkg.versionMajor &&
			     xml->hybridPkg.versionMinor < xml->currentPkg.versionMinor)) {
				xml->hybridIsNewer = 1;
				sceClibMemset(&xml->hybridPkg, 0, sizeof(xml->hybridPkg));
				xml->hybridPkg.versionMajor = xml->currentPkg.versionMajor;
				xml->hybridPkg.versionMinor = xml->currentPkg.versionMinor;
			} else {
				xml->hybridIsNewer = 0;
			}
			xml->parseState = XML_STATE_HYBRID_PACKAGE;
		} else if (depth == 3 && index == 1) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_PARAMSFO;
		} else if (depth == 3 && index == 2) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_CHANGEINFO;
		} else if (depth == 3 && index == 3) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_CHANGEINFO_LANG;
		} else if (depth == 3 && index == 4) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_URL;
		} else if (depth == 4 && index == 0) {
			if (xml->parseState != XML_STATE_PARAMSFO)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_TITLE;
		} else if (depth == 4 && index == 1) {
			if (xml->parseState != XML_STATE_PARAMSFO)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_TITLE_LANG;
		}
		return 0;
	}

	if (event == NP_XML_EVENT_END) {
		if (depth == 0 && index == 0) {
			if (xml->parseState != XML_STATE_TITLEPATCH)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if ((xml->attrMask & (XML_ATTR_TITLEID | XML_ATTR_STATUS)) !=
			    (XML_ATTR_TITLEID | XML_ATTR_STATUS))
				return SCE_GAMEUPDATE_ERROR_XML_VALUE;
			xml->parseState = XML_STATE_ROOT;
		} else if (depth == 1 && index == 0) {
			if (xml->parseState != XML_STATE_TAG)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if ((xml->attrMask & XML_ATTR_TAG_NAME) == 0)
				return SCE_GAMEUPDATE_ERROR_XML_VALUE;
			xml->parseState = XML_STATE_TITLEPATCH;
		} else if (depth == 2 && index == 0) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;

			if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP2) {
				const SceUInt32 reqMask = XML_ATTR_PKG_VERSION | XML_ATTR_PKG_SIZE |
							  XML_ATTR_PKG_SHA1 | XML_ATTR_PKG_URL |
							  XML_ATTR_PKG_CONTENT_ID;
				if ((xml->attrMask & reqMask) != reqMask)
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				if (xml->hasExplicitPkgType == 1 &&
				    (xml->attrMask & XML_ATTR_PKG_TYPE) == 0)
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
			} else if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP) {
				const SceUInt32 reqMask = XML_ATTR_PKG_VERSION | XML_ATTR_PKG_SIZE |
							  XML_ATTR_PKG_SHA1 | XML_ATTR_PKG_URL;
				if ((xml->attrMask & reqMask) != reqMask)
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
			}

			bool isNewerThanInstalled =
				(xml->currentPkg.versionMajor > core->currentVerMajor) ||
				(xml->currentPkg.versionMajor == core->currentVerMajor &&
				 xml->currentPkg.versionMinor > core->currentVerMinor);

			if (isNewerThanInstalled &&
			    sceClibStrncasecmp(xml->currentPkg.contentId, core->contentId, 48) == 0) {
				if (xml->currentPkg.versionMajor > core->latestVerMajor ||
				    (xml->currentPkg.versionMajor == core->latestVerMajor &&
				     xml->currentPkg.versionMinor > core->latestVerMinor)) {
					core->latestVerMajor = xml->currentPkg.versionMajor;
					core->latestVerMinor = xml->currentPkg.versionMinor;
				}

				if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP2) {
					if (xml->currentPkg.requiredPsp2SysVer > core->minPsp2SysVer)
						core->minPsp2SysVer =
							xml->currentPkg.requiredPsp2SysVer;
				} else if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP) {
					if (xml->currentPkg.requiredPspSysVerMajor >
						    core->minPspSysVerMajor ||
					    (xml->currentPkg.requiredPspSysVerMajor ==
						     core->minPspSysVerMajor &&
					     xml->currentPkg.requiredPspSysVerMinor >
						     core->minPspSysVerMinor)) {
						core->minPspSysVerMajor =
							xml->currentPkg.requiredPspSysVerMajor;
						core->minPspSysVerMinor =
							xml->currentPkg.requiredPspSysVerMinor;
					}
				}

				if (core->allocFunc != NULL && core->freeFunc != NULL) {
					GameUpdatePackageNode *node =
						(GameUpdatePackageNode *)core->allocFunc(
							sizeof(GameUpdatePackageNode));
					if (node == NULL)
						return SCE_GAMEUPDATE_ERROR_NO_MEMORY;

					sceClibMemcpy_safe(node, &xml->currentPkg,
							   sizeof(GameUpdatePackageNode));

					GameUpdatePackageNode *prev = NULL;
					GameUpdatePackageNode *cur = core->packageListHead;
					bool duplicate = false;
					while (cur != NULL) {
						if (node->versionMajor < cur->versionMajor ||
						    (node->versionMajor == cur->versionMajor &&
						     node->versionMinor < cur->versionMinor))
							break;
						if (node->versionMajor == cur->versionMajor &&
						    node->versionMinor == cur->versionMinor) {
							duplicate = true;
							break;
						}
						prev = cur;
						cur = cur->next;
					}
					if (duplicate) {
						core->freeFunc(node);
						return SCE_GAMEUPDATE_ERROR_XML_VALUE;
					}
					if (prev != NULL) {
						node->next = prev->next;
						prev->next = node;
					} else {
						node->next = core->packageListHead;
						core->packageListHead = node;
					}
				}
			}

			core->packageCount++;
			sceClibMemset(&xml->currentPkg, 0, sizeof(xml->currentPkg));
			xml->attrMask &= ~(XML_ATTR_PKG_VERSION | XML_ATTR_PKG_SIZE |
					   XML_ATTR_PKG_SHA1 | XML_ATTR_PKG_URL |
					   XML_ATTR_PKG_CONTENT_ID | XML_ATTR_PKG_PSP2_SYSVER |
					   XML_ATTR_PKG_PSP_SYSVER | XML_ATTR_PKG_TYPE |
					   XML_ATTR_CHANGEINFO_URL | XML_ATTR_CHANGEINFO_LANG_URL |
					   XML_ATTR_TITLE_DEFAULT | XML_ATTR_TITLE_LANG);
			xml->parseState = XML_STATE_TAG;
		} else if (depth == 3 && index == 0) {
			if (xml->parseState != XML_STATE_HYBRID_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			const SceUInt32 reqMask = XML_ATTR_HYBRID_SIZE | XML_ATTR_HYBRID_SHA1 |
						  XML_ATTR_HYBRID_URL | XML_ATTR_HYBRID_CONTENT_ID;
			if ((xml->attrMask & reqMask) != reqMask)
				return SCE_GAMEUPDATE_ERROR_XML_VALUE;
			xml->hybridIsNewer = 0;
			xml->attrMask &= ~reqMask;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 1) {
			if (xml->parseState != XML_STATE_PARAMSFO)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 2) {
			if (xml->parseState != XML_STATE_CHANGEINFO)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 3) {
			if (xml->parseState != XML_STATE_CHANGEINFO_LANG)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 4) {
			if (xml->parseState != XML_STATE_URL)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			const SceUInt32 reqMask = XML_ATTR_URL_SIZE | XML_ATTR_URL_SHA1 |
						  XML_ATTR_URL_HREF | XML_ATTR_URL_CONTENT_ID;
			if ((xml->attrMask & reqMask) != reqMask)
				return SCE_GAMEUPDATE_ERROR_XML_VALUE;
			if (sceClibStrncasecmp(xml->altUrlPkg.contentId, core->contentId, 48) == 0) {
				xml->currentPkg.packageSize = xml->altUrlPkg.packageSize;
				sceClibMemcpy_safe(xml->currentPkg.sha1Digest,
						   xml->altUrlPkg.sha1Digest, 20);
				sceClibMemcpy_safe(xml->currentPkg.packageUrl,
						   xml->altUrlPkg.packageUrl, 256);
				sceClibMemcpy_safe(xml->currentPkg.contentId,
						   xml->altUrlPkg.contentId, 48);
			}
			sceClibMemset(&xml->altUrlPkg, 0, sizeof(xml->altUrlPkg));
			xml->attrMask &= ~reqMask;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 4 && index == 0) {
			if (xml->parseState != XML_STATE_TITLE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_PARAMSFO;
		} else if (depth == 4 && index == 1) {
			if (xml->parseState != XML_STATE_TITLE_LANG)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			xml->parseState = XML_STATE_PARAMSFO;
		}
		return 0;
	}

	if (event == NP_XML_EVENT_TEXT) {
		if (depth == 4 && index == 0) {
			if (xml->parseState != XML_STATE_TITLE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if ((xml->attrMask & XML_ATTR_TITLE_LANG) == 0)
				sceClibStrlcpy(xml->currentPkg.titleName, name, 128);
			xml->attrMask |= XML_ATTR_TITLE_DEFAULT;
			xml->parseState = XML_STATE_PARAMSFO;
		} else if (depth == 4 && index == 1) {
			if (xml->parseState != XML_STATE_TITLE_LANG)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if ((xml->attrMask & XML_ATTR_TITLE_DEFAULT) != 0)
				sceClibMemset(xml->currentPkg.titleName, 0, 128);
			sceClibStrlcpy(xml->currentPkg.titleName, name, 128);
			xml->attrMask |= XML_ATTR_TITLE_LANG;
			xml->parseState = XML_STATE_PARAMSFO;
		}
		return 0;
	}

	if (event == NP_XML_EVENT_ATTRIBUTE) {
		if (depth == 0 && index == 0) {
			if (xml->parseState != XML_STATE_TITLEPATCH)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (sceClibStrncasecmp(name, "titleid", 8) == 0) {
				(void)sceClibStrncasecmp(value, core->queryTitleId, 13);
				xml->attrMask |= XML_ATTR_TITLEID;
			} else if (sceClibStrncasecmp(name, "status", 7) == 0) {
				core->titleStatusNotAlive =
					(sceClibStrncasecmp(value, "alive", 6) != 0) ? 1 : 0;
				xml->attrMask |= XML_ATTR_STATUS;
			}
		} else if (depth == 1 && index == 0) {
			if (xml->parseState != XML_STATE_TAG)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (sceClibStrncasecmp(name, "name", 5) == 0) {
				sceClibStrlcpy(core->tagName, value, 17);
				xml->attrMask |= XML_ATTR_TAG_NAME;
			} else if (sceClibStrncasecmp(name, "signoff", 8) == 0) {
				if (sceClibStrncasecmp(value, "true", 5) == 0)
					core->infoFlags |= SCE_GAMEUPDATE_INFO_FLAG_SIGNOFF;
				xml->attrMask |= XML_ATTR_TAG_SIGNOFF;
			} else if (sceClibStrncasecmp(name, "min_system_ver", 15) == 0) {
				if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP2) {
					SceUInt32 sysVer = 0;
					if (!parseDecimalU32(value, &sysVer))
						return SCE_GAMEUPDATE_ERROR_XML_VALUE;
					if (sysVer > core->minPsp2SysVer)
						core->minPsp2SysVer = sysVer;
				} else if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP) {
					if (parseVersionString(value, &parsedMajor, &parsedMinor,
							       VERSION_FMT_MAJOR2_MINOR2 |
							       VERSION_FMT_MAJOR1_MINOR2) < 0)
						return SCE_GAMEUPDATE_ERROR_XML_VALUE;
					if (parsedMajor > core->minPspSysVerMajor ||
					    (parsedMajor == core->minPspSysVerMajor &&
					     parsedMinor > core->minPspSysVerMinor)) {
						core->minPspSysVerMajor = parsedMajor;
						core->minPspSysVerMinor = parsedMinor;
					}
				}
				xml->attrMask |= XML_ATTR_TAG_MIN_SYSVER;
			}
		} else if (depth == 2 && index == 0) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (sceClibStrncasecmp(name, "version", 8) == 0) {
				if (parseVersionString(value, &xml->currentPkg.versionMajor,
						       &xml->currentPkg.versionMinor,
						       VERSION_FMT_MAJOR2_MINOR2) < 0)
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->attrMask |= XML_ATTR_PKG_VERSION;
			} else if (sceClibStrncasecmp(name, "size", 5) == 0) {
				char *endPtr = NULL;
				xml->currentPkg.packageSize =
					(SceUInt64)sceClibStrtoll(value, &endPtr, 10);
				if (endPtr == NULL || *endPtr != '\0')
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->attrMask |= XML_ATTR_PKG_SIZE;
			} else if (sceClibStrncasecmp(name, "sha1sum", 8) == 0) {
				if (!parseSha1Hex(value, xml->currentPkg.sha1Digest))
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->attrMask |= XML_ATTR_PKG_SHA1;
			} else if (sceClibStrncasecmp(name, "url", 4) == 0) {
				sceClibStrlcpy(xml->currentPkg.packageUrl, value, 256);
				xml->attrMask |= XML_ATTR_PKG_URL;
			} else if (sceClibStrncasecmp(name, "psp2_system_ver", 16) == 0) {
				SceUInt32 sysVer = 0;
				if (!parseDecimalU32(value, &sysVer))
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->currentPkg.requiredPsp2SysVer = sysVer;
				xml->attrMask |= XML_ATTR_PKG_PSP2_SYSVER;
			} else if (sceClibStrncasecmp(name, "psp_system_ver", 15) == 0) {
				if (parseVersionString(value,
						       &xml->currentPkg.requiredPspSysVerMajor,
						       &xml->currentPkg.requiredPspSysVerMinor,
						       VERSION_FMT_MAJOR2_MINOR2 |
						       VERSION_FMT_MAJOR1_MINOR2) < 0)
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->attrMask |= XML_ATTR_PKG_PSP_SYSVER;
			} else if (sceClibStrncasecmp(name, "content_id", 11) == 0) {
				sceClibStrlcpy(xml->currentPkg.contentId, value, 48);
				xml->attrMask |= XML_ATTR_PKG_CONTENT_ID;
			} else if (sceClibStrncasecmp(name, "type", 5) == 0) {
				if (sceClibStrncasecmp(value, "cumulative", 11) == 0)
					xml->currentPkg.packageType = SCE_GAMEUPDATE_PKG_CUMULATIVE;
				else if (sceClibStrncasecmp(value, "incremental", 12) == 0)
					xml->currentPkg.packageType = SCE_GAMEUPDATE_PKG_INCREMENTAL;
				else
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->hasExplicitPkgType = 1;
				xml->attrMask |= XML_ATTR_PKG_TYPE;
			}
		} else if (depth == 3 && index == 0) {
			if (xml->parseState != XML_STATE_HYBRID_PACKAGE)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (sceClibStrncasecmp(name, "size", 5) == 0) {
				if (xml->hybridIsNewer == 1) {
					char *endPtr = NULL;
					xml->hybridPkg.packageSize =
						(SceUInt64)sceClibStrtoll(value, &endPtr, 10);
					if (endPtr == NULL || *endPtr != '\0')
						return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				}
				xml->attrMask |= XML_ATTR_HYBRID_SIZE;
			} else if (sceClibStrncasecmp(name, "sha1sum", 8) == 0) {
				if (sceClibStrnlen(value, 41) != 40)
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				if (xml->hybridIsNewer == 1 &&
				    !parseSha1Hex(value, xml->hybridPkg.sha1Digest))
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->attrMask |= XML_ATTR_HYBRID_SHA1;
			} else if (sceClibStrncasecmp(name, "url", 4) == 0) {
				if (xml->hybridIsNewer == 1)
					sceClibStrlcpy(xml->hybridPkg.packageUrl, value, 256);
				xml->attrMask |= XML_ATTR_HYBRID_URL;
			} else if (sceClibStrncasecmp(name, "content_id", 11) == 0) {
				if (xml->hybridIsNewer == 1)
					sceClibStrlcpy(xml->hybridPkg.contentId, value, 48);
				xml->attrMask |= XML_ATTR_HYBRID_CONTENT_ID;
			}
		} else if (depth == 3 && index == 2) {
			if (xml->parseState != XML_STATE_CHANGEINFO)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (sceClibStrncasecmp(name, "url", 4) == 0) {
				if ((xml->attrMask & XML_ATTR_CHANGEINFO_LANG_URL) == 0)
					sceClibStrlcpy(xml->currentPkg.changeInfoUrl, value, 256);
				xml->attrMask |= XML_ATTR_CHANGEINFO_URL;
			}
		} else if (depth == 3 && index == 3) {
			if (xml->parseState != XML_STATE_CHANGEINFO_LANG)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (sceClibStrncasecmp(name, "url", 4) == 0) {
				if ((xml->attrMask & XML_ATTR_CHANGEINFO_URL) != 0)
					sceClibMemset(xml->currentPkg.changeInfoUrl, 0, 256);
				sceClibStrlcpy(xml->currentPkg.changeInfoUrl, value, 256);
				xml->attrMask |= XML_ATTR_CHANGEINFO_LANG_URL;
			}
		} else if (depth == 3 && index == 4) {
			if (xml->parseState != XML_STATE_URL)
				return SCE_GAMEUPDATE_ERROR_XML_SYNTAX;
			if (sceClibStrncasecmp(name, "size", 5) == 0) {
				char *endPtr = NULL;
				xml->altUrlPkg.packageSize =
					(SceUInt64)sceClibStrtoll(value, &endPtr, 10);
				if (endPtr == NULL || *endPtr != '\0')
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->attrMask |= XML_ATTR_URL_SIZE;
			} else if (sceClibStrncasecmp(name, "sha1sum", 8) == 0) {
				if (!parseSha1Hex(value, xml->altUrlPkg.sha1Digest))
					return SCE_GAMEUPDATE_ERROR_XML_VALUE;
				xml->attrMask |= XML_ATTR_URL_SHA1;
			} else if (sceClibStrncasecmp(name, "url", 4) == 0) {
				sceClibStrlcpy(xml->altUrlPkg.packageUrl, value, 256);
				xml->attrMask |= XML_ATTR_URL_HREF;
			} else if (sceClibStrncasecmp(name, "content_id", 11) == 0) {
				sceClibStrlcpy(xml->altUrlPkg.contentId, value, 48);
				xml->attrMask |= XML_ATTR_URL_CONTENT_ID;
			}
		}
	}

	return 0;
}

/* Initializes the streaming XML parser with language-specific element names (0x812AA824) */
static int gameUpdateXmlInit(SceGameUpdateContext *context)
{
	int lang = 1;
	sceRegMgrSystemParamGetInt(REG_KEY_SYSTEM_LANGUAGE, &lang);
	s_titleLangTag[6] = (char)((lang / 10) + '0');
	s_titleLangTag[7] = (char)((lang % 10) + '0');
	s_changeinfoLangTag[11] = (char)((lang / 10) + '0');
	s_changeinfoLangTag[12] = (char)((lang % 10) + '0');
	return SceNpCommon_650D111B(&context->xml->parser, &s_xmlSchema,
				    onGameUpdateXmlEvent, context);
}

/* Executes the HTTP GET request and streams the XML body into the parser (0x812AA4EE) */
static int gameUpdateHttpPerform(SceGameUpdateContext *context)
{
	GameUpdateCore *core = context->core;
	GameUpdateHttpWork *http = context->http;

	bool envSupported = false;
	for (int i = 0; s_allowedNpEnvs[i] != NULL; ++i) {
		if (sceClibStrncasecmp(s_allowedNpEnvs[i], http->npEnv, 17) == 0) {
			envSupported = true;
			break;
		}
	}
	if (!envSupported)
		return 0;

	int ret = sceHttpSendRequest(http->requestId, NULL, 0);
	if (ret < 0)
		return ret;

	ret = sceHttpGetStatusCode(http->requestId, &http->statusCode);
	if (ret < 0)
		return ret;

	int status = http->statusCode;
	if (status == 403 || status == 404) {
		http->contentLength = 0;
		core->infoFlags |= SCE_GAMEUPDATE_INFO_FLAG_SIGNOFF;
		return 0;
	}
	if (core->targetPlatform == SCE_GAMEUPDATE_PLATFORM_PSP2 &&
	    (status == 500 || status == 501 || status == 503)) {
		http->contentLength = 0;
		core->infoFlags |= SCE_GAMEUPDATE_INFO_FLAG_SIGNOFF;
		return 0;
	}
	if (status != 200)
		return SCE_GAMEUPDATE_ERROR_HTTP_FAILED;

	ret = sceHttpGetResponseContentLength(http->requestId, &http->contentLength);
	if (ret < 0)
		return ret;

	if (http->contentLength > 0xFFFFu)
		return SCE_GAMEUPDATE_ERROR_HTTP_FAILED;

	if (http->contentLength == 0)
		return 0;

	SceUInt64 totalRead = 0;
	while (totalRead < http->contentLength) {
		int nread = sceHttpReadData(http->requestId, http->readBuffer,
					    sizeof(http->readBuffer));
		if (nread < 0)
			return nread;
		ret = gameUpdateXmlFeed(context, http->readBuffer, nread);
		if (ret < 0)
			return ret;
		totalRead += (SceUInt64)nread;
	}
	return gameUpdateXmlFinalize(context);
}

/* Releases all allocations and HTTP handles owned by a SceGameUpdateContext (0x812A9708) */
int sceGameUpdateTerm(SceGameUpdateContext *context)
{
	if (context == NULL)
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
	GameUpdateCore *core = context->core;
	if (core == NULL)
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;

	void (*freeFunc)(void *) = core->freeFunc;
	int ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_RUNNING) {
		gameUpdateHttpAbort(context);
		core->lifecycleState = GAMEUPDATE_LIFECYCLE_ABORTING;
	}

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_ABORTING) {
		while (core->lifecycleState != GAMEUPDATE_LIFECYCLE_DONE) {
			sceKernelUnlockLwMutex(&core->mutex, 1);
			sceKernelDelayThread(100000);
			ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
			if (ret < 0)
				return ret;
		}
	}

	if (core->lifecycleState != GAMEUPDATE_LIFECYCLE_READY &&
	    core->lifecycleState != GAMEUPDATE_LIFECYCLE_DONE) {
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return SCE_GAMEUPDATE_ERROR_INVALID_STATE;
	}

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_DONE && core->freeFunc != NULL) {
		GameUpdatePackageNode *node = core->packageListHead;
		while (node != NULL) {
			GameUpdatePackageNode *nextNode = node->next;
			core->freeFunc(node);
			core->packageListHead = nextNode;
			node = nextNode;
		}
	}

	gameUpdateHttpDeleteRequest(context);
	gameUpdateHttpDeleteTemplate(context);
	core->lifecycleState = GAMEUPDATE_LIFECYCLE_UNINIT;
	sceKernelDeleteLwMutex(&core->mutex);

	if (context->core != NULL) {
		freeFunc(context->core);
		context->core = NULL;
	}
	if (context->http != NULL) {
		freeFunc(context->http);
		context->http = NULL;
	}
	if (context->xml != NULL) {
		freeFunc(context->xml);
		context->xml = NULL;
	}
	return ret;
}

/* Aborts an in-flight update check (0x812A9806) */
int sceGameUpdateAbort(SceGameUpdateContext *context)
{
	if (context == NULL)
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
	GameUpdateCore *core = context->core;
	if (core == NULL)
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;

	int ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_RUNNING) {
		gameUpdateHttpAbort(context);
		core->lifecycleState = GAMEUPDATE_LIFECYCLE_ABORTING;
	}
	sceKernelUnlockLwMutex(&core->mutex, 1);
	return ret;
}

/* Copies out metadata for packageIndex from the parsed package list (0x812A985C) */
int sceGameUpdateGetPackageInfo(SceGameUpdateContext *context, SceUInt32 packageIndex,
				SceGameUpdatePackageInfo *outPackage)
{
	if (context == NULL || outPackage == NULL)
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
	GameUpdateCore *core = context->core;
	if (core == NULL)
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;

	int ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_DONE) {
		GameUpdatePackageNode *node = core->packageListHead;
		SceUInt32 idx = 0;
		while (node != NULL && idx < packageIndex) {
			node = node->next;
			++idx;
		}
		if (node != NULL) {
			outPackage->packageSize = node->packageSize;
			sceClibMemcpy_safe(outPackage->sha1Digest, node->sha1Digest, 20);
			sceClibMemcpy_safe(outPackage->packageUrl, node->packageUrl, 256);
			if (core->countryCode[0] != '\0') {
				sceClibStrlcat(outPackage->packageUrl, "?country=", 268);
				sceClibStrlcat(outPackage->packageUrl, core->countryCode, 268);
			}
			outPackage->packageType = node->packageType;
		} else {
			ret = SCE_GAMEUPDATE_ERROR_NO_PACKAGE;
		}
	} else if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_READY ||
		   core->lifecycleState == GAMEUPDATE_LIFECYCLE_RUNNING ||
		   core->lifecycleState == GAMEUPDATE_LIFECYCLE_ABORTING) {
		ret = SCE_GAMEUPDATE_ERROR_NO_PACKAGE;
	} else {
		ret = SCE_GAMEUPDATE_ERROR_INVALID_STATE;
	}

	sceKernelUnlockLwMutex(&core->mutex, 1);
	return ret;
}

/* Records a patch availability status into the shell's update cache (0x812A9954) */
int sceGameUpdateRecordCacheStatus(const char *titleId, SceInt32 status)
{
	GameUpdateCacheEntry entry;
	sceClibMemset(&entry, 0, sizeof(entry));

	if (status == SCE_GAMEUPDATE_STATUS_UP_TO_DATE) {
		entry.networkTick = 0;
	} else {
		SceRtcTick tick;
		sceRtcGetCurrentNetworkTick(&tick);
		entry.networkTick = tick.tick;
	}
	entry.status = status;
	return shellGameUpdateEnqueueCacheWrite(titleId, &entry, 3);
}

/* Allocates and initializes a SceGameUpdateContext (0x812A99AE) */
int sceGameUpdateInit(SceGameUpdateContext *context, int option,
		      const SceGameUpdateInitParam *param)
{
	if (context == NULL || option != 0 || param == NULL)
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;

	sceClibMemset(context, 0, sizeof(*context));

	int ret = 0;
	context->core = (GameUpdateCore *)param->allocFunc(sizeof(GameUpdateCore));
	if (context->core == NULL) {
		ret = SCE_GAMEUPDATE_ERROR_NO_MEMORY;
	} else {
		sceClibMemset(context->core, 0, sizeof(GameUpdateCore));
		context->http = (GameUpdateHttpWork *)param->allocFunc(sizeof(GameUpdateHttpWork));
		if (context->http == NULL) {
			ret = SCE_GAMEUPDATE_ERROR_NO_MEMORY;
		} else {
			sceClibMemset(context->http, 0, sizeof(GameUpdateHttpWork));
			context->xml = (GameUpdateXmlWork *)param->allocFunc(sizeof(GameUpdateXmlWork));
			if (context->xml == NULL)
				ret = SCE_GAMEUPDATE_ERROR_NO_MEMORY;
			else
				sceClibMemset(context->xml, 0, sizeof(GameUpdateXmlWork));
		}
	}

	if (ret == 0) {
		GameUpdateCore *core = context->core;
		core->allocFunc = param->allocFunc;
		core->freeFunc = param->freeFunc;

		SceInt32 platform = param->targetPlatform;
		if (platform == SCE_GAMEUPDATE_PLATFORM_DEFAULT)
			platform = SCE_GAMEUPDATE_PLATFORM_PSP2;

		if (platform != SCE_GAMEUPDATE_PLATFORM_PSP &&
		    platform != SCE_GAMEUPDATE_PLATFORM_PSP2) {
			ret = SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
		} else {
			core->targetPlatform = platform;
			if (param->patchTitleId[0] != '\0')
				sceClibMemcpy_safe(core->queryTitleId, param->patchTitleId, 13);
			else
				sceClibMemcpy_safe(core->queryTitleId, param->titleId, 13);
			sceClibMemcpy_safe(core->originalTitleId, param->titleId, 13);

			if (param->currentAppVer[0] != '\0') {
				ret = parseVersionString(param->currentAppVer,
							 &core->currentVerMajor,
							 &core->currentVerMinor,
							 VERSION_FMT_MAJOR2_MINOR2);
			}
			if (ret >= 0) {
				core->currentPsp2SysVer = param->currentPsp2SysVer;
				core->currentPspSysVerMajor = (SceUInt16)(param->currentPspSysVer / 100);
				core->currentPspSysVerMinor = (SceUInt16)(param->currentPspSysVer % 100);
				sceClibMemcpy_safe(core->contentId, param->contentId, 48);
				core->initFlags = param->flags;

				ret = sceKernelCreateLwMutex(&core->mutex, "SceGameUpdate", 2, 0, NULL);
				if (ret >= 0) {
					ret = gameUpdateHttpReset(context);
					if (ret >= 0) {
						ret = gameUpdateHttpCreateTemplate(context);
						if (ret >= 0) {
							ret = gameUpdateHttpCreateRequest(context);
							if (ret >= 0) {
								ret = gameUpdateXmlInit(context);
								if (ret >= 0) {
									sceRegMgrSystemParamGetStr(
										REG_KEY_NP_COUNTRY,
										core->countryCode, 3);
									core->lifecycleState =
										GAMEUPDATE_LIFECYCLE_READY;
									return ret;
								}
								gameUpdateHttpDeleteRequest(context);
							}
							gameUpdateHttpDeleteTemplate(context);
						}
					}
					sceKernelDeleteLwMutex(&core->mutex);
				}
			}
		}
	}

	if (context->core != NULL) {
		param->freeFunc(context->core);
		context->core = NULL;
	}
	if (context->http != NULL) {
		param->freeFunc(context->http);
		context->http = NULL;
	}
	if (context->xml != NULL) {
		param->freeFunc(context->xml);
		context->xml = NULL;
	}
	return ret;
}

/* Executes the synchronous titlepatch query and populates result & info (0x812A9BB0) */
int sceGameUpdateRun(SceGameUpdateContext *context, SceGameUpdateResult *result,
		     SceGameUpdateInfo *info)
{
	if (context == NULL || result == NULL || info == NULL)
		return SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
	GameUpdateCore *core = context->core;
	if (core == NULL)
		return SCE_GAMEUPDATE_ERROR_NOT_INITIALIZED;

	int ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_RUNNING ||
	    core->lifecycleState == GAMEUPDATE_LIFECYCLE_ABORTING) {
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return SCE_GAMEUPDATE_ERROR_ALREADY_RUNNING;
	}
	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_DONE) {
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return 0;
	}
	if (core->lifecycleState != GAMEUPDATE_LIFECYCLE_READY) {
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return SCE_GAMEUPDATE_ERROR_INVALID_STATE;
	}

	core->lifecycleState = GAMEUPDATE_LIFECYCLE_RUNNING;
	sceKernelUnlockLwMutex(&core->mutex, 1);

	int runResult = 0;
	bool shouldUpdateCache = false;

	if (result->size != sizeof(SceGameUpdateResult)) {
		runResult = SCE_GAMEUPDATE_ERROR_INVALID_ARGUMENT;
	} else {
		int debugUpgradable = 0;
		runResult = sceRegMgrSystemParamGetInt(REG_KEY_NP_DEBUG_UPGRADABLE,
							      &debugUpgradable);
		if (runResult == 0 && debugUpgradable != 0) {
			result->status = SCE_GAMEUPDATE_STATUS_AVAILABLE;
			result->latestVersion[0] = '0';
			result->latestVersion[1] = '0';
			result->latestVersion[2] = '.';
			result->latestVersion[3] = '0';
			result->latestVersion[4] = '0';
			result->latestVersion[5] = '\0';
			info->flags = SCE_GAMEUPDATE_INFO_FLAG_SIGNOFF |
				      SCE_GAMEUPDATE_INFO_FLAG_DEBUG_MOCK;
		} else {
			runResult = gameUpdateHttpPerform(context);
			if (runResult >= 0) {
				if (core->tagName[0] != '\0')
					SceNpManager_9554802B("TPPS Tag", core->tagName, 0);
				sceClibStrlcpy(info->tagName, core->tagName, 17);
				info->flags = core->infoFlags;

				bool resolvedByBgdl = false;
				if ((core->initFlags & SCE_GAMEUPDATE_OPT_SKIP_BGDL_CHECK) == 0) {
					BgdlPatchTaskStatus bgdlStatus;
					shellBgdlPatchTaskStatusInit(&bgdlStatus);
					if (shellBgdlFindPatchTaskByTitleId(core->originalTitleId,
									    &bgdlStatus) >= 0) {
						if (bgdlStatus.taskState == 0) {
							shellBgdlDeletePatchTasksByTitleId(
								core->originalTitleId);
						} else if (bgdlStatus.taskState >= 1 &&
							   bgdlStatus.taskState <= 3) {
							result->status =
								SCE_GAMEUPDATE_STATUS_DOWNLOADING;
							resolvedByBgdl = true;
						} else if (bgdlStatus.taskState == 4) {
							result->status =
								SCE_GAMEUPDATE_STATUS_READY_TO_INSTALL;
							resolvedByBgdl = true;
						}
					}
				}

				if (!resolvedByBgdl) {
					if (core->titleStatusNotAlive == 1) {
						result->status = SCE_GAMEUPDATE_STATUS_DISCONTINUED;
					} else if (core->targetPlatform ==
							   SCE_GAMEUPDATE_PLATFORM_PSP2 &&
						   core->minPsp2SysVer > core->currentPsp2SysVer) {
						result->status =
							SCE_GAMEUPDATE_STATUS_SYSUPDATE_NEEDED;
					} else if (core->targetPlatform ==
							   SCE_GAMEUPDATE_PLATFORM_PSP &&
						   (core->minPspSysVerMajor >
							    core->currentPspSysVerMajor ||
						    (core->minPspSysVerMajor ==
							     core->currentPspSysVerMajor &&
						     core->minPspSysVerMinor >
							     core->currentPspSysVerMinor))) {
						result->status =
							SCE_GAMEUPDATE_STATUS_SYSUPDATE_NEEDED;
					} else if (core->latestVerMajor != 0 ||
						   core->latestVerMinor != 0) {
						result->latestVersion[0] =
							(char)((core->latestVerMajor / 10) + '0');
						result->latestVersion[1] =
							(char)((core->latestVerMajor % 10) + '0');
						result->latestVersion[2] = '.';
						result->latestVersion[3] =
							(char)((core->latestVerMinor / 10) + '0');
						result->latestVersion[4] =
							(char)((core->latestVerMinor % 10) + '0');
						result->latestVersion[5] = '\0';

						SceUInt32 count = 1;
						GameUpdatePackageNode *lastNode =
							core->packageListHead;
						while (lastNode->next != NULL) {
							lastNode = lastNode->next;
							++count;
						}
						info->packageCount = count;
						sceClibStrlcpy(info->titleName,
							       lastNode->titleName, 128);
						sceClibStrlcpy(info->changeInfoUrl,
							       lastNode->changeInfoUrl, 256);
						result->status = SCE_GAMEUPDATE_STATUS_AVAILABLE;
					} else {
						result->status = SCE_GAMEUPDATE_STATUS_UP_TO_DATE;
					}
				}

				shouldUpdateCache =
					((info->flags & SCE_GAMEUPDATE_INFO_FLAG_DEBUG_MOCK) == 0) &&
					(result->status != SCE_GAMEUPDATE_STATUS_DISCONTINUED);
			}
		}
	}

	if (shouldUpdateCache) {
		SceRtcTick tick;
		sceRtcGetCurrentNetworkTick(&tick);
		GameUpdateCacheEntry cacheEntry;
		sceClibMemset(&cacheEntry, 0, sizeof(cacheEntry));
		cacheEntry.networkTick = tick.tick;
		cacheEntry.status = result->status;
		cacheEntry.flags = info->flags & SCE_GAMEUPDATE_INFO_FLAG_SIGNOFF;
		shellGameUpdateEnqueueCacheWrite(core->originalTitleId, &cacheEntry, -1);
	}

	ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_ABORTING)
		runResult = SCE_GAMEUPDATE_ERROR_ABORTED;
	core->lifecycleState = GAMEUPDATE_LIFECYCLE_DONE;
	sceKernelUnlockLwMutex(&core->mutex, 1);
	return runResult;
}

/* Thin wrapper entry points (0x812A9FF0, 0x812A9FF8) */
int sceGameUpdateInitEx(SceGameUpdateContext *context, int option,
			const SceGameUpdateInitParam *param)
{
	return sceGameUpdateInit(context, option, param);
}

int sceGameUpdateRunEx(SceGameUpdateContext *context, SceGameUpdateResult *result,
		       SceGameUpdateInfo *info)
{
	return sceGameUpdateRun(context, result, info);
}
