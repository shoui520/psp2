/*
 * SceNpKdc (vs0:sys/external/np_kdc.suprx): the license key delivery client. It asks Sony's
 * commerce server (an HTTPS form post) for a content's license again, or for the activation of
 * the console, and hands the answer to the secure kernel's activation call.
 *
 * Built by Sony with SNC. No debug record names its files; the names are ours, taken from what
 * the code does.
 */

#include <stddef.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include "np_kdc_internal.h"

static void KdcInitHashes(void);
static int KdcStrtol(const char *pString, char **ppEnd, int base);
static unsigned int KdcHash7(const unsigned char *p);
static unsigned int KdcHash8(const unsigned char *p);
static unsigned int KdcHash11(const unsigned char *p);
static unsigned int KdcHash12(const unsigned char *p);
static unsigned int KdcHash14(const unsigned char *p);
static unsigned int KdcHash15(const unsigned char *p);

SceUInt8 g_busy;
KdcRequest *g_pRequest;
SceUInt8 g_initialized;
SceInt32 g_actType = -1;
SceUID g_mtpModuleId;
void *g_pMtpClient;
SceInt32 g_httpReady;
SceInt32 g_httpTemplate;
void (*g_pInitHashes)(void) = KdcInitHashes;
unsigned int g_hashLiveAreaPath;
unsigned int g_hashCTime;
unsigned int g_hashLLTime;
unsigned int g_hashPromoteAttr;
unsigned int g_hashUpStatus;
unsigned int g_hashLibLocation;
unsigned int g_hashAppConf;
void *g_mspace;

const KdcParamDesc g_paramDescs[KDC_NUM_PARAMS] = {
	{ 8, "loginid=" },
	{ 10, "&password=" },
	{ 11, "&epassword=" },
	{ 11, "&consoleid=" },
	{ 11, "&productid=" },
	{ 10, "&platform=" },
	{ 4, "&c1=" },
	{ 9, "&acttype=" },
};

static const KdcHeaderField s_requestHeaders[3] = {
	{ "Content-Type", "application/x-www-form-urlencoded" },
	{ "User-Agent", "My heart leaps up when I behold A rainbow in the sky" },
	{ "X-I-5-DRM-Version", "1.0" },
};

static const KdcHeaderField s_versionHeader[1] = {
	{ "X-I-5-DRM-Version", "1.0" },
};

static const KdcHeaderField s_contentHeader[1] = {
	{ "Content-Type", "application/x-i-5-drm" },
};

int SceNpKdc_26F8A1B9(SceSize size, void *pMemory)
{
	if (g_initialized == 0) {
		g_mspace = sceClibMspaceCreate(pMemory, size);
		g_initialized = 1;
	}
	return 0;
}

int SceNpKdc_06CB1007(void)
{
	if (g_initialized == 0)
		return 0;
	if (g_mtpModuleId != 0) {
		ScePafStdc_1B77082E(ScePafMisc_7C86911E(g_mtpModuleId));
		g_mtpModuleId = 0;
	}
	sceClibMspaceDestroy(g_mspace);
	g_initialized = 0;
	return 0;
}

__attribute__((noinline))
static void *KdcAlloc(SceSize size)
{
	return sceClibMspaceMalloc(g_mspace, size);
}

__attribute__((noinline))
static void KdcFree(void *pMemory)
{
	sceClibMspaceFree(g_mspace, pMemory);
}

/* the commerce server's address for the environment the account is in */
__attribute__((noinline))
static int KdcGetUrl(int type, char **ppUrl)
{
	char environment[16];
	const char *pFormat;
	SceSize length;
	int ret;

	ret = sceNpCommonGetNpEnviroment(environment);
	if (ret < 0)
		return ret;
	if (type > 0) {
		if (type <= 1) {
			pFormat = "https://commerce.%s.ac.playstation.net/cdp.m";
		} else {
			if (type > 2)
				return NP_KDC_ERROR_INVALID_ARG;
			pFormat = "https://commerce.%s.ac.playstation.net/kdp.m";
		}
	} else {
		if (type < 0)
			return NP_KDC_ERROR_INVALID_ARG;
		pFormat = "https://commerce.%s.ac.playstation.net/cap.m";
	}
	length = sceClibStrnlen(pFormat, 0x7FFFFFFF);
	*ppUrl = sceClibMspaceMalloc(g_mspace, length + 17);
	if (*ppUrl == NULL)
		return NP_KDC_ERROR_NO_MEMORY;
	sceClibMemset(*ppUrl, 0, length + 17);
	sceClibSnprintf(*ppUrl, length + 16, pFormat, environment);
	return 0;
}

static int KdcCreateTemplate(void);
static int KdcDeleteTemplate(void);
static int KdcCreateRequest(KdcRequest **ppRequest, const char *pUrl);
static int KdcDeleteRequest(KdcRequest *pRequest);
static int KdcAddCredentials(KdcRequest *pRequest, const char *pLoginId, const char *pEncryptedPassword);
static int KdcAddProductId(KdcRequest *pRequest, const char *pProductId);
static int KdcAddConsoleKey(KdcRequest *pRequest, const SceUInt8 *pKey);
static int KdcAddPlatform(KdcRequest *pRequest, const char *pPlatform);
static int KdcAddActType(KdcRequest *pRequest, const char *pActType);
static int KdcSend(KdcRequest *pRequest, const char *pUrl);
static int KdcGetContentLength(KdcRequest *pRequest, SceUInt64 *pLength);
static int KdcGetConsoleInfo(KdcRequest *pRequest, SceUInt32 *pMax, SceUInt32 *pCurrent);
static int KdcReadData(KdcRequest *pRequest, char *pData, SceSize size, SceSize *pRead);
static int KdcAbort(KdcRequest *pRequest);
static int KdcCheckActData(int *pActType, int *pVersionFlag, SceUInt64 *pAccountId, SceUInt32 expTimeLow, SceUInt32 expTimeHigh);

static int KdcRemoveActData(int a0);

