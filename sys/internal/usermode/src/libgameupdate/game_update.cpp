/*
 * game_update.cpp: SceGameUpdate core HTTP/HMAC/XML patch check client and
 * public SDK exports (vs0:sys/external/libgameupdate.suprx, 0x81000000-0x810021F3).
 */
#include "libgameupdate_internal.h"

extern "C" {

/* External library imports not yet in shared tree headers */
struct SceAppMgrStatus {
	SceUInt32 header[3];
	char titleId[116];
};

typedef int (*GameUpdateHttpAuthCallback)(int request, int authType, const char *realm,
					  char *username, char *password, bool isProxy,
					  char *domain, SceSize *domainLen,
					  bool *save, void *userArg);

int sceKernelDelayThread(SceUInt32 delay);
int sceAppMgrGetStatusByName(const char *name, SceAppMgrStatus *status);
int sceAppMgrAppParamGetString(SceUID pid, int param, char *buf, SceSize size);
int sceAppMgrAppParamGetInt(SceUID pid, int param, int *value);
void *SceNetInternal_A2C2BC5F(SceSize size, int flags);
void SceNetInternal_E62A2FAA(void *ptr);

int sceHttpCreateTemplate(const char *userAgent, int httpVer, int autoProxyConf);
int sceHttpDeleteTemplate(int tmplId);
int sceHttpSetConnectTimeOut(int id, SceUInt32 usec);
int sceHttpSetRecvTimeOut(int id, SceUInt32 usec);
int sceHttpSetSendTimeOut(int id, SceUInt32 usec);
int sceHttpRemoveRequestHeader(int id, const char *name);
int sceHttpsEnableOptionPrivate(int id, SceUInt32 option);
int sceHttpsDisableOptionPrivate(int id, SceUInt32 option);
int SceHttp_53B722F2(int tmplId, int enable, int proxyType, const char *host, SceUInt16 port);
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

} /* extern "C" */

/* System registry keys */
#define REG_KEY_NP_ENV			210968		/* /CONFIG/NP/env */
#define REG_KEY_SYSTEM_LANGUAGE		226562		/* /CONFIG/SYSTEM/language */
#define REG_KEY_HTTP_PROXY_PASS		1477531		/* /CONFIG/NET/http_proxy_pass */
#define REG_KEY_HTTP_PROXY_PORT		2273681		/* /CONFIG/NET/http_proxy_port */
#define REG_KEY_NP_DEBUG_UPGRADABLE	3225861		/* /CONFIG/NP/debug_upgradable */
#define REG_KEY_HTTP_PROXY_SERVER	6580878		/* /CONFIG/NET/http_proxy_server */
#define REG_KEY_HTTP_PROXY_FLAG		7338025		/* /CONFIG/NET/http_proxy_flag */
#define REG_KEY_HTTP_PROXY_USER		9352296		/* /CONFIG/NET/http_proxy_user */
#define REG_KEY_NP_COUNTRY		9409785		/* /CONFIG/NP/country */

enum GameUpdateLifecycleState {
	GAMEUPDATE_LIFECYCLE_UNINIT   = 0,
	GAMEUPDATE_LIFECYCLE_READY    = 1,
	GAMEUPDATE_LIFECYCLE_RUNNING  = 2,
	GAMEUPDATE_LIFECYCLE_ABORTING = 3,
	GAMEUPDATE_LIFECYCLE_DONE     = 4
};

enum GameUpdateXmlParseState {
	XML_STATE_ROOT            = 0,
	XML_STATE_TITLEPATCH      = 1,
	XML_STATE_TAG             = 2,
	XML_STATE_PACKAGE         = 3,
	XML_STATE_HYBRID_PACKAGE  = 4,
	XML_STATE_PARAMSFO        = 5,
	XML_STATE_TITLE           = 6,
	XML_STATE_TITLE_LANG      = 7,
	XML_STATE_CHANGEINFO      = 8,
	XML_STATE_CHANGEINFO_LANG = 9,
	XML_STATE_URL             = 10
};

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

#define VERSION_FMT_MAJOR2_MINOR2	0x01
#define VERSION_FMT_MAJOR2_MINOR4	0x02
#define VERSION_FMT_MAJOR1_MINOR2	0x04

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

