/*
 * SceNpKdc's own types and helpers. No debug record names them; the names are ours, taken from
 * what the code does.
 */
#ifndef _NP_KDC_INTERNAL_H
#define _NP_KDC_INTERNAL_H

#include <scetypes.h>
#include "np_kdc.h"

#define NP_KDC_ERROR_NO_MEMORY			((int)0x80558201)
#define NP_KDC_ERROR_INVALID_ARG		((int)0x80558202)
#define NP_KDC_ERROR_BUFFER_TOO_SMALL		((int)0x80558203)
#define NP_KDC_ERROR_BUSY			((int)0x80558204)
#define NP_KDC_ERROR_BAD_RESPONSE		((int)0x80558205)
#define NP_KDC_ERROR_BAD_REASON			((int)0x80558206)
#define NP_KDC_ERROR_DRM_REFUSED		((int)0x80558208)
#define NP_KDC_ERROR_BAD_STATUS_CODE		((int)0x80558209)
#define NP_KDC_ERROR_NO_CONSOLE_INFO		((int)0x8055820a)
#define NP_KDC_ERROR_MTP_UNAVAILABLE		((int)0x8055820d)
#define NP_KDC_ERROR_NOT_STARTED		((int)0x80558211)
#define NP_KDC_ERROR_BUSY_OTHER			((int)0x80558212)
#define NP_KDC_ERROR_NOT_INITIALIZED		((int)0x80558213)
#define NP_KDC_ERROR_SSL_UNTRUSTED		((int)0x80558220)
#define NP_KDC_ERROR_SSL_EXPIRED		((int)0x80558221)
#define NP_KDC_ERROR_SSL_NAME			((int)0x80558222)
#define NP_KDC_ERROR_SSL_UNKNOWN_CA		((int)0x80558223)
#define NP_KDC_ERROR_SSL_BAD_CERT		((int)0x80558224)
#define NP_KDC_ERROR_MTP_STATUS			((int)0x80558229)
#define NP_KDC_ERROR_ACTIVATION_FAILED		((int)0x80558228)
#define NP_KDC_ERROR_ALREADY_ACTIVATED		((int)0x80558314)
#define NP_KDC_ERROR_SERVER_REFUSED		((int)0x80558350)
#define NP_KDC_ERROR_SERVER_RETRY		((int)0x80558351)
#define NP_KDC_ERROR_SERVER_BASE		((int)0x80558300)

#define KDC_NUM_PARAMS		8

/* the names of the form's fields */
enum {
	KDC_PARAM_LOGINID,
	KDC_PARAM_PASSWORD,
	KDC_PARAM_EPASSWORD,
	KDC_PARAM_CONSOLEID,
	KDC_PARAM_PRODUCTID,
	KDC_PARAM_PLATFORM,
	KDC_PARAM_C1,
	KDC_PARAM_ACTTYPE
};

typedef struct KdcParamDesc {
	SceSize length;
	const char *pName;
} KdcParamDesc;

typedef struct KdcHeaderField {
	const char *pName;
	const char *pValue;
} KdcHeaderField;

/* one field of the form: the name, then the value to escape */
typedef struct KdcParam {
	struct KdcParam *pNext;
	const KdcParamDesc *pDesc;
	SceSize size;
	char value[1];
} KdcParam;

/* the form's fields, with the ones present */
typedef struct KdcParamList {
	KdcParam *pHead;
	KdcParam *pTail;
	SceUInt32 mask;
} KdcParamList;

/* the request: its connection, the form and the server's answer */
typedef struct KdcRequest {
	SceInt32 connectionId;
	SceInt32 requestId;
	KdcParamList params;
	SceUInt32 flags;		/* 1: the answer names the consoles */
	char *pBody;
	SceSize bodySize;
	SceUInt32 maxConsole;
	SceUInt32 currentConsole;
	int reason;
	int drmRefused;
} KdcRequest;

typedef struct KdcRifInfo {
	int a3, a4, a5, a6, a7, a8, a9, a10, a11;
} KdcRifInfo;

typedef struct KdcActTime {
	SceUInt32 lo;
	SceUInt32 hi;
} KdcActTime;

typedef struct KdcDrmResetInfo {
	int a2, a3, a4, a1;
} KdcDrmResetInfo;

#define KDC_FLAG_CONSOLE_INFO	1

extern SceUInt8 g_busy;
extern KdcRequest *g_pRequest;
extern SceUInt8 g_initialized;
extern SceInt32 g_actType;
extern SceUID g_mtpModuleId;
extern void *g_pMtpClient;
extern SceInt32 g_httpReady;
extern SceInt32 g_httpTemplate;
extern void *g_mspace;
extern const KdcParamDesc g_paramDescs[KDC_NUM_PARAMS];

/* the other libraries (names are the NIDs: nothing names them) */
extern int sceNpCommonGetNpEnviroment(char *pEnvironment);
extern int SceNpManager_DDF30B0E(char *pLoginId, void *pPassword, void *pReserved);
extern int SceLibKernel_9557D15C(int type, void *pActData, void *pConsoleKey);
extern int sceSblGcAuthMgrPcactActivation(void *pData, SceSize size, SceSize size2, int a3, int a4);
extern void *ScePafStdc_FC5CD359(SceSize size);
extern void ScePafStdc_1B77082E(void *pObject);
extern void ScePafStdc_D608246A(void);
extern void *ScePafMisc_7C86911E(SceUID id);
extern SceUID ScePafMisc_B3B5DF38(void *pObject, const char *pPath, int a2, int a3, int a4);
extern int SceMtpClient_0FF15EF8(void *pClient, int a1, void *pOut, int *pResult);
extern int SceMtpClient_1EA892F7(void *pClient, int a1, void *pOut, int a3);
extern int SceMtpClient_8313A28C(void);
extern void *SceMtpClient_90A60690(void);
extern int SceMtpClient_9920926A(void *pClient, int a1, void *pA, int size, int *pOut);
extern int SceMtpClient_99E4CEED(void *pClient);
extern int SceMtpClient_BFB9E02B(void *pClient, void *pName, void *pBuf, int size, int *pOut);
extern int sceHttpUriEscape(char *pOut, SceSize *pRequire, SceSize prepare, const char *pIn);
extern int sceHttpsGetSslError(int id, int *pErrNum, SceUInt32 *pDetail);
extern int sceHttpsEnableOptionPrivate(int id, int option);
extern int sceHttpsDisableOptionPrivate(int id, int option);
extern int _sceNpDrmCheckActData(int *pActType, int *pVersionFlag, SceUInt64 *pAccountId, SceUInt64 *pExpTime);
extern int _sceNpDrmRemoveActData(int a0);
extern int _sceNpDrmGetRifInfo(int a0, int a1, int a2, int *pInfo);
extern int _sceNpDrmGetRifName(char *pRifName, SceUInt64 aid);
extern int _sceNpDrmGetFixedRifName(char *pRifName, SceUInt64 aid);
extern int _sceNpDrmGetRifNameForInstall(char *pRifName, const void *pRif, int a2);
extern int _sceNpDrmCheckDrmReset(int a0, int a1, int *pInfo);
extern int _sceNpDrmPresetRifProvisionalFlag(void);

#endif /* _NP_KDC_INTERNAL_H */