/* a request for the account: its URL, connection, login, platform and the console's key */
__attribute__((noinline))
static int KdcOpenRequest(int type, char **ppUrl, KdcRequest **ppRequest, void *pActData,
    const char *pEncryptedPassword)
{
	char loginId[68];
	SceUInt8 password[36];
	SceUInt8 reserved[8];
	SceUInt8 consoleKey[128];
	int ret;
	int actType;

	ret = KdcGetUrl(type, ppUrl);
	if (ret < 0)
		return ret;
	ret = SceNpManager_DDF30B0E(loginId, password, reserved);
	if (ret < 0)
		return ret;
	if (type > 0) {
		if (type <= 1) {
			actType = 1;
		} else if (type <= 2) {
			actType = 2;
		} else {
			return NP_KDC_ERROR_INVALID_ARG;
		}
	} else if (type < 0) {
		return NP_KDC_ERROR_INVALID_ARG;
	} else {
		actType = 0;
	}
	ret = SceLibKernel_9557D15C(actType, pActData, consoleKey);
	if (ret < 0)
		return ret;
	ret = KdcCreateTemplate();
	if (ret < 0)
		return ret;
	ret = KdcCreateRequest(ppRequest, *ppUrl);
	if (ret < 0
	    || (ret = KdcAddCredentials(*ppRequest, loginId, pEncryptedPassword)) < 0
	    || (ret = KdcAddPlatform(*ppRequest, "psp2")) < 0
	    || (ret = KdcAddConsoleKey(*ppRequest, consoleKey)) < 0) {
		if (*ppUrl != NULL) {
			sceClibMspaceFree(g_mspace, *ppUrl);
			*ppUrl = NULL;
		}
		if (*ppRequest != NULL)
			KdcDeleteRequest(*ppRequest);
		KdcDeleteTemplate();
		return ret;
	}
	return 0;
}

/* the activation: the form is posted, a second time with the activation type when the first is refused */
__attribute__((noinline))
static int KdcActivate(SceUInt32 *pMax, SceUInt32 *pCurrent, const char *pEncryptedPassword)
{
	char *pUrl = NULL;
	SceSize readSize = 0;
	SceUInt64 contentLength = 0;
	char *pData = NULL;
	SceUInt8 actData[32];
	int retried = 0;
	int ret;

	for (;;) {
		ret = KdcOpenRequest(0, &pUrl, &g_pRequest, actData, pEncryptedPassword);
		if (ret < 0 || (retried != 0 && (ret = KdcAddActType(g_pRequest, "4")) < 0))
			break;
		ret = KdcSend(g_pRequest, pUrl);
		if (ret < 0) {
			if (ret == NP_KDC_ERROR_SERVER_REFUSED) {
				if (retried != 0)
					ret = NP_KDC_ERROR_ACTIVATION_FAILED;
				KdcGetConsoleInfo(g_pRequest, pMax, pCurrent);
			}
			if (ret == NP_KDC_ERROR_ALREADY_ACTIVATED)
				ret = 0;
			break;
		}
		ret = KdcGetContentLength(g_pRequest, &contentLength);
		if (ret < 0)
			break;
		pData = sceClibMspaceMalloc(g_mspace, (SceSize)contentLength);
		if (pData == NULL) {
			ret = NP_KDC_ERROR_NO_MEMORY;
			break;
		}
		ret = KdcReadData(g_pRequest, pData, (SceSize)contentLength, &readSize);
		if (ret < 0)
			break;
		ret = sceSblGcAuthMgrPcactActivation(pData, (SceSize)contentLength, (SceSize)contentLength,
		    (int)(contentLength >> 32), (int)pEncryptedPassword);
		if (ret < 0)
			break;
		ret = KdcGetConsoleInfo(g_pRequest, pMax, pCurrent);
		if (ret < 0)
			break;
		ret = 0;
		if (retried != 0)
			break;
		retried = 1;
		if (g_pRequest != NULL) {
			KdcDeleteRequest(g_pRequest);
			KdcDeleteTemplate();
			g_pRequest = NULL;
		}
	}
	if (g_pRequest != NULL) {
		KdcDeleteRequest(g_pRequest);
		KdcDeleteTemplate();
		g_pRequest = NULL;
	}
	if (pUrl != NULL)
		sceClibMspaceFree(g_mspace, pUrl);
	if (pData != NULL)
		sceClibMspaceFree(g_mspace, pData);
	return ret;
}

int SceNpKdc_D8903721(SceUInt32 *pMax, SceUInt32 *pCurrent, const char *pEncryptedPassword)
{
	int actType;
	int versionFlag;
	SceUInt8 accountId[8];
	SceUInt8 expTime[8];
	SceUInt8 expTime2[8];
	int actType2;
	int versionFlag2;
	SceUInt8 accountId2[8];
	SceUInt8 expTime3[8];
	SceUInt8 expTime4[8];
	int ret;
	int actTypeResult;

	if (g_initialized == 0)
		return NP_KDC_ERROR_NOT_INITIALIZED;
	if (g_busy != 0)
		return NP_KDC_ERROR_BUSY_OTHER;
	ret = KdcCheckActData(&actType, &versionFlag, (SceUInt64 *)accountId, (SceUInt32)expTime, (SceUInt32)expTime2);
	actTypeResult = -1;
	if (ret >= 0)
		actTypeResult = actType;
	g_actType = actTypeResult;
	g_busy = 1;
	ret = KdcActivate(pMax, pCurrent, pEncryptedPassword);
	KdcCheckActData(&actType2, &versionFlag2, (SceUInt64 *)accountId2, (SceUInt32)expTime3, (SceUInt32)expTime4);
	g_busy = 0;
	return ret;
}