struct GameUpdateCore {
	SceInt32 lifecycleState;		/* +0x00 */
	SceUInt32 alignmentPad;			/* +0x04 */
	SceKernelLwMutexWork mutex;		/* +0x08 */
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

struct GameUpdateHttpWork {
	SceInt32 templateId;			/* +0x000 */
	SceInt32 connectionId;			/* +0x004 */
	SceInt32 requestId;			/* +0x008 */
	SceInt32 statusCode;			/* +0x00C */
	SceUInt64 contentLength;		/* +0x010 */
	char readBuffer[2049];			/* +0x018 */
	char proxyUser[65];			/* +0x819 */
	char proxyPass[65];			/* +0x85A */
	char npEnv[17];				/* +0x89B */
	char trailingPad[4];			/* +0x8AC */
};

struct GameUpdateXmlWork {
	NpXmlParser parser;			/* +0x000 */
	SceInt32 parseState;			/* +0x248 */
	SceUInt32 attrMask;			/* +0x24C */
	SceUInt64 headerPad;			/* +0x250 */
	GameUpdatePackageNode currentPkg;	/* +0x258 */
	GameUpdateAltPackage altUrlPkg;		/* +0x538 */
	GameUpdateAltPackage hybridPkg;		/* +0x690 */
	SceInt32 hybridIsNewer;			/* +0x7E8 */
	SceInt32 hasExplicitPkgType;		/* +0x7EC */
};

struct GameUpdateInitParam {
	void *(*allocFunc)(SceSize size);
	void (*freeFunc)(void *ptr);
	SceInt32 targetPlatform;
	char titleId[16];
	char currentAppVer[8];
	char contentId[48];
	SceUInt32 currentPsp2SysVer;
	SceInt32 currentPspSysVer;
	char patchTitleId[16];
	SceUInt32 flags;
};

struct GameUpdateContext {
	GameUpdateCore *core;
	GameUpdateHttpWork *http;
	GameUpdateXmlWork *xml;
};

struct GameUpdateInternalResult {
	SceSize size;
	SceInt32 status;
	char latestVersion[8];
};

struct GameUpdateInfo {
	char tagName[20];
	char titleName[128];
	char changeInfoUrl[256];
	SceUInt32 packageCount;
	SceUInt32 flags;
};

typedef char GameUpdatePackageNodeSizeCheck[sizeof(GameUpdatePackageNode) == 0x2E0 ? 1 : -1];
typedef char GameUpdateAltPackageSizeCheck[sizeof(GameUpdateAltPackage) == 0x158 ? 1 : -1];
typedef char GameUpdateCoreSizeCheck[sizeof(GameUpdateCore) == 0xC8 ? 1 : -1];
typedef char GameUpdateHttpWorkSizeCheck[sizeof(GameUpdateHttpWork) == 0x8B0 ? 1 : -1];
typedef char GameUpdateXmlWorkSizeCheck[sizeof(GameUpdateXmlWork) == 0x7F0 ? 1 : -1];
typedef char GameUpdateInitParamSizeCheck[sizeof(GameUpdateInitParam) == 112 ? 1 : -1];
typedef char GameUpdateInternalResultSizeCheck[sizeof(GameUpdateInternalResult) == 16 ? 1 : -1];
typedef char GameUpdateInfoSizeCheck[sizeof(GameUpdateInfo) == 412 ? 1 : -1];

/* XOR-0x7F obfuscated HMAC-SHA256 key for Vita titlepatch URL signing */
static const SceUInt8 s_hmacKeyObfuscated[32] = {
	0x9A, 0x9D, 0x07, 0xD5, 0x61, 0x9C, 0x3F, 0xFD,
	0xDF, 0xF7, 0x58, 0xE3, 0xFC, 0x86, 0xC4, 0xB7,
	0x79, 0xFD, 0x63, 0x2D, 0x8D, 0xD4, 0x22, 0x54,
	0x35, 0xC2, 0xE6, 0x2B, 0x2F, 0x4A, 0x2E, 0x6B
};

static const char *const s_allowedNpEnvs[] = {
	"np",
	"prod-qa",
	"sp-int",
	"rc",
	"e1-np",
	NULL
};

static int s_gameUpdateInitialized = 0;
static char s_changeinfoLangTag[16] = "changeinfo_01";
static char s_titleLangTag[12] = "title_01";
static GameUpdateContext s_gameUpdateContext;

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

/* Forward declarations for internal helpers (in link order) */
static int computeTitlePatchHmacHex(const char *npEnv, const char *titleId,
				    char *outHex, SceSize maxHexLen);
static int gameUpdateHttpReset(GameUpdateContext *context);
static int gameUpdateHttpCreateTemplate(GameUpdateContext *context);
static int gameUpdateHttpDeleteTemplate(GameUpdateContext *context);
static int gameUpdateHttpCreateRequest(GameUpdateContext *context);
static int gameUpdateHttpDeleteRequest(GameUpdateContext *context);
static int gameUpdateHttpPerform(GameUpdateContext *context);
static int gameUpdateHttpAbort(GameUpdateContext *context);
static int onHttpProxyAuth(int request, int authType, const char *realm,
			   char *username, char *password, bool isProxy,
			   char *domain, SceSize *domainLen,
			   bool *save, void *userArg);
static int parseVersionString(const char *verStr, SceUInt16 *outMajor,
			      SceUInt16 *outMinor, SceUInt8 formatMask);
static int gameUpdateXmlInit(GameUpdateContext *context);
static int gameUpdateXmlFeed(GameUpdateContext *context, const char *chunk, int length);
static int gameUpdateXmlFinalize(GameUpdateContext *context);
static int onGameUpdateXmlEvent(NpXmlParser *parser, int event, SceUInt32 depth,
				SceUInt32 index, const char *name, const char *value,
				void *userArg);

__attribute__((noinline))
static void *gameUpdateAlloc(SceSize size)
{
	return SceNetInternal_A2C2BC5F(size, 0);
}

__attribute__((noinline))
static void gameUpdateFree(void *ptr)
{
	if (ptr != NULL)
		SceNetInternal_E62A2FAA(ptr);
}

__attribute__((noinline))
static int gameUpdateContextTerm(GameUpdateContext *context)
{
	if (context == NULL)
		return SCE_GAME_UPDATE_ERROR_INVALID_ARG;
	GameUpdateCore *core = context->core;
	if (core == NULL)
		return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;

	void (*freeFunc)(void *) = core->freeFunc;
	int ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	switch (core->lifecycleState) {
	case GAMEUPDATE_LIFECYCLE_READY:
		break;
	case GAMEUPDATE_LIFECYCLE_RUNNING:
		gameUpdateHttpAbort(context);
		core->lifecycleState = GAMEUPDATE_LIFECYCLE_ABORTING;
		/* fall through */
	case GAMEUPDATE_LIFECYCLE_ABORTING:
		while (1) {
			sceKernelUnlockLwMutex(&core->mutex, 1);
			sceKernelDelayThread(100000);
			ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
			if (ret < 0)
				return ret;
			if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_DONE)
				break;
		}
		/* fall through */
	case GAMEUPDATE_LIFECYCLE_DONE:
		if (core->freeFunc != NULL) {
			GameUpdatePackageNode *node = core->packageListHead;
			while (node != NULL) {
				GameUpdatePackageNode *nextNode = node->next;
				core->freeFunc(node);
				core->packageListHead = nextNode;
				node = nextNode;
			}
		}
		break;
	default:
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return GAMEUPDATE_ERROR_INVALID_STATE;
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

__attribute__((noinline))
static int gameUpdateContextInit(GameUpdateContext *context, int option,
				 const GameUpdateInitParam *param)
{
	if (context == NULL || option != 0 || param == NULL)
		return SCE_GAME_UPDATE_ERROR_INVALID_ARG;

	sceClibMemset(context, 0, sizeof(*context));

	int ret = 0;
	context->core = (GameUpdateCore *)param->allocFunc(sizeof(GameUpdateCore));
	if (context->core == NULL) {
		ret = GAMEUPDATE_ERROR_NO_MEMORY;
	} else {
		sceClibMemset(context->core, 0, sizeof(GameUpdateCore));
		context->http = (GameUpdateHttpWork *)param->allocFunc(sizeof(GameUpdateHttpWork));
		if (context->http == NULL) {
			ret = GAMEUPDATE_ERROR_NO_MEMORY;
		} else {
			sceClibMemset(context->http, 0, sizeof(GameUpdateHttpWork));
			context->xml = (GameUpdateXmlWork *)param->allocFunc(sizeof(GameUpdateXmlWork));
			if (context->xml == NULL)
				ret = GAMEUPDATE_ERROR_NO_MEMORY;
			else
				sceClibMemset(context->xml, 0, sizeof(GameUpdateXmlWork));
		}
	}

	if (ret == 0) {
		GameUpdateCore *core = context->core;
		core->allocFunc = param->allocFunc;
		core->freeFunc = param->freeFunc;

		SceInt32 platform = param->targetPlatform;
		if (platform == GAMEUPDATE_PLATFORM_DEFAULT)
			platform = GAMEUPDATE_PLATFORM_PSP2;

		if (platform != GAMEUPDATE_PLATFORM_PSP &&
		    platform != GAMEUPDATE_PLATFORM_PSP2) {
			ret = SCE_GAME_UPDATE_ERROR_INVALID_ARG;
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

__attribute__((noinline))
static int gameUpdateContextRun(GameUpdateContext *context, GameUpdateInternalResult *result,
				GameUpdateInfo *info)
{
	if (context == NULL || result == NULL || info == NULL)
		return SCE_GAME_UPDATE_ERROR_INVALID_ARG;
	GameUpdateCore *core = context->core;
	if (core == NULL)
		return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;

	int ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	switch (core->lifecycleState) {
	case GAMEUPDATE_LIFECYCLE_READY:
		core->lifecycleState = GAMEUPDATE_LIFECYCLE_RUNNING;
		sceKernelUnlockLwMutex(&core->mutex, 1);
		break;
	case GAMEUPDATE_LIFECYCLE_RUNNING:
	case GAMEUPDATE_LIFECYCLE_ABORTING:
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return SCE_GAME_UPDATE_ERROR_BUSY;
	case GAMEUPDATE_LIFECYCLE_DONE:
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return 0;
	default:
		sceKernelUnlockLwMutex(&core->mutex, 1);
		return GAMEUPDATE_ERROR_INVALID_STATE;
	}

	int runResult = 0;

	if (result->size != sizeof(GameUpdateInternalResult)) {
		runResult = SCE_GAME_UPDATE_ERROR_INVALID_ARG;
	} else {
		int debugUpgradable;
		runResult = sceRegMgrSystemParamGetInt(REG_KEY_NP_DEBUG_UPGRADABLE,
						       &debugUpgradable);
		if (runResult == 0 && debugUpgradable != 0) {
			result->status = GAMEUPDATE_STATUS_AVAILABLE;
			result->latestVersion[0] = '0';
			result->latestVersion[1] = '0';
			result->latestVersion[2] = '.';
			result->latestVersion[3] = '0';
			result->latestVersion[4] = '0';
			result->latestVersion[5] = '\0';
			info->flags = GAMEUPDATE_INFO_FLAG_SIGNOFF |
				      GAMEUPDATE_INFO_FLAG_DEBUG_MOCK;
		} else {
			runResult = gameUpdateHttpPerform(context);
			if (runResult >= 0) {
				sceClibStrlcpy(info->tagName, core->tagName, 17);
				info->flags = core->infoFlags;

				bool resolvedByBgdl = false;
				if ((core->initFlags & GAMEUPDATE_OPT_SKIP_BGDL_CHECK) == 0) {
					DownloadTaskStatus bgdlStatus;
					runResult = downloadFindPatchTask(core->originalTitleId,
									  &bgdlStatus);
					if (runResult >= 0) {
						switch (bgdlStatus.taskState) {
						case 0:
							downloadDeletePatchTask(
								core->originalTitleId);
							break;
						case 1:
						case 2:
						case 3:
							result->status =
								GAMEUPDATE_STATUS_DOWNLOADING;
							resolvedByBgdl = true;
							break;
						case 4:
							result->status =
								GAMEUPDATE_STATUS_READY_TO_INSTALL;
							resolvedByBgdl = true;
							break;
						default:
							break;
						}
					}
				}

				if (!resolvedByBgdl) {
					if (core->titleStatusNotAlive == 1) {
						result->status = GAMEUPDATE_STATUS_DISCONTINUED;
					} else {
						bool sysUpdateNeeded = false;
						switch (core->targetPlatform) {
						case GAMEUPDATE_PLATFORM_PSP2:
							if (core->minPsp2SysVer >
							    core->currentPsp2SysVer)
								sysUpdateNeeded = true;
							break;
						case GAMEUPDATE_PLATFORM_PSP:
							if (core->minPspSysVerMajor >
								    core->currentPspSysVerMajor ||
							    (core->minPspSysVerMajor ==
								     core->currentPspSysVerMajor &&
							     core->minPspSysVerMinor >
								     core->currentPspSysVerMinor))
								sysUpdateNeeded = true;
							break;
						default:
							break;
						}

						if (sysUpdateNeeded) {
							result->status =
								GAMEUPDATE_STATUS_SYSUPDATE_NEEDED;
						} else if (core->latestVerMajor != 0 ||
							   core->latestVerMinor != 0) {
							result->latestVersion[0] =
								(char)((core->latestVerMajor / 10) +
								       '0');
							result->latestVersion[1] =
								(char)((core->latestVerMajor % 10) +
								       '0');
							result->latestVersion[2] = '.';
							result->latestVersion[3] =
								(char)((core->latestVerMinor / 10) +
								       '0');
							result->latestVersion[4] =
								(char)((core->latestVerMinor % 10) +
								       '0');
							result->latestVersion[5] = '\0';

							SceUInt32 count = 1;
							GameUpdatePackageNode *lastNode =
								core->packageListHead;
							info->packageCount = 1;
							while (lastNode->next != NULL) {
								lastNode = lastNode->next;
								++count;
							}
							info->packageCount = count;
							sceClibStrlcpy(info->titleName,
								       lastNode->titleName, 128);
							sceClibStrlcpy(info->changeInfoUrl,
								       lastNode->changeInfoUrl,
								       256);
							result->status = GAMEUPDATE_STATUS_AVAILABLE;
						} else {
							result->status =
								GAMEUPDATE_STATUS_UP_TO_DATE;
						}
					}
				}

				if ((info->flags & GAMEUPDATE_INFO_FLAG_DEBUG_MOCK) == 0 &&
				    result->status != GAMEUPDATE_STATUS_DISCONTINUED) {
					SceRtcTick tick;
					sceRtcGetCurrentNetworkTick(&tick);
					PatchCheckCacheEntry cacheEntry;
					sceClibMemset(&cacheEntry, 0, sizeof(cacheEntry));
					cacheEntry.networkTick = tick.tick;
					cacheEntry.status = result->status;
					cacheEntry.flags =
						info->flags & GAMEUPDATE_INFO_FLAG_SIGNOFF;
					shellSvcPatchCheckSetCache(core->originalTitleId,
								   &cacheEntry, -1);
				}
			}
		}
	}

	ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_ABORTING)
		runResult = SCE_GAME_UPDATE_ERROR_ABORTED;
	core->lifecycleState = GAMEUPDATE_LIFECYCLE_DONE;
	sceKernelUnlockLwMutex(&core->mutex, 1);
	return runResult;
}

extern "C" {

__attribute__((noinline))
SceInt32 sceGameUpdateInit(SceGameUpdateOptParam *optParam)
{
	(void)optParam;
	int ret = 0;
	int appVer;
	SceKernelSystemSwVersion swVer;
	SceAppMgrStatus appStatus;
	char contentId[48];
	GameUpdateInitParam param;

	if (s_gameUpdateInitialized != 1) {
		sceClibMemset(&param, 0, sizeof(param));
		sceClibMemset(&appStatus, 0, sizeof(appStatus));
		ret = sceAppMgrGetStatusByName(NULL, &appStatus);
		if (ret != 0)
			return ret;

		sceClibMemset(contentId, 0, sizeof(contentId));
		ret = sceAppMgrAppParamGetString(0, 6, contentId, sizeof(contentId));
		if (ret != 0)
			return ret;

		appVer = 0;
		ret = sceAppMgrAppParamGetInt(0, 3, &appVer);
		if (ret != 0)
			return ret;

		if ((unsigned int)appVer < 10000)
			sceClibSnprintf(param.currentAppVer, 6, "%02d.%02d",
					appVer / 100, appVer % 100);
		else
			sceClibMemcpy_safe(param.currentAppVer, "00.00", 6);

		swVer.size = sizeof(swVer);
		ret = sceKernelGetSystemSwVersion(&swVer);
		if (ret < 0)
			return ret;

		param.allocFunc = gameUpdateAlloc;
		param.freeFunc = gameUpdateFree;
		sceClibMemcpy_safe(param.titleId, appStatus.titleId, 13);
		sceClibMemcpy_safe(param.contentId, contentId, sizeof(param.contentId));
		param.currentPsp2SysVer = swVer.version;

		ret = gameUpdateContextInit(&s_gameUpdateContext, 0, &param);
		if (ret != 0)
			return ret;
	}
	s_gameUpdateInitialized = 1;
	return ret;
}

__attribute__((noinline))
SceInt32 sceGameUpdateRun(SceGameUpdateResult *result)
{
	GameUpdateInternalResult internalResult;
	GameUpdateInfo info;

	sceClibMemset(&internalResult, 0, sizeof(internalResult));
	sceClibMemset(&info, 0, sizeof(info));
	if (result == NULL)
		return SCE_GAME_UPDATE_ERROR_INVALID_ARG;

	internalResult.size = sizeof(internalResult);
	int ret = gameUpdateContextRun(&s_gameUpdateContext, &internalResult, &info);
	if (ret >= 0) {
		SceInt32 found;
		switch (internalResult.status) {
		case GAMEUPDATE_STATUS_DOWNLOADING:
		case GAMEUPDATE_STATUS_AVAILABLE:
		case GAMEUPDATE_STATUS_SYSUPDATE_NEEDED:
		case GAMEUPDATE_STATUS_READY_TO_INSTALL:
			found = 1;
			break;
		case GAMEUPDATE_STATUS_DISCONTINUED:
			ret = SCE_GAME_UPDATE_ERROR_SERVER_MAINTENANCE;
			found = 0;
			break;
		default:
			found = 0;
			break;
		}
		result->patchExist = found;
		sceClibMemcpy_safe(result->appVer, internalResult.latestVersion,
				   SCE_GAME_UPDATE_APP_VER_SIZE);
	}
	return ret;
}

__attribute__((noinline))
SceInt32 sceGameUpdateTerm(void)
{
	int ret = 0;
	if (s_gameUpdateInitialized == 1) {
		ret = gameUpdateContextTerm(&s_gameUpdateContext);
		s_gameUpdateInitialized = 0;
	}
	return ret;
}

__attribute__((noinline))
SceInt32 sceGameUpdateAbort(void)
{
	GameUpdateCore *core = s_gameUpdateContext.core;
	if (core == NULL)
		return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;

	int ret = sceKernelLockLwMutex(&core->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (core->lifecycleState == GAMEUPDATE_LIFECYCLE_RUNNING) {
		gameUpdateHttpAbort(&s_gameUpdateContext);
		core->lifecycleState = GAMEUPDATE_LIFECYCLE_ABORTING;
	}
	sceKernelUnlockLwMutex(&core->mutex, 1);
	return ret;
}

__attribute__((noinline))
int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

__attribute__((noinline))
int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

} /* extern "C" */

static inline bool parseSha1HexBytes(const char *hexStr, SceUInt8 *outDigest)
{
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

static inline bool parseSha1Hex(const char *hexStr, SceUInt8 *outDigest)
{
	if (sceClibStrnlen(hexStr, 41) != 40)
		return false;
	return parseSha1HexBytes(hexStr, outDigest);
}

static inline bool parseDecimalU32(const char *str, SceUInt32 *outValue)
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

__attribute__((noinline))
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
	SceSize msgLen = sceClibStrlcat(message, titleId, 30);

	int ret = sceHmacSha256Digest(key, sizeof(key), message,
				     (SceUInt32)msgLen, digest);
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

__attribute__((noinline))
static int gameUpdateHttpReset(GameUpdateContext *context)
{
	GameUpdateHttpWork *http = context->http;
	sceClibMemset(http, 0, sizeof(*http));
	http->templateId = -1;
	http->connectionId = -1;
	http->requestId = -1;
	return 0;
}

__attribute__((noinline))
static int gameUpdateHttpCreateTemplate(GameUpdateContext *context)
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
	sceHttpSetRecvTimeOut(http->templateId, 60000000u);
	sceHttpSetSendTimeOut(http->templateId, 60000000u);
	sceHttpRemoveRequestHeader(http->templateId, "Accept");
	sceHttpRemoveRequestHeader(http->templateId, "Accept-Encoding");
	int ret = sceHttpRemoveRequestHeader(http->templateId, "Accept-Charset");
	if (ret < 0)
		ret = 0;

	sceHttpsEnableOptionPrivate(http->templateId, 357);
	sceHttpsDisableOptionPrivate(http->templateId, 24);

	int regInt;
	if (sceRegMgrSystemParamGetInt(REG_KEY_HTTP_PROXY_FLAG, &regInt) >= 0 &&
	    regInt != 0) {
		char proxyHost[256];
		sceClibMemset(proxyHost, 0, sizeof(proxyHost));
		if (sceRegMgrSystemParamGetStr(REG_KEY_HTTP_PROXY_SERVER,
					       proxyHost, sizeof(proxyHost)) >= 0 &&
		    proxyHost[0] != '\0' &&
		    sceRegMgrSystemParamGetInt(REG_KEY_HTTP_PROXY_PORT, &regInt) >= 0 &&
		    regInt > 0 && regInt <= 0xFFFF) {
			SceUInt16 proxyPort = (SceUInt16)regInt;
			if (sceRegMgrSystemParamGetStr(REG_KEY_HTTP_PROXY_USER,
						       http->proxyUser,
						       sizeof(http->proxyUser)) >= 0 &&
			    sceRegMgrSystemParamGetStr(REG_KEY_HTTP_PROXY_PASS,
						       http->proxyPass,
						       sizeof(http->proxyPass)) >= 0) {
				SceHttp_53B722F2(http->templateId, 1, 1, proxyHost, proxyPort);
				sceHttpSetAuthInfoCallback(http->templateId, onHttpProxyAuth, http);
			}
		}
	}
	return ret;
}

__attribute__((noinline))
static int gameUpdateHttpDeleteTemplate(GameUpdateContext *context)
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

__attribute__((noinline))
static int gameUpdateHttpCreateRequest(GameUpdateContext *context)
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

	switch (core->targetPlatform) {
	case GAMEUPDATE_PLATFORM_PSP2:
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
		break;
	case GAMEUPDATE_PLATFORM_PSP:
		sceClibStrlcat(url, "https://a0.ww.", sizeof(url));
		sceClibStrlcat(url, http->npEnv, sizeof(url));
		sceClibStrlcat(url, ".dl.playstation.net/tpl/", sizeof(url));
		sceClibStrlcat(url, http->npEnv, sizeof(url));
		sceClibStrlcat(url, "/", sizeof(url));
		sceClibStrlcat(url, core->queryTitleId, sizeof(url));
		sceClibStrlcat(url, "/", sizeof(url));
		sceClibStrlcat(url, core->queryTitleId, sizeof(url));
		sceClibStrlcat(url, "-ver.xml", sizeof(url));
		break;
	default:
		break;
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

__attribute__((noinline))
static int gameUpdateHttpDeleteRequest(GameUpdateContext *context)
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

__attribute__((noinline))
static int gameUpdateHttpPerform(GameUpdateContext *context)
{
	GameUpdateCore *core = context->core;
	GameUpdateHttpWork *http = context->http;

	int envIdx = 0;
	while (s_allowedNpEnvs[envIdx] != NULL) {
		if (sceClibStrncasecmp(s_allowedNpEnvs[envIdx], http->npEnv, 17) == 0)
			break;
		++envIdx;
	}
	if (s_allowedNpEnvs[envIdx] == NULL)
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
		core->infoFlags |= GAMEUPDATE_INFO_FLAG_SIGNOFF;
		return ret;
	}
	if (core->targetPlatform == GAMEUPDATE_PLATFORM_PSP2 &&
	    (status == 500 || status == 501 || status == 503)) {
		http->contentLength = 0;
		core->infoFlags |= GAMEUPDATE_INFO_FLAG_SIGNOFF;
		return ret;
	}
	if (status != 200)
		return SCE_GAME_UPDATE_ERROR_HTTP_BAD_RESPONSE;

	ret = sceHttpGetResponseContentLength(http->requestId, &http->contentLength);
	if (ret < 0)
		return ret;

	if (http->contentLength > 0xFFFFu)
		return SCE_GAME_UPDATE_ERROR_HTTP_BAD_RESPONSE;

	if (http->contentLength == 0)
		return ret;

	SceUInt64 totalRead = 0;
	while (totalRead < http->contentLength) {
		int nread = sceHttpReadData(http->requestId, http->readBuffer, 2048);
		if (nread < 0)
			return nread;
		ret = gameUpdateXmlFeed(context, http->readBuffer, nread);
		if (ret < 0)
			return ret;
		totalRead += (SceUInt64)(unsigned int)nread;
	}
	return gameUpdateXmlFinalize(context);
}

__attribute__((noinline))
static int gameUpdateHttpAbort(GameUpdateContext *context)
{
	GameUpdateHttpWork *http = context->http;
	if (http->requestId >= 0)
		return sceHttpAbortRequest(http->requestId);
	return 0;
}

__attribute__((noinline))
static int onHttpProxyAuth(int request, int authType, const char *realm,
			   char *username, char *password, bool isProxy,
			   char *domain, SceSize *domainLen,
			   bool *save, void *userArg)
{
	(void)request;
	(void)authType;
	(void)realm;
	(void)isProxy;
	(void)domain;
	(void)domainLen;
	(void)save;
	const GameUpdateHttpWork *http = (const GameUpdateHttpWork *)userArg;
	sceClibMemcpy_safe(username, http->proxyUser, 64);
	sceClibMemcpy_safe(password, http->proxyPass, 64);
	return 0;
}

__attribute__((noinline))
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
		return SCE_GAME_UPDATE_ERROR_INVALID_ARG;

	const char *strEnd = verStr + sceClibStrnlen(verStr, 8);
	const char *minorPtr = NULL;
	SceUInt16 majorVal = 0;

	if (((formatMask & VERSION_FMT_MAJOR2_MINOR2) ||
	     (formatMask & VERSION_FMT_MAJOR2_MINOR4)) &&
	    (sceClibLookCtypeTable(verStr[0]) & 4) != 0 &&
	    (sceClibLookCtypeTable(verStr[1]) & 4) != 0 &&
	    verStr[2] == '.') {
		majorVal = (SceUInt16)((verStr[0] - '0') * 10 + (verStr[1] - '0'));
		minorPtr = verStr + 3;
	} else if ((formatMask & VERSION_FMT_MAJOR1_MINOR2) &&
		   (sceClibLookCtypeTable(verStr[0]) & 4) != 0 &&
		   verStr[1] == '.') {
		majorVal = (SceUInt16)(verStr[0] - '0');
		minorPtr = verStr + 2;
	} else {
		return SCE_GAME_UPDATE_ERROR_INVALID_ARG;
	}

	if ((sceClibLookCtypeTable(minorPtr[0]) & 4) == 0 ||
	    (sceClibLookCtypeTable(minorPtr[1]) & 4) == 0)
		return SCE_GAME_UPDATE_ERROR_INVALID_ARG;

	SceUInt16 minorVal = (SceUInt16)((minorPtr[0] - '0') * 10 + (minorPtr[1] - '0'));
	if (formatMask & VERSION_FMT_MAJOR2_MINOR4) {
		minorVal = (SceUInt16)(minorVal * 100);
		if (minorPtr + 2 < strEnd) {
			if ((sceClibLookCtypeTable(minorPtr[2]) & 4) == 0 ||
			    (sceClibLookCtypeTable(minorPtr[3]) & 4) == 0)
				return SCE_GAME_UPDATE_ERROR_INVALID_ARG;
			minorVal = (SceUInt16)(minorVal + (minorPtr[2] - '0') * 10 +
					       (minorPtr[3] - '0'));
		}
	}

	*outMajor = majorVal;
	*outMinor = minorVal;
	return 0;
}

__attribute__((noinline))
static int gameUpdateXmlInit(GameUpdateContext *context)
{
	int lang = 1;
	GameUpdateXmlWork *xml = context->xml;
	sceRegMgrSystemParamGetInt(REG_KEY_SYSTEM_LANGUAGE, &lang);
	s_titleLangTag[6] = (char)((lang / 10) + '0');
	s_titleLangTag[7] = (char)((lang % 10) + '0');
	s_changeinfoLangTag[11] = (char)((lang / 10) + '0');
	s_changeinfoLangTag[12] = (char)((lang % 10) + '0');
	return SceNpCommon_650D111B(&xml->parser, &s_xmlSchema,
				    onGameUpdateXmlEvent, context);
}

__attribute__((noinline))
static int gameUpdateXmlFeed(GameUpdateContext *context, const char *chunk, int length)
{
	return SceNpCommon_88B18A5B(&context->xml->parser, chunk, length);
}

__attribute__((noinline))
static int gameUpdateXmlFinalize(GameUpdateContext *context)
{
	GameUpdateCore *core = context->core;
	GameUpdateXmlWork *xml = context->xml;

	if (xml->hasExplicitPkgType == 0) {
		GameUpdatePackageNode *node = core->packageListHead;
		if (node != NULL) {
			node->packageType = GAMEUPDATE_PKG_CUMULATIVE;
			node = node->next;
			while (node != NULL) {
				node->packageType = GAMEUPDATE_PKG_INCREMENTAL;
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
				GameUpdatePackageNode *replacement = NULL;
				if (core->allocFunc != NULL && core->freeFunc != NULL)
					replacement = (GameUpdatePackageNode *)core->allocFunc(
						sizeof(GameUpdatePackageNode));
				if (replacement == NULL)
					return GAMEUPDATE_ERROR_NO_MEMORY;

				sceClibMemcpy_safe(replacement, latestNode, sizeof(GameUpdatePackageNode));
				replacement->packageSize = xml->hybridPkg.packageSize;
				sceClibMemcpy_safe(replacement->sha1Digest, xml->hybridPkg.sha1Digest, 20);
				sceClibMemcpy_safe(replacement->packageUrl, xml->hybridPkg.packageUrl, 256);
				sceClibMemcpy_safe(replacement->contentId, xml->hybridPkg.contentId, 48);
				replacement->next = NULL;
				replacement->packageType = GAMEUPDATE_PKG_CUMULATIVE;

				GameUpdatePackageNode *cur = core->packageListHead;
				while (cur != NULL) {
					GameUpdatePackageNode *nextNode = cur->next;
					core->freeFunc(cur);
					core->packageListHead = nextNode;
					cur = nextNode;
				}
				core->packageListHead = replacement;
				core->packageCount = 1;
			}
		}
	}

	return 0;
}

__attribute__((noinline))
static int onGameUpdateXmlEvent(NpXmlParser *parser, int event, SceUInt32 depth,
				SceUInt32 index, const char *name, const char *value,
				void *userArg)
{
	(void)parser;
	GameUpdateContext *context = (GameUpdateContext *)userArg;
	GameUpdateCore *core = context->core;
	GameUpdateXmlWork *xml = context->xml;
	SceUInt16 parsedMajor = 0;
	SceUInt16 parsedMinor = 0;

	if (event == NP_XML_EVENT_START) {
		if (depth == 0 && index == 0) {
			if (xml->parseState != XML_STATE_ROOT)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_TITLEPATCH;
		} else if (depth == 1 && index == 0) {
			if (xml->parseState != XML_STATE_TITLEPATCH)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_TAG;
		} else if (depth == 2 && index == 0) {
			if (xml->parseState != XML_STATE_TAG)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			sceClibMemset(&xml->currentPkg, 0, sizeof(xml->currentPkg));
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 0) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
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
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_PARAMSFO;
		} else if (depth == 3 && index == 2) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_CHANGEINFO;
		} else if (depth == 3 && index == 3) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_CHANGEINFO_LANG;
		} else if (depth == 3 && index == 4) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_URL;
		} else if (depth == 4 && index == 0) {
			if (xml->parseState != XML_STATE_PARAMSFO)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_TITLE;
		} else if (depth == 4 && index == 1) {
			if (xml->parseState != XML_STATE_PARAMSFO)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_TITLE_LANG;
		}
		return 0;
	}