/* the license again: a request to the license server, the activation tried once more with its type */
int SceNpKdc_23521D81(SceUInt32 *pMax, SceUInt32 *pCurrent, const char *pEncryptedPassword, int force)
{
	char *pUrl = NULL;
	SceUInt8 actData[32];
	SceUInt8 accountId[8];
	SceUInt8 expTime[8];
	SceUInt8 expTime2[8];
	SceUInt8 accountId2[8];
	SceUInt8 expTime3[8];
	SceUInt8 expTime4[8];
	SceUInt8 reserved[8];
	int actType;
	int versionFlag;
	int actType2;
	int versionFlag2;
	int retried = 0;
	int ret;
	int err;

	if (g_initialized == 0)
		return NP_KDC_ERROR_NOT_INITIALIZED;
	if (g_busy != 0)
		return NP_KDC_ERROR_BUSY_OTHER;
	err = KdcCheckActData(&actType, &versionFlag, (SceUInt64 *)expTime, (SceUInt32)expTime2, (SceUInt32)reserved);
	g_actType = (err >= 0) ? actType : -1;
	g_busy = 1;
	ret = KdcRemoveActData((int)accountId);
	if (ret >= 0 || ret == (int)0x80870006 || force == 1) {
		for (;;) {
			ret = KdcOpenRequest(1, &pUrl, &g_pRequest, actData, pEncryptedPassword);
			if (ret < 0)
				break;
			if (retried != 0) {
				ret = KdcAddActType(g_pRequest, "4");
				if (ret < 0)
					break;
			}
			ret = KdcSend(g_pRequest, pUrl);
			if (ret < 0) {
				if (ret == NP_KDC_ERROR_ALREADY_ACTIVATED)
					ret = 0;
				break;
			}
			ret = KdcGetConsoleInfo(g_pRequest, pMax, pCurrent);
			if (ret < 0)
				break;
			if (retried != 0) {
				ret = 0;
				break;
			}
			retried = 1;
			if (g_pRequest != NULL) {
				KdcDeleteRequest(g_pRequest);
				KdcDeleteTemplate();
				g_pRequest = NULL;
			}
		}
	}
	if (g_pRequest != NULL) {
		KdcDeleteRequest(g_pRequest);
		KdcDeleteTemplate();
		g_pRequest = NULL;
	}
	if (pUrl != NULL)
		sceClibMspaceFree(g_mspace, pUrl);
	KdcCheckActData(&actType2, &versionFlag2, (SceUInt64 *)accountId2, (SceUInt32)expTime3, (SceUInt32)expTime4);
	g_busy = 0;
	return ret;
}

/* the form posted with the content's ID, the answer read into pData */
__attribute__((noinline))
static int KdcRequestLicense(const char *pContentId, void *pData, SceSize size, SceSize *pRead,
    const char *pEncryptedPassword)
{
	char *pUrl = NULL;
	SceUInt8 actData[32];
	int ret;

	if (pContentId == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (pData == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (pRead == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	ret = KdcOpenRequest(2, &pUrl, &g_pRequest, actData, pEncryptedPassword);
	if (ret >= 0) {
		ret = KdcAddProductId(g_pRequest, pContentId);
		if (ret >= 0) {
			ret = KdcSend(g_pRequest, pUrl);
			if (ret >= 0) {
				ret = KdcReadData(g_pRequest, pData, size, pRead);
				if (ret >= 0)
					ret = 0;
			}
		}
	}
	if (g_pRequest != NULL) {
		KdcDeleteRequest(g_pRequest);
		KdcDeleteTemplate();
		g_pRequest = NULL;
	}
	if (pUrl != NULL)
		sceClibMspaceFree(g_mspace, pUrl);
	return ret;
}

int SceNpKdc_78E56B9B(const char *pContentId, const char *pEncryptedPassword, void *pData, SceSize size,
    SceSize *pRead, int *pMax, int *pCurrent)
{
	int ret;
	int err = 0;
	int retried = 0;
	int actType;
	int versionFlag;
	SceUInt8 accountId[8];
	SceUInt8 expTime[8];
	SceUInt8 expTime2[8];
	int actType2;
	int versionFlag2;
	SceUInt8 accountId2[8];
	SceUInt8 expTime3[8];
	SceUInt8 expTime4[8];

	if (pContentId == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (pData == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (pRead == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (size < 0x200)
		return NP_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return NP_KDC_ERROR_NOT_INITIALIZED;
	if (g_busy != 0)
		return NP_KDC_ERROR_BUSY_OTHER;
	ret = KdcCheckActData(&actType, &versionFlag, (SceUInt64 *)accountId, (SceUInt32)expTime, (SceUInt32)expTime2);
	g_actType = (ret >= 0) ? actType : -1;
	g_busy = 1;
	if (g_actType != -1)
		goto activate;
	for (;;) {
		ret = KdcRequestLicense(pContentId, pData, size, pRead, pEncryptedPassword);
		if (ret >= 0) {
			ret = 0;
			break;
		}
		if (ret != NP_KDC_ERROR_SERVER_RETRY || retried != 0)
			break;
activate:
		err = KdcActivate((SceUInt32 *)pMax, (SceUInt32 *)pCurrent, pEncryptedPassword);
		if (err < 0 && err != NP_KDC_ERROR_ACTIVATION_FAILED) {
			ret = err;
			goto out;
		}
		retried = 1;
	}
	if (err == NP_KDC_ERROR_ACTIVATION_FAILED)
		ret = NP_KDC_ERROR_ACTIVATION_FAILED;
out:
	KdcCheckActData(&actType2, &versionFlag2, (SceUInt64 *)accountId2, (SceUInt32)expTime3, (SceUInt32)expTime4);
	g_busy = 0;
	return ret;
}

/* the license from the console's own computer over MTP: the MTP client is loaded for the call */
int SceNpKdc_A1441E8C(const char *pContentId, void *pData, SceSize size, SceSize *pRead, int bufferSize, int reserved)
{
	int ret = 0;
	int err;
	int rejected = 0;
	int retried = 0;
	int actType;
	int versionFlag;
	SceUInt8 accountId[8];
	SceUInt8 expTime[8];
	SceUInt8 expTime2[8];
	int actType2;
	int versionFlag2;
	SceUInt8 accountId2[8];
	SceUInt8 expTime3[8];
	SceUInt8 expTime4[8];
	SceUInt8 name[48];
	SceUInt8 key[128];
	SceUInt8 key2[128];
	SceUInt8 actData[32];
	SceUInt8 actData2[32];
	int objectId;
	int status;
	int *pBuffer;
	void *pReceive;
	int *pObject;
	int *pClient;

	if (pContentId == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (pData == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (pRead == NULL)
		return NP_KDC_ERROR_INVALID_ARG;
	if (size < 0x200)
		return NP_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return NP_KDC_ERROR_NOT_INITIALIZED;
	if (g_busy != 0)
		return NP_KDC_ERROR_BUSY_OTHER;
	err = KdcCheckActData(&actType, &versionFlag, (SceUInt64 *)accountId, (SceUInt32)expTime, (SceUInt32)expTime2);
	g_actType = (err >= 0) ? actType : -1;
	g_busy = 1;
	g_mtpModuleId = 0;
	pObject = ScePafStdc_FC5CD359(4);
	if (pObject != NULL)
		g_mtpModuleId = ScePafMisc_B3B5DF38(pObject, "vs0:vsh/common/mtp_client.suprx", 0, 0, 0);
	if (g_mtpModuleId == 0) {
		ret = NP_KDC_ERROR_MTP_UNAVAILABLE;
	} else {
		pClient = ScePafStdc_FC5CD359(96);
		if (pClient != NULL) {
			pClient[0] = 0;
			pClient[1] = 0;
			pClient[10] = 0;
			pClient[11] = 0;
			pClient[12] = 0;
			pClient[16] = 0;
			pBuffer = ScePafStdc_FC5CD359(24);
			if (pBuffer == NULL)
				ScePafStdc_D608246A();
			pClient[15] = (int)pBuffer;
			if (pBuffer != NULL) {
				pBuffer[0] = (int)pBuffer;
				pBuffer[1] = (int)pBuffer;
				pBuffer[2] = (int)pBuffer;
				((SceUInt8 *)pBuffer)[20] = 0;
				((SceUInt8 *)pClient[15])[21] = 1;
			}
			pClient[18] = 0;
			pClient[19] = 0;
			pClient[20] = 0;
			pClient[21] = -1;
			pClient[22] = 0;
			pClient[23] = -1;
		}
		g_pMtpClient = pClient;
		if (pClient == NULL) {
			ret = NP_KDC_ERROR_NO_MEMORY;
		} else {
			ret = SceMtpClient_99E4CEED(pClient);
			if (ret >= 0)
				ret = 0;
		}
		if (ret < 0 && g_pMtpClient != NULL) {
			ScePafStdc_1B77082E(SceMtpClient_90A60690());
			g_pMtpClient = NULL;
		}
	}
	err = ret;
	if (ret >= 0) {
		objectId = 0;
		if (g_actType == -1)
			goto download;
		for (;;) {
			status = -1;
			objectId = 0;
			pReceive = sceClibMspaceMalloc(g_mspace, 0x204);
			if (pReceive != NULL) {
				sceClibStrncpy((char *)name, pContentId, 0x30);
				err = SceLibKernel_9557D15C(2, actData2, key2);
				if (err >= 0) {
					err = SceMtpClient_BFB9E02B(g_pMtpClient, name, key2, 128, &objectId);
					if (err >= 0) {
						err = SceMtpClient_0FF15EF8(g_pMtpClient, objectId, pReceive, &status);
						if (err >= 0) {
							if (status >= 0) {
								if (*((SceUInt32 *)pReceive + 128) <= size) {
									sceClibMemcpy_safe(pData, pReceive, *((SceUInt32 *)pReceive + 128));
									err = 0;
									*pRead = *((SceUInt32 *)pReceive + 128);
								} else {
									err = NP_KDC_ERROR_BUFFER_TOO_SMALL;
								}
							} else {
								err = (SceUInt8)status - 2141879552;
							}
						}
					}
				}
			} else {
				err = NP_KDC_ERROR_NO_MEMORY;
			}
			if (pReceive != NULL)
				sceClibMspaceFree(g_mspace, pReceive);
			ret = err;
			if (err >= 0) {
				ret = 0;
				goto cleanup;
			}
			if (err != NP_KDC_ERROR_SERVER_RETRY || retried != 0)
				break;
download:
			{
				int first = 0;
				int tries = 0;
				int pair[2];
				void *pBuf;

				pair[0] = 0;
				pair[1] = 0;
				pBuf = sceClibMspaceMalloc(g_mspace, 0x1090);
				if (pBuf != NULL) {
					for (;;) {
						ret = SceLibKernel_9557D15C(0, actData, key);
						if (ret < 0)
							break;
						if (tries != 0)
							first = 1;
						ret = SceMtpClient_9920926A(g_pMtpClient, first, key, 128, pair);
						if (ret < 0)
							break;
						status = SceMtpClient_1EA892F7(g_pMtpClient, pair[0], pBuf, bufferSize);
						if ((status | pair[1]) < 0) {
							ret = (SceUInt8)pair[1] - 2141879552;
							if ((SceUInt8)pair[1] == 80 && tries != 0)
								ret = NP_KDC_ERROR_ACTIVATION_FAILED;
							else if ((SceUInt8)pair[1] == 20)
								ret = 0;
							break;
						}
						ret = sceSblGcAuthMgrPcactActivation(pBuf, 4240, 0, 0, reserved);
						if (ret < 0)
							break;
						if (tries != 0) {
							ret = 0;
							break;
						}
						tries = 1;
					}
				} else {
					ret = NP_KDC_ERROR_NO_MEMORY;
				}
				if (pBuf != NULL)
					sceClibMspaceFree(g_mspace, pBuf);
				rejected = ret;
				if (ret < 0 && ret != NP_KDC_ERROR_ACTIVATION_FAILED)
					goto cleanup;
				retried = 1;
			}
		}
		if (rejected == NP_KDC_ERROR_ACTIVATION_FAILED)
			ret = NP_KDC_ERROR_ACTIVATION_FAILED;
	} else {
		ret = err;
	}
cleanup:
	if (g_pMtpClient != NULL) {
		SceMtpClient_8313A28C();
		if (g_pMtpClient != NULL)
			ScePafStdc_1B77082E(SceMtpClient_90A60690());
		g_pMtpClient = NULL;
	}
	KdcCheckActData(&actType2, &versionFlag2, (SceUInt64 *)accountId2, (SceUInt32)expTime3, (SceUInt32)expTime4);
	g_busy = 0;
	return ret;
}

int SceNpKdc_C3B78C87(void)
{
	if (g_pRequest != NULL)
		return KdcAbort(g_pRequest);
	return NP_KDC_ERROR_NOT_STARTED;
}

__attribute__((noinline))
static void KdcInitHashes(void)
{
	g_hashLiveAreaPath = KdcHash15("#_livearea_path");
	g_hashCTime = KdcHash7("#_ctime");
	g_hashLLTime = KdcHash8("#_lltime");
	g_hashPromoteAttr = KdcHash14("#_promote_attr");
	g_hashUpStatus = KdcHash11("#_up_status");
	g_hashLibLocation = KdcHash12("#LIBLOCATION");
	g_hashAppConf = KdcHash8("#appconf");
}

/* the form's body: each field's name, then its escaped value, into pOut (or only its size) */
__attribute__((noinline))
static int KdcBuildBody(KdcParamList *pList, char *pOut, SceSize prepare, SceSize *pSize)
{
	const KdcParam *pParam;
	SceSize total = 0;
	SceSize require;
	SceSize used;
	int ret;

	for (pParam = pList->pHead; pParam != NULL; pParam = pParam->pNext) {
		require = pParam->pDesc->length;
		if (pOut != NULL) {
			if (prepare <= require)
				return NP_KDC_ERROR_BUFFER_TOO_SMALL;
			sceClibStrncpy(pOut, pParam->pDesc->pName, require);
			pOut += require;
			prepare -= require;
		}
		used = total + require;
		ret = sceHttpUriEscape(pOut, &require, prepare, pParam->value);
		total = used + --require;
		if (ret != 0)
			return ret;
		if (pOut != NULL) {
			pOut += require;
			prepare -= require;
		}
	}
	if (pSize != NULL)
		*pSize = total + 1;
	if (pOut != NULL && prepare != 0)
		*pOut = 0;
	return 0;
}

__attribute__((noinline))
static int KdcConvertHttpResult(int requestId, int result)
{
	SceUInt32 detail;
	int errNum;
	int ret;

	if (result != (int)0x80431075)
		return result;
	ret = sceHttpsGetSslError(requestId, &errNum, &detail);
	if (ret < 0)
		return ret;
	ret = errNum;
	if (errNum == (int)0x80435060) {
		ret = NP_KDC_ERROR_SSL_UNTRUSTED;
		if ((detail & 4) == 0) {
			ret = NP_KDC_ERROR_SSL_EXPIRED;
			if ((detail & 0x20) == 0) {
				ret = NP_KDC_ERROR_SSL_NAME;
				if ((detail & 2) == 0) {
					ret = NP_KDC_ERROR_SSL_BAD_CERT;
					if ((detail & 1) != 0)
						ret = NP_KDC_ERROR_SSL_UNKNOWN_CA;
				}
			}
		}
	}
	return ret;
}

/* a header field of the answer the server must send */
static inline __attribute__((always_inline)) int KdcCheckHeader(const char *pHeader, SceSize headerSize, const KdcHeaderField *pFields)
{
	const char *pValue;
	SceSize valueSize;
	int ret;
	int i;

	for (i = 0; i < 1; i++) {
		ret = sceHttpParseResponseHeader(pHeader, headerSize, pFields[i].pName, &pValue, &valueSize);
		if (ret < 0)
			return ret;
		if (sceClibStrncmp(pValue, pFields[i].pValue, sceClibStrnlen(pFields[i].pValue, 0x7FFFFFFF)) != 0)
			return NP_KDC_ERROR_BAD_RESPONSE;
	}
	return 0;
}

__attribute__((noinline))
static int KdcSendRequest(KdcRequest *pRequest, const char *pUrl)
{
	char *pHeader;
	SceSize headerSize;
	const char *pStatus;
	SceSize statusSize;
	char *pEnd;
	char *pValue;
	SceUInt64 contentLength;
	SceUInt64 postLength;
	SceSize bodySize;
	char number[16];
	int statusCode;
	int ret;
	int i;

	if (pRequest->pBody != NULL)
		return NP_KDC_ERROR_BUSY;
	ret = KdcBuildBody(&pRequest->params, NULL, 0, &bodySize);
	if (ret >= 0) {
		pRequest->bodySize = (bodySize > 0x400) ? bodySize : 0x400;
		pRequest->pBody = KdcAlloc(pRequest->bodySize + 1);
		if (pRequest->pBody == NULL)
			return NP_KDC_ERROR_NO_MEMORY;
		sceClibMemset(pRequest->pBody, 0, pRequest->bodySize + 1);
		ret = KdcBuildBody(&pRequest->params, pRequest->pBody, pRequest->bodySize, &bodySize);
		if (ret >= 0) {
			postLength = bodySize - 1;
			ret = sceHttpCreateRequestWithURL(pRequest->connectionId, 1, pUrl, postLength);
			if (ret >= 0) {
				pRequest->requestId = ret;
				for (i = 0; i < 3; i++) {
					ret = sceHttpAddRequestHeader(pRequest->requestId, s_requestHeaders[i].pName,
					    s_requestHeaders[i].pValue, 0);
					if (ret < 0)
						goto fail;
				}
				ret = sceHttpSendRequest(pRequest->requestId, pRequest->pBody, bodySize - 1);
				if (ret < 0) {
					ret = KdcConvertHttpResult(pRequest->requestId, ret);
					goto check;
				}
				ret = sceHttpGetStatusCode(pRequest->requestId, &statusCode);
				if (ret < 0)
					goto fail;
				if (statusCode != 200) {
					ret = NP_KDC_ERROR_BAD_STATUS_CODE;
					goto fail;
				}
				ret = sceHttpGetAllResponseHeaders(pRequest->requestId, &pHeader, &headerSize);
				if (ret < 0)
					goto fail;
				ret = KdcCheckHeader(pHeader, headerSize, s_versionHeader);
				if (ret < 0)
					goto fail;
				if (sceHttpParseResponseHeader(pHeader, headerSize, "X-I-5-DRM-Status", (const char **)&pStatus,
				    &statusSize) < 0) {
					ret = NP_KDC_ERROR_BAD_RESPONSE;
					goto fail;
				}
				pEnd = sceClibStrstr(pStatus, "\r\n");
				if (sceClibStrncmp("OK", pStatus, 2) != 0) {
					pRequest->drmRefused = 1;
					pValue = sceClibStrstr(pStatus, "reason=");
					if (pValue == NULL) {
						ret = NP_KDC_ERROR_BAD_REASON;
					} else {
						SceSize length;
						char *pSemicolon;

						pValue += 7;
						pSemicolon = sceClibStrchr(pValue, ';');
						length = ((pSemicolon != NULL) ? pSemicolon : pEnd) - pValue;
						if (length > 15) {
							ret = NP_KDC_ERROR_BAD_REASON;
						} else {
							sceClibMemcpy_safe(number, pValue, length);
							number[length] = 0;
							pRequest->reason = KdcStrtol(number, NULL, 16);
							ret = 0;
							if (pRequest->reason == (int)0x80000000 || pRequest->reason == 0x7FFFFFFF)
								ret = NP_KDC_ERROR_BAD_REASON;
						}
					}
				} else {
					pRequest->drmRefused = 0;
					ret = 0;
					pValue = sceClibStrstr(pStatus, "max_console=");
					if (pValue != NULL) {
						SceSize length;
						char *pSemicolon;
						SceUInt32 value;

						pValue += 12;
						pSemicolon = sceClibStrchr(pValue, ';');
						length = ((pSemicolon != NULL) ? pSemicolon : pEnd) - pValue;
						if (length <= 15) {
							sceClibMemcpy_safe(number, pValue, length);
							number[length] = 0;
							value = KdcStrtol(number, NULL, 10);
							pRequest->maxConsole = value;
							if (value != 0x80000000 && value <= 0x7FFFFFFE) {
								pValue = sceClibStrstr(pStatus, "current_console=");
								if (pValue != NULL) {
									pValue += 16;
									pSemicolon = sceClibStrchr(pValue, ';');
									length = ((pSemicolon != NULL) ? pSemicolon : pEnd) - pValue;
									if (length <= 15) {
										sceClibMemcpy_safe(number, pValue, length);
										number[length] = 0;
										value = KdcStrtol(number, NULL, 10);
										pRequest->currentConsole = value;
										if (value != 0x80000000 && value <= 0x7FFFFFFE)
											pRequest->flags |= KDC_FLAG_CONSOLE_INFO;
									}
								}
							}
						}
					}
				}
				if (ret < 0)
					goto fail;
				if (pRequest->drmRefused == 0) {
					ret = sceHttpGetResponseContentLength(pRequest->requestId, &contentLength);
					if (ret >= 0) {
						if (contentLength == 0)
							return 0;
						ret = KdcCheckHeader(pHeader, headerSize, s_contentHeader);
						if (ret >= 0)
							return 0;
					}
					goto fail;
				}
				if (pRequest->reason > 255) {
					ret = NP_KDC_ERROR_DRM_REFUSED;
					goto fail;
				}
				ret = pRequest->reason | NP_KDC_ERROR_SERVER_BASE;
check:
				if (ret >= 0)
					goto release;
			}
		}
fail:
		if (pRequest->requestId > 0) {
			sceHttpDeleteRequest(pRequest->requestId);
			pRequest->requestId = 0;
		}
release:
		if (pRequest->pBody != NULL) {
			KdcFree(pRequest->pBody);
			pRequest->pBody = NULL;
			pRequest->bodySize = 0;
		}
	}
	return ret;
}

__attribute__((noinline))
static int KdcCreateTemplate(void)
{
	int ret;
	int err;

	ret = sceHttpCreateTemplate("My heart leaps up when I behold A rainbow in the sky", 1, 0);
	g_httpTemplate = ret;
	if (ret < 0)
		return ret;
	err = sceHttpSetConnectTimeOut(ret, 20000000);
	if (err < 0
	    || (err = sceHttpSetSendTimeOut(g_httpTemplate, 20000000)) < 0
	    || (err = sceHttpSetRecvTimeOut(g_httpTemplate, 60000000)) < 0) {
		if (g_httpTemplate > 0) {
			sceHttpDeleteTemplate(g_httpTemplate);
			g_httpTemplate = -1;
		}
		return err;
	}
	sceHttpsEnableOptionPrivate(g_httpTemplate, 357);
	sceHttpsDisableOptionPrivate(g_httpTemplate, 24);
	g_httpReady = 1;
	return 0;
}

__attribute__((noinline))
static int KdcDeleteTemplate(void)
{
	int ret;

	if (g_httpTemplate > 0) {
		ret = sceHttpDeleteTemplate(g_httpTemplate);
		if (ret < 0)
			return ret;
		g_httpTemplate = -1;
	}
	g_httpReady = 0;
	return 0;
}

__attribute__((noinline))
static int KdcCreateRequest(KdcRequest **ppRequest, const char *pUrl)
{
	KdcRequest *pRequest;
	int ret;

	pRequest = KdcAlloc(sizeof(KdcRequest));
	*ppRequest = pRequest;
	if (pRequest == NULL)
		return NP_KDC_ERROR_NO_MEMORY;
	sceClibMemset(pRequest, 0, sizeof(KdcRequest));
	pRequest->params.pHead = NULL;
	pRequest->params.pTail = NULL;
	pRequest->params.mask = 0;
	ret = sceHttpCreateConnectionWithURL(g_httpTemplate, pUrl, 1);
	if (ret >= 0) {
		pRequest->connectionId = ret;
		ret = 0;
		pRequest->pBody = NULL;
	}
	if (ret >= 0)
		return 0;
	return ret;
}

__attribute__((noinline))
static int KdcDeleteRequest(KdcRequest *pRequest)
{
	KdcParam *pParam;

	if (pRequest->requestId > 0) {
		sceHttpDeleteRequest(pRequest->requestId);
		pRequest->requestId = 0;
	}
	sceHttpDeleteConnection(pRequest->connectionId);
	if (pRequest->pBody != NULL) {
		KdcFree(pRequest->pBody);
		pRequest->pBody = NULL;
	}
	for (pParam = pRequest->params.pHead; pParam != NULL; pParam = pRequest->params.pHead) {
		pRequest->params.pHead = pParam->pNext;
		KdcFree(pParam);
	}
	KdcFree(pRequest);
	return 0;
}

/* a field of the form: '&' fields go to the end, loginid to the front */
__attribute__((noinline))
static int KdcAddParam(KdcParamList *pList, unsigned int index, const void *pValue, SceSize size)
{
	KdcParam *pParam;

	if (index >= 8)
		return NP_KDC_ERROR_INVALID_ARG;
	pParam = KdcAlloc(size + 13);
	if (pParam == NULL)
		return NP_KDC_ERROR_NO_MEMORY;
	sceClibMemset(pParam, 0, size + 13);
	pParam->pNext = NULL;
	pParam->size = size;
	pParam->pDesc = &g_paramDescs[index];
	sceClibMemcpy_safe(pParam->value, pValue, size);
	if (pList->pHead != NULL) {
		if (pParam->pDesc->pName[0] == '&') {
			pList->pTail->pNext = pParam;
			pList->pTail = pParam;
			pParam->pNext = NULL;
		} else {
			pParam->pNext = pList->pHead;
			pList->pHead = pParam;
		}
	} else {
		pList->pHead = pParam;
		pList->pTail = pParam;
	}
	pList->mask |= 1 << index;
	return 0;
}

__attribute__((noinline))
static int KdcAddCredentials(KdcRequest *pRequest, const char *pLoginId, const char *pEncryptedPassword)
{
	int ret;

	ret = KdcAddParam(&pRequest->params, KDC_PARAM_LOGINID, pLoginId, sceClibStrnlen(pLoginId, 0x7FFFFFFF));
	if (ret >= 0) {
		int err = KdcAddParam(&pRequest->params, KDC_PARAM_EPASSWORD, pEncryptedPassword,
		    sceClibStrnlen(pEncryptedPassword, 0x7FFFFFFF));

		ret = 0;
		if (err < 0)
			return err;
	}
	return ret;
}

__attribute__((noinline))
static int KdcAddProductId(KdcRequest *pRequest, const char *pProductId)
{
	return KdcAddParam(&pRequest->params, KDC_PARAM_PRODUCTID, pProductId, sceClibStrnlen(pProductId, 0x7FFFFFFF));
}

/* the console's 128 byte key in hex */
__attribute__((noinline))
static int KdcAddConsoleKey(KdcRequest *pRequest, const SceUInt8 *pKey)
{
	char hex[257];
	char *pOut;
	int err;
	int ret;
	int i;

	sceClibMemset(hex, 0, 0x101);
	pOut = hex;
	for (i = 128; i != 0; i--) {
		sceClibSnprintf(pOut, 3, "%02x", *pKey++);
		pOut += 2;
	}
	err = KdcAddParam(&pRequest->params, KDC_PARAM_C1, hex, 0x100);
	ret = 0;
	if (err < 0)
		ret = err;
	return ret;
}

__attribute__((noinline))
static int KdcAddPlatform(KdcRequest *pRequest, const char *pPlatform)
{
	return KdcAddParam(&pRequest->params, KDC_PARAM_PLATFORM, pPlatform, sceClibStrnlen(pPlatform, 0x7FFFFFFF));
}

__attribute__((noinline))
static int KdcAddActType(KdcRequest *pRequest, const char *pActType)
{
	return KdcAddParam(&pRequest->params, KDC_PARAM_ACTTYPE, pActType, sceClibStrnlen(pActType, 0x7FFFFFFF));
}

__attribute__((noinline))
static int KdcSend(KdcRequest *pRequest, const char *pUrl)
{
	return KdcSendRequest(pRequest, pUrl);
}

__attribute__((noinline))
static int KdcGetContentLength(KdcRequest *pRequest, SceUInt64 *pLength)
{
	return sceHttpGetResponseContentLength(pRequest->requestId, pLength);
}

__attribute__((noinline))
static int KdcGetConsoleInfo(KdcRequest *pRequest, SceUInt32 *pMax, SceUInt32 *pCurrent)
{
	int ret = NP_KDC_ERROR_NO_CONSOLE_INFO;

	if ((pRequest->flags & KDC_FLAG_CONSOLE_INFO) != 0) {
		ret = 0;
		*pMax = pRequest->maxConsole;
		*pCurrent = pRequest->currentConsole;
	}
	return ret;
}

__attribute__((noinline))
static int KdcReadData(KdcRequest *pRequest, char *pData, SceSize size, SceSize *pRead)
{
	char *p;
	int ret;
	int n;

	*pRead = 0;
	p = pData;
	do {
		n = sceHttpReadData(pRequest->requestId, p, size);
		if (n < 0) {
			ret = KdcConvertHttpResult(pRequest->requestId, n);
			goto done;
		}
		size -= n;
		p += n;
	} while (size != 0 && n != 0);
	ret = 0;
done:
	*pRead = p - pData;
	return ret;
}

__attribute__((noinline))
static int KdcAbort(KdcRequest *pRequest)
{
	return sceHttpAbortRequest(pRequest->requestId);
}

/* strtol: the digits of a number in the base (0: from its prefix) */
__attribute__((noinline))
static int KdcStrtol(const char *pString, char **ppEnd, int base)
{
	const char *p = pString;
	const char *pNext;
	char c;
	int sign = 1;
	int value = 0;
	int digit;
	int ch;

	if (pString == NULL)
		return 0;
	c = *p;
	pNext = p + 1;
	while ((sceClibLookCtypeTable(c) & 8) != 0) {
		p = pNext;
		c = *pNext++;
	}
	ch = *p;
	if (ch == '-')
		goto sign;
	while (ch == '+') {
		do {
sign:
			if (ch == '-')
				sign = -sign;
			p = pNext;
			ch = *pNext++;
		} while (ch == '-');
	}
	if (base < 2 || base > 36)
		base = 0;
	if (ch != '0') {
		if (sceClibToupper(ch) == 'O') {
			p = pNext++;
			base = 8;
			goto digits;
		}
		goto decimal;
	}
	p = pNext;
	{
		int next = *pNext;
		const char *pPrev = pNext++;

		if (next != 'x') {
			if (next == 'b')
				goto binary;
			if (next != 'X') {
				if (next != 'B') {
					if (base == 0)
						base = 8;
					goto digits;
				}
binary:
				if (base == 0 || base == 2) {
					p = pNext;
					pNext = pPrev + 2;
					base = 2;
					goto digits;
				}
decimal:
				if (base == 0)
					base = 10;
				goto digits;
			}
		}
		if (base != 0 && base != 16)
			goto decimal;
		p = pNext;
		pNext = pPrev + 2;
		base = 16;
	}
digits:
	for (;; value = base * value + digit) {
		c = *p;
		p = pNext;
		ch = c;
		if ((sceClibLookCtypeTable(c) & 4) != 0) {
			digit = ch - '0';
		} else {
			digit = 9999999;
			if ((sceClibLookCtypeTable(ch) & 3) != 0)
				digit = sceClibTolower(ch) - 87;
		}
		if (digit >= base)
			break;
		pNext = p + 1;
	}
	if (ppEnd != NULL)
		*ppEnd = (char *)(p - 1);
	return value * sign;
}

__attribute__((noinline))
int KdcFunc17F4(void)
{
	return 9;
}

__attribute__((noinline))
int KdcFunc1812(void)
{
	return 9;
}

__attribute__((noinline))
static unsigned int KdcHash7(const unsigned char *p)
{
	return 16777619u * (p[6] ^ (16777619u * (p[5] ^ (16777619u * (p[4] ^ (16777619u * (p[3] ^ (16777619u * (p[2] ^ (16777619u * (p[1] ^ (16777619u * (p[0] ^ 0x811C9DC5u)))))))))))));
}

__attribute__((noinline))
static unsigned int KdcHash8(const unsigned char *p)
{
	return 16777619u * (p[7] ^ (16777619u * (p[6] ^ (16777619u * (p[5] ^ (16777619u * (p[4] ^ (16777619u * (p[3] ^ (16777619u * (p[2] ^ (16777619u * (p[1] ^ (16777619u * (p[0] ^ 0x811C9DC5u)))))))))))))));
}

__attribute__((noinline))
static unsigned int KdcHash11(const unsigned char *p)
{
	return 16777619u * (p[10] ^ (16777619u * (p[9] ^ (16777619u * (p[8] ^ (16777619u * (p[7] ^ (16777619u * (p[6] ^ (16777619u * (p[5] ^ (16777619u * (p[4] ^ (16777619u * (p[3] ^ (16777619u * (p[2] ^ (16777619u * (p[1] ^ (16777619u * (p[0] ^ 0x811C9DC5u)))))))))))))))))))));
}

__attribute__((noinline))
static unsigned int KdcHash12(const unsigned char *p)
{
	return 16777619u * (p[11] ^ (16777619u * (p[10] ^ (16777619u * (p[9] ^ (16777619u * (p[8] ^ (16777619u * (p[7] ^ (16777619u * (p[6] ^ (16777619u * (p[5] ^ (16777619u * (p[4] ^ (16777619u * (p[3] ^ (16777619u * (p[2] ^ (16777619u * (p[1] ^ (16777619u * (p[0] ^ 0x811C9DC5u)))))))))))))))))))))));
}

__attribute__((noinline))
static unsigned int KdcHash14(const unsigned char *p)
{
	return 16777619u * (p[13] ^ (16777619u * (p[12] ^ (16777619u * (p[11] ^ (16777619u * (p[10] ^ (16777619u * (p[9] ^ (16777619u * (p[8] ^ (16777619u * (p[7] ^ (16777619u * (p[6] ^ (16777619u * (p[5] ^ (16777619u * (p[4] ^ (16777619u * (p[3] ^ (16777619u * (p[2] ^ (16777619u * (p[1] ^ (16777619u * (p[0] ^ 0x811C9DC5u)))))))))))))))))))))))))));
}

__attribute__((noinline))
static unsigned int KdcHash15(const unsigned char *p)
{
	return 16777619u * (p[14] ^ (16777619u * (p[13] ^ (16777619u * (p[12] ^ (16777619u * (p[11] ^ (16777619u * (p[10] ^ (16777619u * (p[9] ^ (16777619u * (p[8] ^ (16777619u * (p[7] ^ (16777619u * (p[6] ^ (16777619u * (p[5] ^ (16777619u * (p[4] ^ (16777619u * (p[3] ^ (16777619u * (p[2] ^ (16777619u * (p[1] ^ (16777619u * (p[0] ^ 0x811C9DC5u)))))))))))))))))))))))))))));
}

__attribute__((noinline))
static int KdcCheckActData(int *pActType, int *pVersionFlag, SceUInt64 *pAccountId, SceUInt32 expTimeLow,
    SceUInt32 expTimeHigh)
{
	KdcActTime expTime;

	expTime.lo = expTimeLow;
	expTime.hi = expTimeHigh;
	return _sceNpDrmCheckActData(pActType, pVersionFlag, pAccountId, (SceUInt64 *)&expTime);
}

__attribute__((noinline))
static int KdcRemoveActData(int a0)
{
	return _sceNpDrmRemoveActData(a0);
}

int KdcGetRifInfo(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11)
{
	KdcRifInfo info;

	info.a6 = a6;
	info.a7 = a7;
	info.a3 = a3;
	info.a4 = a4;
	info.a5 = a5;
	info.a8 = a8;
	info.a9 = a9;
	info.a10 = a10;
	info.a11 = a11;
	return _sceNpDrmGetRifInfo(a0, a1, a2, (int *)&info);
}

int KdcGetRifName(char *pRifName, SceUInt64 aid)
{
	return _sceNpDrmGetRifName(pRifName, aid);
}

int KdcGetFixedRifName(char *pRifName, SceUInt64 aid)
{
	return _sceNpDrmGetFixedRifName(pRifName, aid);
}

int KdcGetRifNameForInstall(char *pRifName, const void *pRif, int a2)
{
	return _sceNpDrmGetRifNameForInstall(pRifName, pRif, a2);
}

int KdcCheckDrmReset(int a0, int a1, int a2, int a3, int a4)
{
	KdcDrmResetInfo info;

	info.a2 = a2;
	info.a3 = a3;
	info.a4 = a4;
	info.a1 = a1;
	return _sceNpDrmCheckDrmReset(a0, a1, (int *)&info);
}

int KdcPresetRifProvisionalFlag(void)
{
	return _sceNpDrmPresetRifProvisionalFlag();
}