	if (event == NP_XML_EVENT_END) {
		if (depth == 0 && index == 0) {
			if (xml->parseState != XML_STATE_TITLEPATCH)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if ((xml->attrMask & (XML_ATTR_TITLEID | XML_ATTR_STATUS)) !=
			    (XML_ATTR_TITLEID | XML_ATTR_STATUS))
				return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
			xml->parseState = XML_STATE_ROOT;
		} else if (depth == 1 && index == 0) {
			if (xml->parseState != XML_STATE_TAG)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if ((xml->attrMask & XML_ATTR_TAG_NAME) == 0)
				return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
			xml->parseState = XML_STATE_TITLEPATCH;
		} else if (depth == 2 && index == 0) {
			if (xml->parseState != XML_STATE_PACKAGE)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;

			if (core->targetPlatform == GAMEUPDATE_PLATFORM_PSP2) {
				const SceUInt32 reqMask = XML_ATTR_PKG_VERSION | XML_ATTR_PKG_SIZE |
							  XML_ATTR_PKG_SHA1 | XML_ATTR_PKG_URL |
							  XML_ATTR_PKG_CONTENT_ID;
				if ((xml->attrMask & reqMask) != reqMask)
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				if (xml->hasExplicitPkgType == 1 &&
				    (xml->attrMask & XML_ATTR_PKG_TYPE) == 0)
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
			} else if (core->targetPlatform == GAMEUPDATE_PLATFORM_PSP) {
				const SceUInt32 reqMask = XML_ATTR_PKG_VERSION | XML_ATTR_PKG_SIZE |
							  XML_ATTR_PKG_SHA1 | XML_ATTR_PKG_URL;
				if ((xml->attrMask & reqMask) != reqMask)
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
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

				if (core->targetPlatform == GAMEUPDATE_PLATFORM_PSP2) {
					if (xml->currentPkg.requiredPsp2SysVer > core->minPsp2SysVer)
						core->minPsp2SysVer =
							xml->currentPkg.requiredPsp2SysVer;
				} else if (core->targetPlatform == GAMEUPDATE_PLATFORM_PSP) {
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
						return GAMEUPDATE_ERROR_NO_MEMORY;

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
						return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
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
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			const SceUInt32 reqMask = XML_ATTR_HYBRID_SIZE | XML_ATTR_HYBRID_SHA1 |
						  XML_ATTR_HYBRID_URL | XML_ATTR_HYBRID_CONTENT_ID;
			if ((xml->attrMask & reqMask) != reqMask)
				return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
			xml->hybridIsNewer = 0;
			xml->attrMask &= ~reqMask;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 1) {
			if (xml->parseState != XML_STATE_PARAMSFO)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 2) {
			if (xml->parseState != XML_STATE_CHANGEINFO)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 3) {
			if (xml->parseState != XML_STATE_CHANGEINFO_LANG)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_PACKAGE;
		} else if (depth == 3 && index == 4) {
			if (xml->parseState != XML_STATE_URL)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			const SceUInt32 reqMask = XML_ATTR_URL_SIZE | XML_ATTR_URL_SHA1 |
						  XML_ATTR_URL_HREF | XML_ATTR_URL_CONTENT_ID;
			if ((xml->attrMask & reqMask) != reqMask)
				return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
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
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_PARAMSFO;
		} else if (depth == 4 && index == 1) {
			if (xml->parseState != XML_STATE_TITLE_LANG)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			xml->parseState = XML_STATE_PARAMSFO;
		}
		return 0;
	}

	if (event == NP_XML_EVENT_TEXT) {
		if (depth == 4 && index == 0) {
			if (xml->parseState != XML_STATE_TITLE)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if ((xml->attrMask & XML_ATTR_TITLE_LANG) == 0)
				sceClibStrlcpy(xml->currentPkg.titleName, name, 128);
			xml->attrMask |= XML_ATTR_TITLE_DEFAULT;
			xml->parseState = XML_STATE_PARAMSFO;
		} else if (depth == 4 && index == 1) {
			if (xml->parseState != XML_STATE_TITLE_LANG)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
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
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
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
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if (sceClibStrncasecmp(name, "name", 5) == 0) {
				sceClibStrlcpy(core->tagName, value, 17);
				xml->attrMask |= XML_ATTR_TAG_NAME;
			} else if (sceClibStrncasecmp(name, "signoff", 8) == 0) {
				if (sceClibStrncasecmp(value, "true", 5) == 0)
					core->infoFlags |= GAMEUPDATE_INFO_FLAG_SIGNOFF;
				xml->attrMask |= XML_ATTR_TAG_SIGNOFF;
			} else if (sceClibStrncasecmp(name, "min_system_ver", 15) == 0) {
				if (core->targetPlatform == GAMEUPDATE_PLATFORM_PSP2) {
					SceUInt32 sysVer = 0;
					if (!parseDecimalU32(value, &sysVer))
						return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
					if (sysVer > core->minPsp2SysVer)
						core->minPsp2SysVer = sysVer;
				} else if (core->targetPlatform == GAMEUPDATE_PLATFORM_PSP) {
					if (parseVersionString(value, &parsedMajor, &parsedMinor,
							       VERSION_FMT_MAJOR2_MINOR2 |
							       VERSION_FMT_MAJOR1_MINOR2) < 0)
						return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
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
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if (sceClibStrncasecmp(name, "version", 8) == 0) {
				if (parseVersionString(value, &xml->currentPkg.versionMajor,
						       &xml->currentPkg.versionMinor,
						       VERSION_FMT_MAJOR2_MINOR2) < 0)
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				xml->attrMask |= XML_ATTR_PKG_VERSION;
			} else if (sceClibStrncasecmp(name, "size", 5) == 0) {
				char *endPtr = NULL;
				xml->currentPkg.packageSize =
					(SceUInt64)sceClibStrtoll(value, &endPtr, 10);
				if (endPtr == NULL || *endPtr != '\0')
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				xml->attrMask |= XML_ATTR_PKG_SIZE;
			} else if (sceClibStrncasecmp(name, "sha1sum", 8) == 0) {
				if (!parseSha1Hex(value, xml->currentPkg.sha1Digest))
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				xml->attrMask |= XML_ATTR_PKG_SHA1;
			} else if (sceClibStrncasecmp(name, "url", 4) == 0) {
				sceClibStrlcpy(xml->currentPkg.packageUrl, value, 256);
				xml->attrMask |= XML_ATTR_PKG_URL;
			} else if (sceClibStrncasecmp(name, "psp2_system_ver", 16) == 0) {
				SceUInt32 sysVer = 0;
				if (!parseDecimalU32(value, &sysVer))
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				xml->currentPkg.requiredPsp2SysVer = sysVer;
				xml->attrMask |= XML_ATTR_PKG_PSP2_SYSVER;
			} else if (sceClibStrncasecmp(name, "psp_system_ver", 15) == 0) {
				if (parseVersionString(value,
						       &xml->currentPkg.requiredPspSysVerMajor,
						       &xml->currentPkg.requiredPspSysVerMinor,
						       VERSION_FMT_MAJOR2_MINOR2 |
						       VERSION_FMT_MAJOR1_MINOR2) < 0)
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				xml->attrMask |= XML_ATTR_PKG_PSP_SYSVER;
			} else if (sceClibStrncasecmp(name, "content_id", 11) == 0) {
				sceClibStrlcpy(xml->currentPkg.contentId, value, 48);
				xml->attrMask |= XML_ATTR_PKG_CONTENT_ID;
			} else if (sceClibStrncasecmp(name, "type", 5) == 0) {
				if (sceClibStrncasecmp(value, "cumulative", 11) == 0)
					xml->currentPkg.packageType = GAMEUPDATE_PKG_CUMULATIVE;
				else if (sceClibStrncasecmp(value, "incremental", 12) == 0)
					xml->currentPkg.packageType = GAMEUPDATE_PKG_INCREMENTAL;
				else
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				xml->hasExplicitPkgType = 1;
				xml->attrMask |= XML_ATTR_PKG_TYPE;
			}
		} else if (depth == 3 && index == 0) {
			if (xml->parseState != XML_STATE_HYBRID_PACKAGE)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if (sceClibStrncasecmp(name, "size", 5) == 0) {
				if (xml->hybridIsNewer == 1) {
					char *endPtr = NULL;
					xml->hybridPkg.packageSize =
						(SceUInt64)sceClibStrtoll(value, &endPtr, 10);
					if (endPtr == NULL || *endPtr != '\0')
						return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				}
				xml->attrMask |= XML_ATTR_HYBRID_SIZE;
			} else if (sceClibStrncasecmp(name, "sha1sum", 8) == 0) {
				if (sceClibStrnlen(value, 41) != 40)
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				if (xml->hybridIsNewer == 1 &&
				    !parseSha1HexBytes(value, xml->hybridPkg.sha1Digest))
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
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
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if (sceClibStrncasecmp(name, "url", 4) == 0) {
				if ((xml->attrMask & XML_ATTR_CHANGEINFO_LANG_URL) == 0)
					sceClibStrlcpy(xml->currentPkg.changeInfoUrl, value, 256);
				xml->attrMask |= XML_ATTR_CHANGEINFO_URL;
			}
		} else if (depth == 3 && index == 3) {
			if (xml->parseState != XML_STATE_CHANGEINFO_LANG)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if (sceClibStrncasecmp(name, "url", 4) == 0) {
				if ((xml->attrMask & XML_ATTR_CHANGEINFO_URL) != 0)
					sceClibMemset(xml->currentPkg.changeInfoUrl, 0, 256);
				sceClibStrlcpy(xml->currentPkg.changeInfoUrl, value, 256);
				xml->attrMask |= XML_ATTR_CHANGEINFO_LANG_URL;
			}
		} else if (depth == 3 && index == 4) {
			if (xml->parseState != XML_STATE_URL)
				return SCE_GAME_UPDATE_ERROR_PARSER_FAILED;
			if (sceClibStrncasecmp(name, "size", 5) == 0) {
				char *endPtr = NULL;
				xml->altUrlPkg.packageSize =
					(SceUInt64)sceClibStrtoll(value, &endPtr, 10);
				if (endPtr == NULL || *endPtr != '\0')
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
				xml->attrMask |= XML_ATTR_URL_SIZE;
			} else if (sceClibStrncasecmp(name, "sha1sum", 8) == 0) {
				if (!parseSha1Hex(value, xml->altUrlPkg.sha1Digest))
					return SCE_GAME_UPDATE_ERROR_BAD_RESPONSE;
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
