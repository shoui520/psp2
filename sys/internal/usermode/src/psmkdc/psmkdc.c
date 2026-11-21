/*
 * SceDrmPsmKdc (vs0:sys/external/psmkdc.suprx): PlayStation Mobile's key delivery client. It
 * activates the console for PSM with Sony's KDS server (a multipart HTTPS post of the PSN
 * ticket and the secure kernel's C1, answered by an R1 the secure kernel checks), fetches a
 * content's license, says whether an application may boot (the activation's and the license's
 * times against the secure clock), and keeps the signed revocation list (ur0:/psm/rvk.dat),
 * whose contents it marks revoked in their application data.
 *
 * Built by Sony with SNC; its request code is np_kdc's (SceNpKdc). No debug record names its
 * files or functions; the PSM application of 1.80 (NPXS10074) names three exports in its
 * messages: scePsmDrmKdcInit, scePsmDrmIsActivated and scePsmDrmIsBootable. The other names are
 * ours, taken from what the code does.
 */

#include <stddef.h>
#include <libkernel/libkernel.h>
#include <kernel/error.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libhttp/https.h>
#include <libhttp/libhttp_url.h>
#include <libmt19937.h>
#include <libsha256.h>
#include <sceerror.h>
#include <gcauthmgr/rsa_signature.h>
#include "psmkdc_internal.h"

#define KDC_USER_AGENT		"PSM KDC for VITA"
#define KDC_BOUNDARY_DASHES	"------------------------------"
#define KDC_BOUNDARY_DEFAULT	"70a7ab695037"
#define KDC_CRLF		"\r\n"
#define KDC_RVK_LOCK		"ur0:/psm/rvk.dat"
#define KDC_RVK_DIRECTORY	"ur0:/psm"
#define KDC_PSM_DIRECTORY	"ux0:/psm"

#define KDC_TICKET_PART		"Content-Disposition: form-data; name=\"ticket\";\r\n" \
				"Content-Type: application/octet-stream\r\n"
#define KDC_C1_PART		"Content-Disposition: form-data; name=\"c1\";\r\n" \
				"Content-Type: application/octet-stream\r\n"
#define KDC_CONTENT_ID_PART	"Content-Disposition: form-data; name=\"contentid\";\r\n" \
				"Content-Type: application/octet-stream\r\n"

/* the sizes of the buffers the activation calls share */
#define KDC_C1_SIZE		1024
#define KDC_RESPONSE_SIZE	1584
#define KDC_R1_OFFSET		512
#define KDC_RIF_SIZE		1024

static int KdcSendPart(KdcRequest *pRequest, const char *pSeparator, const char *pHeader,
		       const void *pData, SceSize size);
static int KdcSendRequest(KdcRequest *pRequest, const void *pTicket, SceSize ticketSize,
			  const void *pC1, SceSize c1Size, const void *pContentId,
			  SceSize contentIdSize, const char *pUrl);
static int KdcCreateTemplate(SceUInt32 connectTimeout, SceUInt32 sendTimeout,
			     SceUInt32 recvTimeout);
static int KdcDeleteTemplate(void);
static int KdcCreateRequest(KdcRequest *volatile *ppRequest, const char *pUrl);
static int KdcDeleteRequest(KdcRequest *pRequest);
static int KdcSend(KdcRequest *pRequest, const void *pTicket, SceSize ticketSize,
		   const void *pC1, SceSize c1Size, const void *pContentId, SceSize contentIdSize,
		   const char *pUrl);
static int KdcGetContentLength(KdcRequest *pRequest, SceUInt64 *pLength);
static int KdcGetConsoleInfo(KdcRequest *pRequest, SceUInt32 *pMax, SceUInt32 *pAvailable);
static int KdcReadData(KdcRequest *pRequest, char *pData, SceSize size, SceSize *pRead);
static int KdcAbort(KdcRequest *pRequest);
static int KdcActivate(const void *pTicket, SceSize ticketSize, SceUInt32 *pMax,
		       SceUInt32 *pAvailable, int ignoreError);
static int KdcConvertReason(int reason);
static int KdcConvertDrmError(int error);
static int KdcConvertVerifyError(int error);
static int KdcConvertC1Error(int error);
static int KdcPost(const char *pHost, const char *pPath, const KdcPostParam *pParam,
		   const KdcPostResult *pResult);
static int KdcPostToServer(const char *pPath, const KdcPostParam *pParam,
			   const KdcPostResult *pResult);
static int KdcAbortPost(void);
static int KdcMutexCreate(const char *pName, SceUID *pId);
static int KdcMutexLock(SceUID *pId);
static int KdcMutexUnlock(SceUID *pId);
static int KdcMutexDelete(SceUID *pId);
static int KdcRemoveFiles(char *pPath, SceSize size, SceSize length);
static int KdcRevokeContents(const char *pPsmPath, const KdcRevocationEntry *pEntry,
			     SceSize size);
static void KdcSwapRevocationHeader(KdcRevocationHeader *pHeader);
static int KdcCheckRevocationHeader(const KdcRevocationHeader *pHeader);
static int KdcDownloadRevocationList(const char *pHost, const char *pPath, const char *pFile,
				     void *pBuffer, SceSize size, SceUInt32 version);
static int KdcUpdateRevocationList(int unused, const char *pFile, void *pBuffer, SceSize size,
				   SceUInt32 version);
static int KdcAbortDownload(void);
static int KdcVerifyRevocationList(SceUChar8 *pDigest, SceUInt8 *pSignature);
static int KdcRevocationInit(SceSize size, void *pBuffer, int unused);
static int KdcReadRevocationList(SceUInt32 *pVersion, const char *pContentId, int *pFound);
static int KdcApplyRevocationList(const char *pPsmPath, const char *pFile);
static int KdcRefreshRevocationList(const char *pPsmPath);
static int KdcCheckRevocationList(const char *pPsmPath);
static int KdcIsRevoked(const char *pContentId, int *pRevoked);
static int KdcStateInit(void);
static int KdcStateTerm(void);
static int KdcStateBegin(int state);
static int KdcStateGet(void);
static int KdcStateEnd(void);
static int KdcSetAborted(int aborted);
static int KdcIsAborted(void);
static int KdcStateLock(void);
static int KdcStateUnlock(void);
static int KdcReadFile(const char *pPath, void *pBuffer, SceSize size);
static int KdcGetTitleId(const KdcRevocationEntry *pEntry, char *pTitleId);
static int KdcIsFile(const char *pPath);
static int KdcIsDirectory(const char *pPath);
static int KdcGetLicensePath(const char *pAppPath, const char *pName, char *pPath, SceSize size);
static int KdcGetLicenseDirectory(const char *pPsmPath, const KdcRevocationEntry *pEntry,
				  char *pPath, SceSize size);
static int KdcGetRevokedPath(const char *pPsmPath, const KdcRevocationEntry *pEntry,
			     char *pPath, SceSize size);
static int KdcGetSystemRevokedPath(const char *pDataPath, char *pPath, SceSize size);
static int KdcPlatformInit(int unused);
static int KdcGetNpEnvironment(char *pEnvironment);
static int KdcPlatformTerm(void);
static int KdcGetCurrentSecureTick(SceUInt64 *pTick);
static int KdcStrtol(const char *pString, char **ppEnd, int base);

int g_httpReady = 0;
int g_httpTemplate = 0;
int g_initialized = 0;
KdcRequest g_postRequestData = { 0 };
KdcRequest *g_pPostRequest = NULL;
KdcRequest g_downloadRequestData = { 0 };
KdcRequest *volatile g_pDownloadRequest = NULL;
void *g_pRevocationBuffer = NULL;	/* where the revocation list is read through */
SceSize g_revocationBufferSize = 0;	/* a multiple of the entries' size */
int g_state = 0;			/* KDC_STATE_* */
int g_aborted = 0;
int g_reserved = 0;
SceUInt8 g_c1Response[64];		/* the secure kernel's context of the C1 sent */
char g_response[KDC_RESPONSE_SIZE];	/* the server's answer: the R1, then what it carries */
char g_psmPath[128];			/* where the PSM applications' data are (ux0:/psm) */
ScePsmDrmLicense g_license;
SceUInt8 g_c1[KDC_C1_SIZE];
char g_url[128];
char g_revocationTmpPath[256];
char g_revocationPath[256];
SceUID g_stateMutex;
char g_body[0x400];

/* the headers the server is sent, and the one it must answer with */
static const KdcHeaderField s_requestHeaders[1] = {
	{ "X-PSM-Version", "1.0" },
};

static const KdcHeaderField s_versionHeader[1] = {
	{ "X-PSM-Version", "1.0" },
};

/* the server's reasons for a refusal */
static const KdcReason s_reasons[] = {
	{ 0x0001, (int)0x808F0480 },
	{ 0x0002, (int)0x808F0481 },
	{ 0x0003, (int)0x808F0482 },
	{ 0x0004, (int)0x808F0483 },
	{ 0x0005, (int)0x808F0484 },
	{ 0x0007, (int)0x808F0485 },
	{ 0x000A, (int)0x808F0486 },
	{ 0x0011, (int)0x808F0487 },
	{ 0x0064, (int)0x808F0488 },
	{ 0x1000, (int)0x808F0489 },
	{ 0x2000, (int)0x808F048A },
	{ 0x2001, (int)0x808F048B },
	{ 0x2100, (int)0x808F048C },
	{ 0x2101, (int)0x808F048D },
	{ 0x2102, (int)0x808F048E },
	{ 0x2200, (int)0x808F048F },
	{ 0x2300, (int)0x808F0490 },
	{ 0x2301, (int)0x808F0491 },
	{ 0x2302, (int)0x808F0492 },
	{ 0x2303, (int)0x808F0493 },
	{ 0xFFFF, PSM_KDC_ERROR_UNKNOWN_REASON },
};

/* the key the revocation list is signed with: RSA-2048, its words least significant first */
static const unsigned int s_revocationModulus[64] = {
	0x66fc2341, 0xe0d9d5da, 0x253e684b, 0xff166397,
	0xf55628fa, 0xde8b948f, 0x3b35b8bd, 0x3070d71c,
	0xb4f6dae4, 0xf86adf4a, 0x3539b47d, 0xebeefb6b,
	0x7c313fef, 0x19cfcf3d, 0xb164de03, 0xa50d4cc2,
	0x99ec2e5b, 0x1247baa0, 0x73bd4c4f, 0x2ff54f9b,
	0x440e4b44, 0xec1e1007, 0x60d8a162, 0x44d61e4e,
	0xf386f712, 0x0aa540b6, 0xf4ac516b, 0xa1b693da,
	0xe2e4ac26, 0xe075295b, 0x1bddf475, 0x4bb00809,
	0xfaceabfe, 0xdfb974b2, 0xf2680bda, 0x078332ab,
	0x1d2f6080, 0xea297dcd, 0xc7db8f60, 0x86830539,
	0x4740852f, 0x2beaa2d1, 0x45b4d70b, 0x3b80a52c,
	0x630d216c, 0x3fb098e3, 0xed571597, 0x707e8a86,
	0x97a38a4d, 0x1e604528, 0x19dbbb9a, 0x5ff1f323,
	0xcb9aa9e8, 0x1819c90f, 0xee1caa7a, 0xd2ddc3e9,
	0x7adbd8be, 0xfadcbc74, 0x89ef15b2, 0xe24bce88,
	0x36ace369, 0x38b72bec, 0xfcafa98c, 0xb9a48a54,
};

static const unsigned int s_revocationExponent[64] = {
	0x00010001,
};

/* the request code: np_kdc's, its body a multipart form */

/* one part of the body: the separator, the part's header, the data */
__attribute__((noinline))
static int KdcSendPart(KdcRequest *pRequest, const char *pSeparator, const char *pHeader,
		       const void *pData, SceSize size)
{
	char head[256];
	SceSize length;
	SceSize crlf;
	SceSize headerLength;
	int ret;

	length = sceClibStrnlen(pSeparator, 0x7FFFFFFF);
	sceClibMemcpy_safe(head, pSeparator, length);
	crlf = sceClibStrnlen(KDC_CRLF, 0x7FFFFFFF);
	sceClibMemcpy_safe(head + length, KDC_CRLF, crlf);
	length += crlf;
	headerLength = sceClibStrnlen(pHeader, 0x7FFFFFFF);
	sceClibMemcpy_safe(head + length, pHeader, headerLength);
	length += headerLength;
	crlf = sceClibStrnlen(KDC_CRLF, 0x7FFFFFFF);
	sceClibMemcpy_safe(head + length, KDC_CRLF, crlf);
	head[crlf + length] = 0;
	ret = sceHttpSendRequest(pRequest->requestId, head, crlf + length);
	if (ret == 0) {
		ret = sceHttpSendRequest(pRequest->requestId, pData, size);
		if (ret == 0) {
			ret = sceHttpSendRequest(pRequest->requestId, KDC_CRLF,
						 sceClibStrnlen(KDC_CRLF, 0x7FFFFFFF));
			if (ret == 0)
				ret = 0;
		}
	}
	return ret;
}

/* a header field of the answer the server must send */
static inline __attribute__((always_inline)) int KdcCheckHeader(const char *pHeader,
		SceSize headerSize, const KdcHeaderField *pFields)
{
	const char *pValue;
	SceSize valueSize;
	int ret;
	int i;

	for (i = 0; i < 1; i++) {
		ret = sceHttpParseResponseHeader(pHeader, headerSize, pFields[i].pName, &pValue,
						 &valueSize);
		if (ret < 0)
			return ret;
		if (sceClibStrncmp(pValue, pFields[i].pValue,
				   sceClibStrnlen(pFields[i].pValue, 0x7FFFFFFF)) != 0)
			return PSM_KDC_ERROR_BAD_RESPONSE;
	}
	return 0;
}

/* a number from a field of the answer, up to a ';' or the line's end */
static inline __attribute__((always_inline)) const char *KdcFieldEnd(const char *pValue,
		const char *pEnd)
{
	const char *pSemicolon = sceClibStrchr(pValue, ';');

	if (pSemicolon == NULL || pSemicolon >= pEnd)
		pSemicolon = pEnd;
	return pSemicolon;
}

/*
 * Posts the ticket, the C1 and the content ID as the parts of a multipart form, and reads the
 * server's status: its reason when it refuses, the consoles the activation counts.
 */
__attribute__((noinline))
static int KdcSendRequest(KdcRequest *pRequest, const void *pTicket, SceSize ticketSize,
			  const void *pC1, SceSize c1Size, const void *pContentId,
			  SceSize contentIdSize, const char *pUrl)
{
	char tail[256];
	char reasonText[16];
	char availableText[16];
	char maxText[16];
	int statusCode;
	const char *pStatus;
	char *pHeader;
	SceSize headerSize;
	SceSize statusSize;
	char *pUnused;
	SceSize unusedSize;
	char *pDebugHeader;
	SceSize debugHeaderSize;
	char *pSentHeader;
	SceSize sentHeaderSize;
	SceSize require;
	SceUInt64 contentLength;
	SceUInt64 tick;
	SceMt19937Context mt;
	char boundary[16];
	char separator[64];
	char contentType[128];
	const KdcParam *pParam;
	SceSize total;
	SceSize bodySize = 0;
	SceSize separatorLength;
	SceSize crlf;
	SceSize length;
	int ret;
	int i;

	if (pRequest->pBody != NULL)
		return KDC_ERROR_BUSY;

	/* the form's fields (none: the parts carry the request) */
	total = 0;
	ret = 0;
	for (pParam = pRequest->params.pHead; pParam != NULL; pParam = pParam->pNext) {
		SceSize used;

		require = pParam->pDesc->length;
		used = total + require;
		ret = sceHttpUriEscape(NULL, &require, 0, pParam->value);
		total = used + --require;
		if (ret != 0)
			break;
	}
	if (ret == 0)
		bodySize = total + 1;
	if (ret < 0)
		return ret;
	pRequest->bodySize = (bodySize > 0x400) ? bodySize : 0x400;
	pRequest->pBody = g_body;
	sceClibMemset(g_body, 0, 0x400);

	/* the parts' boundary: random from the secure clock */
	if (KdcGetCurrentSecureTick(&tick) != 0) {
		sceClibStrncpy(boundary, KDC_BOUNDARY_DEFAULT, 12);
	} else {
		char *p = boundary;

		sceMt19937Init(&mt, (SceUInt32)tick);
		sceClibMemset(boundary, 0, 13);
		for (i = 0; i < 3; i++) {
			if (sceClibSnprintf(p, 5, "%04x", sceMt19937UInt(&mt)) < 0) {
				sceClibStrncpy(boundary, KDC_BOUNDARY_DEFAULT, 12);
				break;
			}
			p += 4;
		}
		boundary[12] = 0;
	}
	sceClibMemset(separator, 0, sizeof(separator));
	sceClibSnprintf(separator, sizeof(separator), "--%s%s", KDC_BOUNDARY_DASHES, boundary);

	separatorLength = sceClibStrnlen(separator, 0x7FFFFFFF);
	length = 0;
	crlf = sceClibStrnlen(KDC_CRLF, 0x7FFFFFFF);
	if (ticketSize != 0)
		length = sceClibStrnlen(KDC_TICKET_PART, 0x7FFFFFFF) + separatorLength + ticketSize +
			 3 * crlf;
	if (c1Size != 0)
		length += sceClibStrnlen(KDC_C1_PART, 0x7FFFFFFF) + separatorLength + c1Size +
			  3 * crlf;
	if (contentIdSize != 0)
		length += sceClibStrnlen(KDC_CONTENT_ID_PART, 0x7FFFFFFF) + separatorLength +
			  contentIdSize + 3 * crlf;
	length += sceClibStrnlen("--", 0x7FFFFFFF) + separatorLength + crlf;

	ret = sceHttpCreateRequestWithURL(pRequest->connectionId, SCE_HTTP_METHOD_POST, pUrl,
					  (SceULong64)length);
	if (ret < 0)
		return ret;
	pRequest->requestId = ret;
	ret = sceHttpAddRequestHeader(ret, "User-Agent", KDC_USER_AGENT, SCE_HTTP_HEADER_OVERWRITE);
	if (ret < 0)
		return ret;
	for (i = 0; i < 1; i++) {
		ret = sceHttpAddRequestHeader(pRequest->requestId, s_requestHeaders[i].pName,
					      s_requestHeaders[i].pValue, SCE_HTTP_HEADER_OVERWRITE);
		if (ret < 0)
			return ret;
	}
	sceClibMemset(contentType, 0, sizeof(contentType));
	sceClibSnprintf(contentType, sizeof(contentType), "multipart/form-data; boundary=%s%s",
			KDC_BOUNDARY_DASHES, boundary);
	sceHttpAddRequestHeader(pRequest->requestId, "Content-type", contentType,
				SCE_HTTP_HEADER_OVERWRITE);
	sceHttpGetAllResponseHeaders(pRequest->requestId, &pSentHeader, &sentHeaderSize);

	ret = KdcSendPart(pRequest, separator, KDC_TICKET_PART, pTicket, ticketSize);
	if (ret != 0)
		return ret;
	ret = KdcSendPart(pRequest, separator, KDC_C1_PART, pC1, c1Size);
	if (ret != 0)
		return ret;
	if (contentIdSize != 0) {
		ret = KdcSendPart(pRequest, separator, KDC_CONTENT_ID_PART, pContentId,
				  contentIdSize);
		if (ret != 0)
			return ret;
	}
	sceClibSnprintf(tail, sizeof(tail), "%s--\r\n", separator);
	ret = sceHttpSendRequest(pRequest->requestId, tail, sceClibStrnlen(tail, 0x7FFFFFFF));
	if (ret != 0)
		return ret;

	sceHttpGetAllResponseHeaders(pRequest->requestId, &pDebugHeader, &debugHeaderSize);
	ret = sceHttpGetStatusCode(pRequest->requestId, &statusCode);
	if (ret < 0)
		return ret;
	if (statusCode != 200)
		return PSM_KDC_ERROR_BAD_STATUS_CODE;
	ret = sceHttpGetAllResponseHeaders(pRequest->requestId, &pHeader, &headerSize);
	if (ret < 0)
		return ret;
	sceHttpGetAllResponseHeaders(pRequest->requestId, &pUnused, &unusedSize);
	ret = KdcCheckHeader(pHeader, headerSize, s_versionHeader);
	if (ret < 0)
		return ret;

	/* the status: OK, or the reason of a refusal */
	if (sceHttpParseResponseHeader(pHeader, headerSize, "X-PSM-Status", &pStatus,
				       &statusSize) < 0)
		return KDC_ERROR_BAD_RESPONSE;
	{
		const char *pEnd = sceClibStrstr(pStatus, KDC_CRLF);

		if (sceClibStrncmp("OK", pStatus, 2) == 0) {
			pRequest->refused = 0;
			ret = 0;
		} else {
			const char *pValue;

			pRequest->refused = 1;
			pValue = sceClibStrstr(pStatus, "reason=");
			ret = PSM_KDC_ERROR_BAD_RESPONSE;
			if (pValue != NULL) {
				pValue += 7;
				length = KdcFieldEnd(pValue, pEnd) - pValue;
				if (length <= 15) {
					sceClibMemcpy_safe(reasonText, pValue, length);
					reasonText[length] = 0;
					pRequest->reason = KdcStrtol(reasonText, NULL, 16);
					if (pRequest->reason != (int)0x80000000 &&
					    pRequest->reason != 0x7FFFFFFF)
						ret = 0;
				}
			}
		}
	}
	if (ret < 0)
		return ret;

	/* an activation's answer counts the consoles */
	if (pContentId == NULL &&
	    (pRequest->refused == 0 || pRequest->reason == KDC_REASON_CONSOLE_LIMIT)) {
		const char *pActivation;
		const char *pEnd;
		const char *pValue;
		SceUInt32 value;

		if (sceHttpParseResponseHeader(pHeader, headerSize, "X-PSM-Activation",
					       &pActivation, &statusSize) < 0)
			return PSM_KDC_ERROR_BAD_RESPONSE;
		pEnd = sceClibStrstr(pActivation, KDC_CRLF);
		pValue = sceClibStrstr(pActivation, "max=");
		if (pValue != NULL) {
			pValue += 4;
			length = KdcFieldEnd(pValue, pEnd) - pValue;
			if (length <= 15) {
				sceClibMemcpy_safe(maxText, pValue, length);
				maxText[length] = 0;
				value = KdcStrtol(maxText, NULL, 10);
				pRequest->maxConsole = value;
				if (value != 0x80000000 && value <= 0x7FFFFFFE) {
					pValue = sceClibStrstr(pActivation, "available=");
					if (pValue != NULL) {
						pValue += 10;
						length = KdcFieldEnd(pValue, pEnd) - pValue;
						if (length <= 15) {
							sceClibMemcpy_safe(availableText, pValue,
									   length);
							availableText[length] = 0;
							value = KdcStrtol(availableText, NULL, 10);
							pRequest->availableConsole = value;
							if (value != 0x80000000 &&
							    value <= 0x7FFFFFFE)
								pRequest->flags |=
									KDC_FLAG_CONSOLE_INFO;
						}
					}
				}
			}
		}
	}

	if (pRequest->refused != 0)
		return pRequest->reason | KDC_ERROR_SERVER_BASE;
	ret = sceHttpGetResponseContentLength(pRequest->requestId, &contentLength);
	if (ret >= 0)
		ret = 0;
	return ret;
}

__attribute__((noinline))
static int KdcCreateTemplate(SceUInt32 connectTimeout, SceUInt32 sendTimeout,
			     SceUInt32 recvTimeout)
{
	int ret;
	int err;

	sceHttpInit(0x4E20);
	ret = sceHttpCreateTemplate(KDC_USER_AGENT, SCE_HTTP_VERSION_1_1, SCE_FALSE);
	g_httpTemplate = ret;
	if (ret < 0)
		return ret;
	err = sceHttpSetConnectTimeOut(ret, connectTimeout);
	if (err < 0
	    || (err = sceHttpSetSendTimeOut(g_httpTemplate, sendTimeout)) < 0
	    || (err = sceHttpSetRecvTimeOut(g_httpTemplate, recvTimeout)) < 0) {
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

/* the request in *ppRequest, connected to the server of pUrl */
__attribute__((noinline))
static int KdcCreateRequest(KdcRequest *volatile *ppRequest, const char *pUrl)
{
	KdcRequest *pRequest = *ppRequest;
	int ret;

	sceClibMemset(pRequest, 0, sizeof(KdcRequest));
	pRequest->params.pHead = NULL;
	pRequest->params.pTail = NULL;
	pRequest->params.mask = 0;
	ret = sceHttpCreateConnectionWithURL(g_httpTemplate, pUrl, SCE_TRUE);
	if (ret >= 0) {
		pRequest->connectionId = ret;
		pRequest->pBody = NULL;
		return 0;
	}
	return ret;
}

__attribute__((noinline))
static int KdcDeleteRequest(KdcRequest *pRequest)
{
	if (pRequest->requestId > 0) {
		sceHttpDeleteRequest(pRequest->requestId);
		pRequest->requestId = 0;
	}
	sceHttpDeleteConnection(pRequest->connectionId);
	return 0;
}

__attribute__((noinline))
static int KdcSend(KdcRequest *pRequest, const void *pTicket, SceSize ticketSize,
		   const void *pC1, SceSize c1Size, const void *pContentId, SceSize contentIdSize,
		   const char *pUrl)
{
	return KdcSendRequest(pRequest, pTicket, ticketSize, pC1, c1Size, pContentId,
			      contentIdSize, pUrl);
}

__attribute__((noinline))
static int KdcGetContentLength(KdcRequest *pRequest, SceUInt64 *pLength)
{
	return sceHttpGetResponseContentLength(pRequest->requestId, pLength);
}

__attribute__((noinline))
static int KdcGetConsoleInfo(KdcRequest *pRequest, SceUInt32 *pMax, SceUInt32 *pAvailable)
{
	int ret = KDC_ERROR_NO_CONSOLE_INFO;

	if ((pRequest->flags & KDC_FLAG_CONSOLE_INFO) != 0) {
		ret = 0;
		*pMax = pRequest->maxConsole;
		*pAvailable = pRequest->availableConsole;
	}
	return ret;
}

/* SceHttp's SSL failure as the certificate's problem */
static inline __attribute__((always_inline)) int KdcConvertHttpResult(int requestId, int result)
{
	SceUInt32 detail;
	int errNum;
	int ret;

	if (result != KDC_HTTP_ERROR_SSL)
		return result;
	ret = sceHttpsGetSslError(requestId, &errNum, &detail);
	if (ret < 0)
		return ret;
	ret = errNum;
	if (errNum == KDC_SSL_ERROR_CERT) {
		ret = KDC_ERROR_SSL_UNTRUSTED;
		if ((detail & 4) == 0) {
			ret = KDC_ERROR_SSL_EXPIRED;
			if ((detail & 0x20) == 0) {
				ret = KDC_ERROR_SSL_NAME;
				if ((detail & 2) == 0) {
					ret = KDC_ERROR_SSL_BAD_CERT;
					if ((detail & 1) != 0)
						ret = KDC_ERROR_SSL_UNKNOWN_CA;
				}
			}
		}
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
	for (;;) {
		n = sceHttpReadData(pRequest->requestId, p, size);
		if (n < 0) {
			ret = KdcConvertHttpResult(pRequest->requestId, n);
			break;
		}
		size -= n;
		p += n;
		if (size == 0 || n == 0) {
			ret = 0;
			break;
		}
	}
	*pRead = p - pData;
	return ret;
}

__attribute__((noinline))
static int KdcAbort(KdcRequest *pRequest)
{
	return sceHttpAbortRequest(pRequest->requestId);
}

int module_start(SceSize args, const void *argp)
{
	return 0;
}

int module_stop(SceSize args, const void *argp)
{
	return 0;
}

/* the exports */

/* scePsmDrmKdcInit: the heap's size (32 KiB at least, the revocation list read through it),
 * the heap, and where the PSM applications' data are */
int scePsmDrmKdcInit(SceSize size, void *pBuffer, const char *pPsmPath)
{
	if (size < 0x8000 || pBuffer == NULL || pPsmPath == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (sceClibStrnlen(pPsmPath, 0x80) > 0x7F)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized != 0)
		return PSM_KDC_ERROR_ALREADY_INITIALIZED;
	KdcRevocationInit(size, pBuffer, 0);
	if (KdcPlatformInit(0) != 0)
		return PSM_KDC_ERROR_INTERNAL;
	if (KdcStateInit() != 0)
		return PSM_KDC_ERROR_INTERNAL;
	if (KdcStateEnd() != 0)
		return PSM_KDC_ERROR_INTERNAL;
	sceClibStrncpy(g_psmPath, pPsmPath, 0x7F);
	g_initialized = 1;
	return 0;
}

/* the end */
int SceDrmPsmKdc_7EE0F1BC(void)
{
	int ret;

	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_LOCAL);
	if (ret != 0)
		return ret;
	KdcPlatformTerm();
	g_initialized = 0;
	KdcStateTerm();
	return 0;
}

/* asks the server whether the console can be activated: the consoles' maximum and how many
 * are left */
int SceDrmPsmKdc_FB75B314(const void *pTicket, SceSize ticketSize, SceUInt32 *pMax,
			  SceUInt32 *pAvailable)
{
	KdcPostResult result;
	KdcPostParam param;
	int ret;

	if (pTicket == NULL || ticketSize == 0 || pMax == NULL || pAvailable == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_RUNNING);
	if (ret != 0)
		return ret;
	ret = SceLibKernel_622A81E6(1, g_c1, g_c1Response);
	if (ret != 0) {
		ret = KdcConvertC1Error(ret);
	} else {
		sceClibMemset(&param, 0, sizeof(param));
		param.pTicket = pTicket;
		param.ticketSize = ticketSize;
		param.pC1 = g_c1;
		param.c1Size = KDC_C1_SIZE;
		param.pContentId = NULL;
		param.contentIdSize = 0;
		sceClibMemset(&result, 0, sizeof(result));
		result.pMaxConsole = pMax;
		result.pAvailableConsole = pAvailable;
		result.wantConsoleInfo = 1;
		result.pBody = NULL;
		result.bodySize = 0;
		ret = KdcPostToServer("/kds/check_activation", &param, &result);
	}
	KdcStateEnd();
	return ret;
}

/* deactivates the console: the activation removed, then the server told (its answer
 * ignored) */
int SceDrmPsmKdc_A4B7788D(const void *pTicket, SceSize ticketSize, SceUInt32 *pMax,
			  SceUInt32 *pAvailable)
{
	KdcPostResult result;
	KdcPostParam param;
	SceUInt64 accountId;
	int ret;

	if (pTicket == NULL || ticketSize == 0 || pMax == NULL || pAvailable == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_RUNNING);
	if (ret != 0)
		return ret;
	ret = KdcRemoveActData(&accountId);
	if (ret == 0 || ret == PSM_DRM_ERROR_NOT_ACTIVATED) {
		ret = SceLibKernel_622A81E6(3, g_c1, g_c1Response);
		if (ret != 0) {
			ret = KdcConvertC1Error(ret);
		} else {
			sceClibMemset(&param, 0, sizeof(param));
			param.pTicket = pTicket;
			param.ticketSize = ticketSize;
			param.pC1 = g_c1;
			param.c1Size = KDC_C1_SIZE;
			param.pContentId = NULL;
			param.contentIdSize = 0;
			sceClibMemset(&result, 0, sizeof(result));
			result.pMaxConsole = pMax;
			result.pAvailableConsole = pAvailable;
			result.wantConsoleInfo = 1;
			result.pBody = NULL;
			result.bodySize = 0;
			ret = KdcPostToServer("/kds/exec_deactivation", &param, &result);
			if (ret != 0)
				ret = 0;
		}
	} else {
		ret = KdcConvertDrmError(ret);
	}
	KdcStateEnd();
	return ret;
}

/* the license of the content contentId from the server (1024 bytes into pRif), for an
 * activated console */
int SceDrmPsmKdc_D342757C(const void *pTicket, SceSize ticketSize, const char *pContentId,
			  void *pRif, SceSize size)
{
	KdcPostResult result;
	KdcPostParam param;
	SceUInt64 now;
	SceUInt64 endTime;
	SceUInt64 startTime;
	SceUInt64 accountId;
	int actType;
	int version;
	int ret;

	if (pTicket == NULL || ticketSize == 0 || pContentId == NULL || pRif == NULL ||
	    size != KDC_RIF_SIZE)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (sceClibStrnlen(pContentId, KDC_CONTENT_ID_LENGTH + 1) > KDC_CONTENT_ID_LENGTH)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_RUNNING);
	if (ret != 0)
		return ret;
	if (KdcGetCurrentSecureTick(&now) != 0)
		ret = PSM_KDC_ERROR_NO_SECURE_CLOCK;
	else if ((ret = KdcGetActInfo(&actType, &version, &accountId, &startTime, &endTime)) != 0)
		ret = KdcConvertDrmError(ret);
	else if (now <= endTime)
		ret = 0;
	else
		ret = PSM_KDC_ERROR_ACTIVATION_EXPIRED;
	if (ret != PSM_KDC_ERROR_NO_SECURE_CLOCK) {
		if (ret != 0) {
			ret = PSM_DRM_ERROR_NOT_ACTIVATED;
		} else if ((ret = SceLibKernel_622A81E6(4, g_c1, g_c1Response)) != 0) {
			ret = KdcConvertC1Error(ret);
		} else {
			sceClibMemset(&param, 0, sizeof(param));
			param.pTicket = pTicket;
			param.ticketSize = ticketSize;
			param.pC1 = g_c1;
			param.c1Size = KDC_C1_SIZE;
			param.pContentId = pContentId;
			param.contentIdSize = sceClibStrnlen(pContentId, 0x7FFFFFFF);
			sceClibMemset(&result, 0, sizeof(result));
			result.wantConsoleInfo = 0;
			result.pMaxConsole = NULL;
			result.pAvailableConsole = NULL;
			result.pBody = g_response;
			result.bodySize = KDC_RESPONSE_SIZE;
			ret = KdcPostToServer("/kds/get_rif", &param, &result);
			if (ret == 0) {
				char *pOutput = result.pBody + KDC_R1_OFFSET;

				ret = SceLibKernel_EB6DA895(result.pBody, g_c1Response, pOutput);
				if (ret != 0)
					ret = KdcConvertVerifyError(ret);
				else
					sceClibMemcpy_safe(pRif, pOutput, KDC_RIF_SIZE);
			}
		}
	}
	KdcStateEnd();
	return ret;
}

/*
 * scePsmDrmIsBootable: whether the content may boot for the account: the activation and the
 * license (in pAppPath's License) within their times, the application not revoked (pDataPath's
 * System/revoked); *pExpireTime the end of the shorter, in seconds since 1970.
 */
int scePsmDrmIsBootable(SceUInt64 accountId, const char *pContentId, SceUInt64 *pExpireTime,
			const char *pAppPath, const char *pDataPath)
{
	char licensePath[256];
	SceUInt64 now;
	SceUInt64 actEndTime;
	SceUInt64 rifStartTime;
	SceUInt64 rifEndTime;
	SceUInt64 actAccountId;
	SceUInt64 actStartTime;
	SceUInt64 rifAccountId;
	char rifContentId[48];
	char rifName[48];
	char path[256];
	int actType;
	int version;
	int ret;
	int err;

	if (pContentId == NULL || pExpireTime == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (sceClibStrnlen(pContentId, KDC_CONTENT_ID_LENGTH + 1) > KDC_CONTENT_ID_LENGTH)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (pAppPath == NULL || sceClibStrnlen(pAppPath, 0x80) > 0x7F ||
	    pDataPath == NULL || sceClibStrnlen(pDataPath, 0x80) > 0x7F)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_RUNNING);
	if (ret != 0)
		return ret;
	if (KdcGetCurrentSecureTick(&now) != 0)
		ret = PSM_KDC_ERROR_NO_SECURE_CLOCK;
	else if ((err = KdcGetActInfo(&actType, &version, &actAccountId, &actStartTime,
				      &actEndTime)) != 0)
		ret = KdcConvertDrmError(err);
	else if (accountId != actAccountId)
		ret = PSM_KDC_ERROR_ACCOUNT_MISMATCH;
	else if (now <= actEndTime)
		ret = 0;
	else
		ret = PSM_KDC_ERROR_ACTIVATION_EXPIRED;
	if (ret == 0) {
		if (KdcGetSystemRevokedPath(pDataPath, path, sizeof(path)) != 0) {
			ret = PSM_KDC_ERROR_INTERNAL;
		} else if (KdcIsFile(path)) {
			ret = SCE_PSM_DRM_KDC_ERROR_REVOKED;
		} else {
			sceClibMemset(licensePath, 0, sizeof(licensePath));
			err = KdcGetRifName(rifName, accountId);
			if (err == 0) {
				err = KdcGetLicensePath(pAppPath, rifName, licensePath,
							sizeof(licensePath));
				if (err == 0) {
					err = KdcReadFile(licensePath, &g_license, sizeof(g_license));
					if (err != 0)
						err = PSM_KDC_ERROR_NOT_FOUND;
				}
			}
			ret = err;
			if (err == 0) {
				err = KdcGetRifInfo(&g_license, rifContentId, &rifAccountId,
						    &rifStartTime, &rifEndTime);
				if (err != 0) {
					ret = KdcConvertDrmError(err);
				} else if (now > rifEndTime) {
					ret = PSM_KDC_ERROR_LICENSE_EXPIRED;
				} else if (now + KDC_CLOCK_TOLERANCE < rifStartTime) {
					ret = PSM_KDC_ERROR_LICENSE_NOT_STARTED;
				} else {
					SceUInt64 endTime = rifEndTime;

					if (actEndTime < rifEndTime)
						endTime = actEndTime;
					ret = 0;
					*pExpireTime = (endTime - KDC_UNIX_EPOCH_TICK) / 1000 / 1000;
				}
			}
		}
	}
	KdcStateEnd();
	return ret;
}

/* scePsmDrmIsActivated: whether the console is activated for the account; *pExpireTime the
 * activation's end, in seconds since 1970 */
int scePsmDrmIsActivated(SceUInt64 accountId, SceUInt64 *pExpireTime)
{
	SceUInt64 now;
	SceUInt64 endTime;
	SceUInt64 actAccountId;
	SceUInt64 startTime;
	int actType;
	int version;
	int ret;
	int err;

	if (pExpireTime == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_LOCAL);
	if (ret != 0)
		return ret;
	if (KdcGetCurrentSecureTick(&now) != 0)
		ret = PSM_KDC_ERROR_NO_SECURE_CLOCK;
	else if ((err = KdcGetActInfo(&actType, &version, &actAccountId, &startTime,
				      &endTime)) != 0)
		ret = KdcConvertDrmError(err);
	else if (accountId != actAccountId)
		ret = PSM_KDC_ERROR_ACCOUNT_MISMATCH;
	else if (now <= endTime)
		ret = 0;
	else
		ret = PSM_KDC_ERROR_ACTIVATION_EXPIRED;
	if (ret == 0) {
		*pExpireTime = (endTime - KDC_UNIX_EPOCH_TICK) / 1000 / 1000;
		ret = 0;
	}
	KdcStateEnd();
	return ret;
}

/*
 * The activation's state for the account: 0 valid, 1 ending (less left than five minutes past
 * its start, or expired: 2), 3 none or another account's, 4 not known now. Returns 1 when the
 * activation should be renewed.
 */
int SceDrmPsmKdc_FAFA8C6A(SceUInt64 accountId, int *pState)
{
	SceUInt64 now;
	SceUInt64 actAccountId;
	SceUInt64 startTime;
	SceUInt64 endTime;
	int actType;
	int version;
	int ret;

	if (pState == NULL)
		return 0;
	if (g_initialized == 0) {
		*pState = 4;
		return 0;
	}
	if (KdcStateGet() != KDC_STATE_IDLE || KdcStateBegin(KDC_STATE_LOCAL) != 0) {
		*pState = 4;
		return 0;
	}
	if (KdcGetCurrentSecureTick(&now) != 0) {
		*pState = 4;
		ret = 0;
	} else if (KdcGetActInfo(&actType, &version, &actAccountId, &startTime, &endTime) != 0) {
		*pState = 3;
		ret = 0;
	} else if (accountId != actAccountId) {
		*pState = 3;
		ret = 0;
	} else if (now > endTime) {
		*pState = 2;
		ret = 1;
	} else if (now + KDC_CLOCK_TOLERANCE < startTime) {
		*pState = 3;
		ret = 0;
	} else if (endTime - (now + KDC_CLOCK_TOLERANCE) >= now + KDC_CLOCK_TOLERANCE - startTime) {
		ret = 0;
		*pState = 0;
	} else {
		ret = 1;
		*pState = 1;
	}
	KdcStateEnd();
	return ret;
}

/* the revocation list's lock, shared with the other PSM processes; its ID, or an error */
static inline __attribute__((always_inline)) SceUID KdcLockRevocationList(void)
{
	SceUInt32 timeout = 30000000;
	SceUID id;
	int ret;

	id = sceKernelCreateMutex(KDC_RVK_LOCK, SCE_KERNEL_ATTR_OPENABLE, 0, NULL);
	if (id == SCE_KERNEL_ERROR_DUPLICATE_NAME)
		id = sceKernelOpenMutex(KDC_RVK_LOCK);
	if (id < 0)
		return PSM_KDC_ERROR_INTERNAL;
	ret = sceKernelLockMutex(id, 1, &timeout);
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return PSM_KDC_ERROR_BUSY;
	if (ret < 0) {
		sceKernelDeleteMutex(id);
		return ret;
	}
	return id;
}

/* whether the content contentId is revoked: 1 also when it cannot be told */
int SceDrmPsmKdc_4625BEB8(const char *pContentId)
{
	int revoked;
	SceUID lock;

	if (pContentId == NULL ||
	    (revoked = 1, sceClibStrnlen(pContentId, KDC_CONTENT_ID_LENGTH + 1) >
	     KDC_CONTENT_ID_LENGTH) ||
	    g_initialized == 0 || KdcStateGet() != KDC_STATE_IDLE ||
	    KdcStateBegin(KDC_STATE_RUNNING) != 0)
		return 1;
	lock = KdcLockRevocationList();
	if (lock < 0) {
		revoked = 1;
	} else {
		if (KdcIsRevoked(pContentId, &revoked) != 0)
			revoked = 1;
		sceKernelUnlockMutex(lock, 1);
		sceKernelDeleteMutex(lock);
	}
	KdcStateEnd();
	return revoked;
}

/* the license file's name for the account */
int SceDrmPsmKdc_C2889519(char *pName, SceUInt64 accountId)
{
	int ret;

	if (pName == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_LOCAL);
	if (ret != 0)
		return ret;
	ret = KdcGetRifName(pName, accountId);
	KdcStateEnd();
	return ret;
}

/* stops the transfer running */
int SceDrmPsmKdc_5380412F(void)
{
	int state;
	int ret;

	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateLock() != 0)
		return PSM_KDC_ERROR_INTERNAL;
	state = KdcStateGet();
	if (state == KDC_STATE_IDLE || state == KDC_STATE_LOCAL) {
		ret = PSM_KDC_ERROR_NOTHING_TO_ABORT;
	} else {
		ret = KdcSetAborted(1);
		if ((SceUInt32)state > KDC_STATE_RUNNING) {
			if ((SceUInt32)state <= KDC_STATE_POSTING)
				ret = KdcAbortPost();
			else if ((SceUInt32)state <= KDC_STATE_DOWNLOADING)
				ret = KdcAbortDownload();
			else
				ret = PSM_KDC_ERROR_INTERNAL;
		} else if (state != KDC_STATE_IDLE) {
			ret = 0;
		} else {
			ret = PSM_KDC_ERROR_INTERNAL;
		}
	}
	KdcStateUnlock();
	return ret;
}

/* applies the revocation list to the contents installed */
int SceDrmPsmKdc_2C124A9B(void)
{
	SceUID lock;
	int ret;

	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_LOCAL);
	if (ret != 0)
		return ret;
	lock = KdcLockRevocationList();
	if (lock >= 0) {
		ret = KdcCheckRevocationList(g_psmPath);
		sceKernelUnlockMutex(lock, 1);
		sceKernelDeleteMutex(lock);
	} else {
		ret = lock;
	}
	KdcStateEnd();
	return ret;
}

/*
 * Activates the console: the server sent the C1 and answers with an R1, which the secure kernel
 * checks; the revocation list is then brought up to date. An error of the post is ignored when
 * ignoreError says so.
 */
__attribute__((noinline))
static int KdcActivate(const void *pTicket, SceSize ticketSize, SceUInt32 *pMax,
		       SceUInt32 *pAvailable, int ignoreError)
{
	KdcPostResult result;
	KdcPostParam param;
	SceUID lock;
	int ret;

	if (pTicket == NULL || ticketSize == 0 || pMax == NULL || pAvailable == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	if (g_initialized == 0)
		return PSM_KDC_ERROR_NOT_INITIALIZED;
	if (KdcStateGet() != KDC_STATE_IDLE)
		return PSM_KDC_ERROR_BUSY;
	ret = KdcStateBegin(KDC_STATE_RUNNING);
	if (ret != 0)
		return ret;
	ret = SceLibKernel_622A81E6(2, g_c1, g_c1Response);
	if (ret != 0) {
		ret = KdcConvertC1Error(ret);
	} else {
		sceClibMemset(&param, 0, sizeof(param));
		param.pTicket = pTicket;
		param.ticketSize = ticketSize;
		param.pC1 = g_c1;
		param.c1Size = KDC_C1_SIZE;
		param.pContentId = NULL;
		param.contentIdSize = 0;
		sceClibMemset(&result, 0, sizeof(result));
		result.pMaxConsole = pMax;
		result.pAvailableConsole = pAvailable;
		result.wantConsoleInfo = 1;
		result.pBody = g_response;
		result.bodySize = KDC_RESPONSE_SIZE;
		ret = KdcPostToServer("/kds/exec_activation", &param, &result);
		if (ret != 0) {
			if (ignoreError != 0)
				ret = 0;
		} else if ((ret = SceLibKernel_EB6DA895(result.pBody, g_c1Response,
							result.pBody + KDC_R1_OFFSET)) != 0) {
			ret = KdcConvertVerifyError(ret);
		} else {
			lock = KdcLockRevocationList();
			if (lock >= 0) {
				ret = KdcRefreshRevocationList(g_psmPath);
				sceKernelUnlockMutex(lock, 1);
				sceKernelDeleteMutex(lock);
			} else {
				ret = lock;
			}
		}
	}
	KdcStateEnd();
	return ret;
}

/* activates the console, the server's errors ignored */
int SceDrmPsmKdc_BEDD3A8B(const void *pTicket, SceSize ticketSize, SceUInt32 *pMax,
			  SceUInt32 *pAvailable)
{
	return KdcActivate(pTicket, ticketSize, pMax, pAvailable, 1);
}

/* activates the console */
int SceDrmPsmKdc_4321FAB4(const void *pTicket, SceSize ticketSize, SceUInt32 *pMax,
			  SceUInt32 *pAvailable)
{
	return KdcActivate(pTicket, ticketSize, pMax, pAvailable, 0);
}

/* the posts */

/* the error a server's reason is reported as */
__attribute__((noinline))
static int KdcConvertReason(int reason)
{
	int i;

	for (i = 0; s_reasons[i].reason != 0xFFFF; i++) {
		if (s_reasons[i].reason == reason)
			return s_reasons[i].error;
	}
	return PSM_KDC_ERROR_UNKNOWN_REASON;
}

/* ScePsmDrm's error, as an activation's call reports it */
__attribute__((noinline))
static int KdcConvertDrmError(int error)
{
	switch (error) {
	case (int)0x808F0102:
	case (int)0x808F0103:
	case (int)0x808F0104:
	case (int)0x808F0105:
	case (int)0x808F0106:
	case (int)0x808F0107:
	case (int)0x808F0109:
	case (int)0x808F010A:
		return error;
	case PSM_DRM_ERROR_ACCOUNT_MISMATCH:
		return PSM_KDC_ERROR_ACCOUNT_MISMATCH;
	default:
		return PSM_KDC_ERROR_INTERNAL;
	}
}

/* the same, for the check of an R1 */
__attribute__((noinline))
static int KdcConvertVerifyError(int error)
{
	switch (error) {
	case (int)0x808F0102:
	case (int)0x808F0103:
	case (int)0x808F0104:
	case (int)0x808F0105:
	case (int)0x808F0106:
	case (int)0x808F0107:
	case (int)0x808F0109:
	case (int)0x808F010A:
		return error;
	case PSM_DRM_ERROR_ACCOUNT_MISMATCH:
		return PSM_KDC_ERROR_ACCOUNT_MISMATCH;
	default:
		return PSM_KDC_ERROR_INTERNAL;
	}
}

/* GcAuthMgr's error of a C1 */
__attribute__((noinline))
static int KdcConvertC1Error(int error)
{
	switch (error) {
	case GCAUTHMGR_ERROR_NO_SECURE_CLOCK:
		return PSM_KDC_ERROR_NO_SECURE_CLOCK;
	case GCAUTHMGR_ERROR_PSMACT_FAILED:
		return PSM_KDC_ERROR_C1_FAILED;
	case GCAUTHMGR_ERROR_PSMACT_EXPIRED:
		return PSM_KDC_ERROR_C1_EXPIRED;
	default:
		return PSM_KDC_ERROR_INTERNAL;
	}
}

/* posts to https://pHost pPath, the answer's body read into pResult's buffer */
__attribute__((noinline))
static int KdcPost(const char *pHost, const char *pPath, const KdcPostParam *pParam,
		   const KdcPostResult *pResult)
{
	SceSize read;
	SceUInt64 contentLength;
	char url[128];
	KdcRequest *pRequest;
	int ret;

	contentLength = 0;
	read = 0;
	sceClibMemset(url, 0, sizeof(url));
	sceClibSnprintf(url, sizeof(url), "https://%s%s", pHost, pPath);
	sceClibMemset(&g_postRequestData, 0, sizeof(KdcRequest));
	g_pPostRequest = &g_postRequestData;
	ret = KdcStateBegin(KDC_STATE_POSTING);
	if (ret == 0) {
		if (KdcCreateTemplate(5000000, 20000000, 60000000) >= 0) {
			if (KdcCreateRequest(&g_pPostRequest, url) < 0) {
				ret = PSM_KDC_ERROR_CONNECTION;
				if (g_pPostRequest != NULL)
					KdcDeleteRequest(g_pPostRequest);
				KdcDeleteTemplate();
			} else {
				ret = 0;
			}
		} else {
			ret = PSM_KDC_ERROR_INTERNAL;
		}
		if (ret == 0) {
			ret = KdcSend(g_pPostRequest, pParam->pTicket, pParam->ticketSize,
				      pParam->pC1, pParam->c1Size, pParam->pContentId,
				      pParam->contentIdSize, url);
			if (ret != 0) {
				pRequest = g_pPostRequest;
				if (pRequest->refused == 1) {
					ret = KdcConvertReason(pRequest->reason);
					pRequest = g_pPostRequest;
				}
				if (pResult->wantConsoleInfo != 0 &&
				    pRequest->reason == KDC_REASON_CONSOLE_LIMIT)
					KdcGetConsoleInfo(pRequest, pResult->pMaxConsole,
							  pResult->pAvailableConsole);
			} else if (pResult->wantConsoleInfo != 0 &&
				   (ret = KdcGetConsoleInfo(g_pPostRequest, pResult->pMaxConsole,
							    pResult->pAvailableConsole)) < 0) {
				ret = PSM_KDC_ERROR_BAD_RESPONSE;
			} else if (pResult->pBody != NULL) {
				ret = KdcGetContentLength(g_pPostRequest, &contentLength);
				if (ret >= 0) {
					if (contentLength == pResult->bodySize) {
						ret = KdcReadData(g_pPostRequest, pResult->pBody,
								  (SceSize)contentLength, &read);
						if (ret < 0)
							ret = PSM_KDC_ERROR_BAD_BODY;
					} else {
						ret = PSM_KDC_ERROR_BAD_BODY;
					}
				}
			}
		}
	}
	KdcDeleteRequest(g_pPostRequest);
	KdcDeleteTemplate();
	if (ret != 0) {
		if (KdcIsAborted() != 0)
			ret = PSM_KDC_ERROR_ABORTED;
	} else {
		ret = KdcStateBegin(KDC_STATE_RUNNING);
	}
	g_pPostRequest = NULL;
	return ret;
}

/* posts to the KDS server of the console's NP environment */
__attribute__((noinline))
static int KdcPostToServer(const char *pPath, const KdcPostParam *pParam,
			   const KdcPostResult *pResult)
{
	char environment[17];
	char host[128];
	int ret;

	ret = KdcGetNpEnvironment(environment);
	if (ret == 0) {
		sceClibSnprintf(host, sizeof(host), "kds.%s.psm.playstation.net", environment);
		ret = KdcPost(host, pPath, pParam, pResult);
	}
	return ret;
}

__attribute__((noinline))
static int KdcAbortPost(void)
{
	int ret;

	if (g_pPostRequest == NULL)
		return 0;
	ret = KdcAbort(g_pPostRequest);
	if (ret != 0)
		return PSM_KDC_ERROR_INTERNAL;
	return ret;
}

/* the mutexes */

__attribute__((noinline))
static int KdcMutexCreate(const char *pName, SceUID *pId)
{
	SceUID id = sceKernelCreateMutex(pName, 0, 0, NULL);

	if (id >= 0) {
		*pId = id;
		return 0;
	}
	return id;
}

__attribute__((noinline))
static int KdcMutexLock(SceUID *pId)
{
	return sceKernelLockMutex(*pId, 1, NULL);
}

__attribute__((noinline))
static int KdcMutexUnlock(SceUID *pId)
{
	return sceKernelUnlockMutex(*pId, 1);
}

__attribute__((noinline))
static int KdcMutexDelete(SceUID *pId)
{
	return sceKernelDeleteMutex(*pId);
}

/* the revocation list */

/* removes the files of the directory pPath (length characters, in a buffer of size) */
__attribute__((noinline))
static int KdcRemoveFiles(char *pPath, SceSize size, SceSize length)
{
	SceIoDirent entry;
	SceUID fd;
	int ret;

	fd = sceIoDopen(pPath);
	if (fd == SCE_ERROR_ERRNO_ENOENT || fd == SCE_ERROR_ERRNO_ENODEV)
		return PSM_KDC_ERROR_NOT_FOUND;
	if (fd < 0)
		return PSM_KDC_ERROR_DIRECTORY;
	for (;;) {
		ret = sceIoDread(fd, &entry);
		if (ret == 0)
			break;
		if (ret < 0)
			return PSM_KDC_ERROR_DIRECTORY;
		if (sceClibStrncmp(entry.d_name, ".", 2) == 0 ||
		    sceClibStrncmp(entry.d_name, "..", 3) == 0)
			continue;
		if (sceClibStrnlen(entry.d_name, 0x7FFFFFFF) + length + 1 > size)
			break;
		sceClibStrncpy(pPath + length, entry.d_name, size - length);
		if (sceIoRemove(pPath) != 0)
			break;
	}
	ret = sceIoDclose(fd);
	if (ret != 0)
		return PSM_KDC_ERROR_INTERNAL;
	return ret;
}

/*
 * Revokes the contents of the list's entries (size bytes) that are installed: their licenses
 * removed, and "revoked" made in their System folder.
 */
__attribute__((noinline))
static int KdcRevokeContents(const char *pPsmPath, const KdcRevocationEntry *pEntry,
			     SceSize size)
{
	char path[1024];
	char titleId[10];
	char roMount[16];
	char rwMount[16];
	char *pRoMount;
	char *pRwMount;
	SceUInt32 count;
	SceUInt32 i;
	int ret;
	int roErr;
	int rwErr;
	int err;
	SceUID fd;

	count = size >> 6;
	for (i = 0; i < count; i++, pEntry++) {
		pRoMount = roMount;
		if (KdcIsAborted() != 0)
			return PSM_KDC_ERROR_ABORTED;
		roErr = 0;
		rwErr = 0;
		pRwMount = NULL;
		if (KdcGetTitleId(pEntry, titleId) != 0)
			return PSM_KDC_ERROR_INTERNAL;
		sceClibSnprintf(path, sizeof(path), "%s/%s", KDC_PSM_DIRECTORY, titleId);
		if (!KdcIsDirectory(path))
			continue;
		err = sceAppMgrAppDataMountById(106, titleId, pRoMount);
		if (err == 0) {
			err = sceAppMgrAppDataMountById(107, titleId, rwMount);
			if (err == 0) {
				pRwMount = rwMount;
				err = KdcGetLicenseDirectory(pPsmPath, pEntry, path, sizeof(path));
				if (err == 0) {
					ret = KdcRemoveFiles(path, sizeof(path),
							     sceClibStrnlen(path, 0x7FFFFFFF));
					err = ret;
					if (ret == PSM_KDC_ERROR_NOT_FOUND) {
						err = 0;
					} else if (ret == 0) {
						err = KdcGetRevokedPath(pPsmPath, pEntry, path,
									sizeof(path));
						if (err == 0) {
							fd = sceIoOpen(path, SCE_O_RDWR | SCE_O_CREAT,
								       0606);
							if (fd >= 0) {
								sceIoClose(fd);
								fd = 0;
							}
							err = fd;
							if (fd != 0)
								err = 0;
						}
					}
				}
			}
			if (pRoMount != NULL)
				roErr = sceAppMgrUmount(pRoMount);
			if (pRwMount != NULL)
				rwErr = sceAppMgrUmount(pRwMount);
		}
		if (err != 0)
			return err;
		if ((roErr | rwErr) != 0)
			return PSM_KDC_ERROR_INTERNAL;
	}
	return 0;
}

/* the header of the revocation list, read big-endian */
__attribute__((noinline))
static void KdcSwapRevocationHeader(KdcRevocationHeader *pHeader)
{
	SceUInt64 magic = pHeader->magic;
	SceUInt32 value;

	pHeader->magic = (magic << 56) | ((magic << 40) & 0x00FF000000000000ULL) |
			 ((magic << 24) & 0x0000FF0000000000ULL) |
			 ((magic << 8) & 0x000000FF00000000ULL) | ((magic >> 8) & 0xFF000000ULL) |
			 ((magic >> 24) & 0xFF0000ULL) | ((magic >> 40) & 0xFF00ULL) | (magic >> 56);
	value = pHeader->format;
	pHeader->format = (value >> 24) | (((value >> 16) & 0xFF) << 8) |
			  (((value >> 8) & 0xFF) << 16) | ((value & 0xFF) << 24);
	value = pHeader->version;
	pHeader->version = (value >> 24) | (((value >> 16) & 0xFF) << 8) |
			   (((value >> 8) & 0xFF) << 16) | ((value & 0xFF) << 24);
	value = pHeader->size;
	pHeader->size = (value >> 24) | (((value >> 16) & 0xFF) << 8) |
			(((value >> 8) & 0xFF) << 16) | ((value & 0xFF) << 24);
	value = pHeader->count;
	pHeader->count = (value >> 24) | (((value >> 16) & 0xFF) << 8) |
			 (((value >> 8) & 0xFF) << 16) | ((value & 0xFF) << 24);
}

__attribute__((noinline))
static int KdcCheckRevocationHeader(const KdcRevocationHeader *pHeader)
{
	if (pHeader->magic == KDC_RVK_MAGIC && pHeader->format == KDC_RVK_FORMAT)
		return 0;
	return PSM_KDC_ERROR_RVK_FORMAT;
}

/*
 * Downloads the revocation list from http://pHost pPath into pFile, through the buffer; the
 * list must be newer than version and signed.
 */
__attribute__((noinline))
static int KdcDownloadRevocationList(const char *pHost, const char *pPath, const char *pFile,
				     void *pBuffer, SceSize size, SceUInt32 version)
{
	SceUChar8 digest[32];
	int statusCode;
	SceUInt64 contentLength;
	KdcRevocationHeader header;
	SceSha256Context sha;
	SceUInt8 signature[KDC_RVK_SIGNATURE_SIZE];
	void *pData;
	SceUID fd;
	SceSize remaining;
	SceSize total;
	char *p;
	int requestId;
	int ret;
	int err;
	int n;

	pData = pBuffer;
	contentLength = 0;
	sceClibMemset(&g_downloadRequestData, 0, sizeof(KdcRequest));
	g_pDownloadRequest = &g_downloadRequestData;
	ret = KdcStateBegin(KDC_STATE_DOWNLOADING);
	if (ret != 0)
		goto done;
	sceClibSnprintf(g_url, sizeof(g_url), "http://%s%s", pHost, pPath);
	if (KdcCreateTemplate(20000000, 20000000, 60000000) < 0) {
		ret = PSM_KDC_ERROR_INTERNAL;
		goto done;
	}
	if (KdcCreateRequest(&g_pDownloadRequest, g_url) < 0) {
		ret = PSM_KDC_ERROR_RVK_CONNECTION;
		goto done;
	}
	requestId = sceHttpCreateRequestWithURL(g_pDownloadRequest->connectionId,
						SCE_HTTP_METHOD_GET, g_url, 0);
	if (requestId < 0) {
		ret = PSM_KDC_ERROR_RVK_CONNECTION;
		goto done;
	}
	g_pDownloadRequest->requestId = requestId;
	if (sceHttpSendRequest(g_pDownloadRequest->requestId, NULL, 0) != 0) {
		ret = PSM_KDC_ERROR_RVK_CONNECTION;
		goto done;
	}
	if (sceHttpGetStatusCode(g_pDownloadRequest->requestId, &statusCode) < 0) {
		ret = PSM_KDC_ERROR_RVK_STATUS;
		goto done;
	}
	if (statusCode != 200) {
		ret = PSM_KDC_ERROR_RVK_STATUS;
		goto done;
	}
	if (sceHttpGetResponseContentLength(g_pDownloadRequest->requestId, &contentLength) < 0) {
		ret = PSM_KDC_ERROR_RVK_STATUS;
		goto done;
	}
	if (contentLength != KDC_RVK_DATA_OFFSET && ((contentLength - 64) & 0x3F) != 0) {
		ret = PSM_KDC_ERROR_RVK_LENGTH;
		goto done;
	}
	requestId = g_pDownloadRequest->requestId;
	sceSha256BlockInit(&sha);
	fd = sceIoOpen(pFile, SCE_O_RDWR | SCE_O_CREAT, 0606);
	if (fd < 0) {
		ret = PSM_KDC_ERROR_RVK_WRITE;
		goto done;
	}
	if (sceHttpReadData(requestId, &header, KDC_RVK_HEADER_SIZE) != KDC_RVK_HEADER_SIZE) {
		err = PSM_KDC_ERROR_RVK_READ;
		goto close;
	}
	if (sceIoWrite(fd, &header, KDC_RVK_HEADER_SIZE) != KDC_RVK_HEADER_SIZE) {
		err = PSM_KDC_ERROR_RVK_WRITE;
		goto close;
	}
	sceSha256BlockUpdate(&sha, &header, KDC_RVK_HEADER_SIZE);
	KdcSwapRevocationHeader(&header);
	err = KdcCheckRevocationHeader(&header);
	if (err != 0)
		goto close;
	if (header.version <= version) {
		err = PSM_KDC_ERROR_RVK_NOT_NEWER;
		goto close;
	}
	sceClibMemset(signature, 0, sizeof(signature));
	sceSha256BlockUpdate(&sha, signature, sizeof(signature));
	if (sceHttpReadData(requestId, signature, sizeof(signature)) != sizeof(signature)) {
		err = PSM_KDC_ERROR_RVK_READ;
		goto close;
	}
	if (sceIoWrite(fd, signature, sizeof(signature)) != sizeof(signature)) {
		err = PSM_KDC_ERROR_RVK_WRITE;
		goto close;
	}
	for (total = 0; ; total += err) {
		remaining = size;
		p = pData;
		err = 0;
		while (remaining != 0) {
			n = sceHttpReadData(requestId, p, remaining);
			if (KdcIsAborted() != 0) {
				err = PSM_KDC_ERROR_ABORTED;
				goto check;
			}
			if (n == 0)
				break;
			if (n < 0) {
				err = n;
				break;
			}
			remaining -= n;
			err += n;
			p += n;
		}
		if (err == 0)
			break;
check:
		if (err < 0)
			goto close;
		if ((err & 0x3F) != 0) {
			err = PSM_KDC_ERROR_RVK_FORMAT;
			goto close;
		}
		if (err != sceIoWrite(fd, pData, err)) {
			err = PSM_KDC_ERROR_RVK_WRITE;
			goto close;
		}
		sceSha256BlockUpdate(&sha, pData, err);
	}
	if (header.size == total + KDC_RVK_DATA_OFFSET && header.count == total >> 6) {
		sceSha256BlockResult(&sha, digest);
		err = KdcVerifyRevocationList(digest, signature);
		if (err != 0)
			err = PSM_KDC_ERROR_RVK_BAD_SIGNATURE;
	} else {
		err = PSM_KDC_ERROR_RVK_FORMAT;
	}
close:
	if (fd >= 0)
		sceIoClose(fd);
	ret = err;
done:
	KdcDeleteRequest(g_pDownloadRequest);
	KdcDeleteTemplate();
	if (ret != 0) {
		if (KdcIsAborted() != 0)
			ret = PSM_KDC_ERROR_ABORTED;
	} else {
		ret = KdcStateBegin(KDC_STATE_RUNNING);
	}
	g_pDownloadRequest = NULL;
	return ret;
}

/* downloads the revocation list of the console's NP environment, up to three times */
__attribute__((noinline))
static int KdcUpdateRevocationList(int unused, const char *pFile, void *pBuffer, SceSize size,
				   SceUInt32 version)
{
	char environment[17];
	char path[128];
	int tries = 0;
	int ret;

	ret = KdcGetNpEnvironment(environment);
	if (ret != 0)
		return ret;
	if (sceClibStrncmp(environment, "np", 2) != 0)
		sceClibSnprintf(path, sizeof(path), "/psm/revoke/%s/rvk.dat", environment);
	else
		sceClibStrncpy(path, "/psm/revoke/rvk.dat", sizeof(path));
	do {
		ret = KdcDownloadRevocationList("psm.dl.playstation.net", path, pFile, pBuffer, size,
						version);
		tries++;
	} while (ret != 0 && tries < 3);
	return ret;
}

__attribute__((noinline))
static int KdcAbortDownload(void)
{
	int ret;

	if (g_pDownloadRequest == NULL)
		return 0;
	ret = KdcAbort(g_pDownloadRequest);
	if (ret != 0)
		return PSM_KDC_ERROR_INTERNAL;
	return ret;
}

/* the list's signature over its SHA-256 */
__attribute__((noinline))
static int KdcVerifyRevocationList(SceUChar8 *pDigest, SceUInt8 *pSignature)
{
	MarlinRsaExponentKey key;
	MarlinRsaData signature;
	MarlinRsaData digest;
	int ret;

	digest.data = pDigest;
	digest.size = 32;
	signature.data = pSignature;
	signature.size = KDC_RVK_SIGNATURE_SIZE;
	key.modulus = (unsigned int *)s_revocationModulus;
	key.exponent = (unsigned int *)s_revocationExponent;
	ret = GcAuthRsaVerify2048(&signature, &digest, &key, 11);
	if (ret >= 0)
		ret = 0;
	return ret;
}

/* the buffer the list is read through, and the list's paths */
__attribute__((noinline))
static int KdcRevocationInit(SceSize size, void *pBuffer, int unused)
{
	g_pRevocationBuffer = pBuffer;
	g_revocationBufferSize = size - (size & 0x3F);
	sceClibMemset(g_revocationPath, 0, sizeof(g_revocationPath));
	sceClibMemset(g_revocationTmpPath, 0, sizeof(g_revocationTmpPath));
	sceClibSnprintf(g_revocationPath, sizeof(g_revocationPath), "%s/%s", KDC_RVK_DIRECTORY,
			"rvk.dat");
	sceClibSnprintf(g_revocationTmpPath, sizeof(g_revocationTmpPath), "%s/%s",
			KDC_RVK_DIRECTORY, "rvk.dat.tmp");
	return 0;
}

/*
 * Reads and checks the revocation list: its version into *pVersion; whether pContentId is in
 * it into *pFound. 1 when there is no list.
 */
__attribute__((noinline))
static int KdcReadRevocationList(SceUInt32 *pVersion, const char *pContentId, int *pFound)
{
	KdcRevocationHeader header;
	SceUChar8 digest[32];
	SceSha256Context sha;
	SceUInt8 signature[KDC_RVK_SIGNATURE_SIZE];
	const KdcRevocationEntry *pEntry;
	void *pData;
	SceSize remaining;
	SceSize total;
	SceUInt32 count;
	SceUInt32 i;
	char *p;
	SceUID fd;
	int found;
	int got;
	int ret;
	int n;

	total = 0;
	sceSha256BlockInit(&sha);
	fd = sceIoOpen(g_revocationPath, SCE_O_RDONLY, 0);
	if (fd == SCE_ERROR_ERRNO_ENOENT)
		return 1;
	if (fd < 0)
		return PSM_KDC_ERROR_RVK_READ;
	n = sceIoRead(fd, &header, KDC_RVK_HEADER_SIZE);
	if (n != KDC_RVK_HEADER_SIZE) {
		ret = PSM_KDC_ERROR_RVK_READ;
		if (n == SCE_ERROR_ERRNO_ENODEV)
			ret = SCE_ERROR_ERRNO_ENODEV;
		goto close;
	}
	sceSha256BlockUpdate(&sha, &header, KDC_RVK_HEADER_SIZE);
	KdcSwapRevocationHeader(&header);
	ret = KdcCheckRevocationHeader(&header);
	if (ret != 0)
		goto close;
	if (pVersion != NULL)
		*pVersion = header.version;
	sceClibMemset(signature, 0, sizeof(signature));
	sceSha256BlockUpdate(&sha, signature, sizeof(signature));
	n = sceIoRead(fd, signature, sizeof(signature));
	if (n != sizeof(signature)) {
		ret = PSM_KDC_ERROR_RVK_READ;
		if (n == SCE_ERROR_ERRNO_ENODEV)
			ret = SCE_ERROR_ERRNO_ENODEV;
		goto close;
	}
	pData = g_pRevocationBuffer;
	if (pContentId != NULL)
		*pFound = 0;
	for (;;) {
		remaining = g_revocationBufferSize;
		p = pData;
		got = 0;
		for (;;) {
			n = sceIoRead(fd, p, remaining);
			if (n == 0)
				break;
			if (n < 0) {
				got = n;
				break;
			}
			got += n;
			remaining -= n;
			p += n;
		}
		if (got == 0)
			break;
		if (KdcIsAborted() != 0) {
			ret = PSM_KDC_ERROR_ABORTED;
			goto close;
		}
		if (got < 0) {
			ret = PSM_KDC_ERROR_RVK_READ;
			if (got == SCE_ERROR_ERRNO_ENODEV)
				ret = SCE_ERROR_ERRNO_ENODEV;
			goto close;
		}
		if ((got & 0x3F) != 0) {
			ret = PSM_KDC_ERROR_RVK_FORMAT;
			goto close;
		}
		sceSha256BlockUpdate(&sha, g_pRevocationBuffer, got);
		pData = g_pRevocationBuffer;
		total += got;
		if (pContentId != NULL && *pFound == 0) {
			count = (SceUInt32)got >> 6;
			pEntry = g_pRevocationBuffer;
			found = 0;
			for (i = 0; i < count; i++, pEntry++) {
				if (sceClibMemcmp(pEntry, pContentId, KDC_CONTENT_ID_LENGTH) == 0) {
					pData = g_pRevocationBuffer;
					found = 1;
					break;
				}
			}
			if (found == 0)
				pData = g_pRevocationBuffer;
			*pFound = found;
		}
	}
	if (header.size == total + KDC_RVK_DATA_OFFSET && header.count == total >> 6) {
		sceSha256BlockResult(&sha, digest);
		ret = KdcVerifyRevocationList(digest, signature);
		if (ret != 0)
			ret = PSM_KDC_ERROR_RVK_BAD_LOCAL_SIGNATURE;
	} else {
		ret = PSM_KDC_ERROR_RVK_FORMAT;
	}
close:
	if (fd != -1)
		sceIoClose(fd);
	return ret;
}

/* revokes the contents the list pFile names */
__attribute__((noinline))
static int KdcApplyRevocationList(const char *pPsmPath, const char *pFile)
{
	SceSize remaining;
	char *p;
	SceUID fd;
	int ret;
	int got;
	int n;

	fd = sceIoOpen(pFile, SCE_O_RDONLY, 0);
	if (fd < 0) {
		ret = PSM_KDC_ERROR_RVK_READ;
	} else if (sceIoLseek32(fd, KDC_RVK_DATA_OFFSET, SCE_SEEK_SET) < 0) {
		ret = PSM_KDC_ERROR_RVK_READ;
	} else {
		for (;;) {
			p = g_pRevocationBuffer;
			remaining = g_revocationBufferSize;
			got = 0;
			for (;;) {
				n = sceIoRead(fd, p, remaining);
				if (n == 0)
					break;
				if (n < 0) {
					got = n;
					goto read;
				}
				got += n;
				remaining -= n;
				p += n;
			}
read:
			if (got == 0)
				break;
			if (got < 0) {
				ret = PSM_KDC_ERROR_RVK_SCAN;
				goto close;
			}
			if ((got & 0x3F) != 0) {
				ret = PSM_KDC_ERROR_RVK_FORMAT;
				goto close;
			}
			ret = KdcRevokeContents(pPsmPath, g_pRevocationBuffer, got);
			if (ret != 0)
				goto close;
		}
		ret = 0;
	}
close:
	if (fd >= 0)
		sceIoClose(fd);
	return ret;
}

/* the version of the list the activation asks for, against the one installed */
static inline __attribute__((always_inline)) int KdcCompareRevocationList(SceUInt32 *pVersion,
		int *pRequired, int *pUpdate)
{
	SceUInt64 accountId;
	SceUInt64 startTime;
	SceUInt64 endTime;
	int actType;
	int tries = 0;
	int ret;

	ret = KdcGetActInfo(&actType, pRequired, &accountId, &startTime, &endTime);
	if (ret != 0)
		return KdcConvertDrmError(ret);
	if (*pRequired == 0)
		return 0;
	do {
		ret = KdcReadRevocationList(pVersion, NULL, NULL);
		tries++;
		if (ret == 0) {
			if ((SceUInt32)*pRequired <= *pVersion)
				return 0;
			*pUpdate = 1;
			return PSM_KDC_ERROR_RVK_OUTDATED;
		}
	} while (tries < 3);
	*pUpdate = 1;
	*pVersion = 0;
	if (ret == 1)
		ret = PSM_KDC_ERROR_RVK_READ;
	return ret;
}

/* brings the revocation list up to the activation's version, applied to the contents */
__attribute__((noinline))
static int KdcRefreshRevocationList(const char *pPsmPath)
{
	SceUInt32 version = 0;
	int required = 0;
	int update = 0;
	int tries = 0;
	int ret;
	int err;

	err = KdcCompareRevocationList(&version, &required, &update);
	ret = err;
	if (err == SCE_ERROR_ERRNO_ENODEV) {
		ret = PSM_KDC_ERROR_RVK_READ;
	} else if (update != 0) {
		if (!KdcIsDirectory(KDC_RVK_DIRECTORY)) {
			ret = sceIoMkdir(KDC_RVK_DIRECTORY, 0707);
			if (ret != 0)
				return ret;
		}
		ret = KdcUpdateRevocationList(0, g_revocationTmpPath, g_pRevocationBuffer,
					      g_revocationBufferSize, version);
		if (ret == 0) {
			do {
				ret = KdcApplyRevocationList(pPsmPath, g_revocationTmpPath);
				tries++;
			} while (ret != 0 && tries < 3);
			if (ret == PSM_KDC_ERROR_RVK_READ) {
				ret = PSM_KDC_ERROR_RVK_WRITE;
			} else if (ret == 0 && (!KdcIsFile(g_revocationPath) ||
						(ret = sceIoRemove(g_revocationPath)) == 0)) {
				ret = sceIoRename(g_revocationTmpPath, g_revocationPath);
				if (ret == 0)
					return 0;
				ret = PSM_KDC_ERROR_RVK_RENAME;
			}
		}
	} else if (err == 0) {
		return ret;
	}
	if (KdcIsFile(g_revocationTmpPath))
		sceIoRemove(g_revocationTmpPath);
	return ret;
}

/* applies the installed list to the contents, when it is the activation's version */
__attribute__((noinline))
static int KdcCheckRevocationList(const char *pPsmPath)
{
	SceUInt32 version = 0;
	int required = 0;
	int update = 0;
	int tries = 0;
	int ret;

	ret = KdcCompareRevocationList(&version, &required, &update);
	if (ret != 0 || update == 1) {
		if (ret == SCE_ERROR_ERRNO_ENODEV)
			ret = PSM_KDC_ERROR_RVK_READ;
	} else if (required != 0) {
		do {
			ret = KdcApplyRevocationList(pPsmPath, g_revocationPath);
			tries++;
		} while (ret != 0 && tries < 3);
	} else {
		ret = 0;
	}
	return ret;
}

/* whether the list names the content: *pRevoked, 1 unless the list says otherwise */
__attribute__((noinline))
static int KdcIsRevoked(const char *pContentId, int *pRevoked)
{
	SceUInt64 accountId;
	SceUInt64 startTime;
	SceUInt64 endTime;
	int actType;
	int required;
	int tries = 0;
	int ret;

	*pRevoked = 1;
	ret = KdcGetActInfo(&actType, &required, &accountId, &startTime, &endTime);
	if (ret != 0)
		return KdcConvertDrmError(ret);
	if (required == 0) {
		*pRevoked = 0;
		return 0;
	}
	do {
		ret = KdcReadRevocationList(NULL, pContentId, pRevoked);
		tries++;
	} while (ret != 0 && tries < 3);
	if (ret == 1)
		ret = PSM_KDC_ERROR_RVK_READ;
	else if (ret == SCE_ERROR_ERRNO_ENODEV)
		ret = PSM_KDC_ERROR_RVK_READ;
	return ret;
}

/* the state: what runs, and whether it was aborted */

__attribute__((noinline))
static int KdcStateInit(void)
{
	int ret = 0;

	if (KdcMutexCreate("stateMutex", &g_stateMutex) != 0)
		return PSM_KDC_ERROR_INTERNAL;
	return ret;
}

__attribute__((noinline))
static int KdcStateTerm(void)
{
	int ret = 0;

	if (KdcMutexDelete(&g_stateMutex) != 0)
		return PSM_KDC_ERROR_INTERNAL;
	return ret;
}

/* state begins, unless an abort is pending */
__attribute__((noinline))
static int KdcStateBegin(int state)
{
	int ret = 0;

	if (KdcMutexLock(&g_stateMutex) != 0)
		return PSM_KDC_ERROR_INTERNAL;
	if (g_aborted != 0)
		ret = PSM_KDC_ERROR_ABORTED;
	else
		g_state = state;
	KdcMutexUnlock(&g_stateMutex);
	return ret;
}

__attribute__((noinline))
static int KdcStateGet(void)
{
	return g_state;
}

__attribute__((noinline))
static int KdcStateEnd(void)
{
	if (KdcMutexLock(&g_stateMutex) != 0)
		return PSM_KDC_ERROR_INTERNAL;
	g_state = KDC_STATE_IDLE;
	g_aborted = 0;
	KdcMutexUnlock(&g_stateMutex);
	return 0;
}

__attribute__((noinline))
static int KdcSetAborted(int aborted)
{
	g_aborted = aborted;
	return aborted;
}

__attribute__((noinline))
static int KdcIsAborted(void)
{
	return g_aborted;
}

__attribute__((noinline))
static int KdcStateLock(void)
{
	return KdcMutexLock(&g_stateMutex);
}

__attribute__((noinline))
static int KdcStateUnlock(void)
{
	return KdcMutexUnlock(&g_stateMutex);
}

/* the files */

/* reads the file pPath, of exactly size bytes */
__attribute__((noinline))
static int KdcReadFile(const char *pPath, void *pBuffer, SceSize size)
{
	SceIoStat stat;
	char *p = pBuffer;
	SceUID fd;
	int ret = 0;
	int n;

	if (sceIoGetstat(pPath, &stat) != 0)
		return PSM_KDC_ERROR_NOT_FOUND;
	if (stat.st_size != size)
		return PSM_DRM_ERROR_BAD_LICENSE_SIZE;
	fd = sceIoOpen(pPath, SCE_O_RDONLY, 0);
	if (fd < 0) {
		ret = PSM_KDC_ERROR_INTERNAL;
	} else {
		while (size != 0) {
			n = sceIoRead(fd, p, size);
			if (n < 0) {
				ret = PSM_KDC_ERROR_INTERNAL;
				break;
			}
			size -= n;
			p += n;
		}
	}
	if (fd >= 0)
		sceIoClose(fd);
	return ret;
}

/* the title ID in a content ID */
__attribute__((noinline))
static int KdcGetTitleId(const KdcRevocationEntry *pEntry, char *pTitleId)
{
	if (pEntry == NULL || pTitleId == NULL)
		return PSM_KDC_ERROR_INVALID_ARG;
	sceClibMemset(pTitleId, 0, KDC_TITLE_ID_LENGTH + 1);
	sceClibMemcpy_safe(pTitleId, pEntry->contentId + KDC_TITLE_ID_OFFSET, KDC_TITLE_ID_LENGTH);
	return 0;
}

__attribute__((noinline))
static int KdcIsFile(const char *pPath)
{
	SceIoStat stat;
	int ret = 0;

	if (sceIoGetstat(pPath, &stat) == 0)
		ret = SCE_STM_ISREG(stat.st_mode);
	return ret;
}

__attribute__((noinline))
static int KdcIsDirectory(const char *pPath)
{
	SceIoStat stat;
	int ret = 0;

	if (sceIoGetstat(pPath, &stat) == 0)
		ret = SCE_STM_ISDIR(stat.st_mode);
	return ret;
}

/* an application's license: pAppPath/License/name */
__attribute__((noinline))
static int KdcGetLicensePath(const char *pAppPath, const char *pName, char *pPath, SceSize size)
{
	int n = sceClibSnprintf(pPath, size, "%s/License/%s", pAppPath, pName);

	if (n >= 0 && n < size)
		return 0;
	return PSM_KDC_ERROR_INTERNAL;
}

/* the licenses of an installed content: pPsmPath/<title ID>/RO/License/ */
__attribute__((noinline))
static int KdcGetLicenseDirectory(const char *pPsmPath, const KdcRevocationEntry *pEntry,
				  char *pPath, SceSize size)
{
	char titleId[10];
	int n;

	if (pEntry == NULL)
		return PSM_KDC_ERROR_INTERNAL;
	sceClibMemset(titleId, 0, sizeof(titleId));
	sceClibMemcpy_safe(titleId, pEntry->contentId + KDC_TITLE_ID_OFFSET, KDC_TITLE_ID_LENGTH);
	n = sceClibSnprintf(pPath, size, "%s/%s/%s/", pPsmPath, titleId, "RO/License");
	if (n >= 0 && n < size)
		return 0;
	return PSM_KDC_ERROR_INTERNAL;
}

/* the mark of a revoked content: pPsmPath/<title ID>/RW/System/revoked */
__attribute__((noinline))
static int KdcGetRevokedPath(const char *pPsmPath, const KdcRevocationEntry *pEntry,
			     char *pPath, SceSize size)
{
	char titleId[10];
	int n;

	if (pEntry == NULL)
		return PSM_KDC_ERROR_INTERNAL;
	sceClibMemset(titleId, 0, sizeof(titleId));
	sceClibMemcpy_safe(titleId, pEntry->contentId + KDC_TITLE_ID_OFFSET, KDC_TITLE_ID_LENGTH);
	n = sceClibSnprintf(pPath, size, "%s/%s/%s/%s", pPsmPath, titleId, "RW/System", "revoked");
	if (n >= 0 && n < size)
		return 0;
	return PSM_KDC_ERROR_INTERNAL;
}

/* the running application's mark: pDataPath/System/revoked */
__attribute__((noinline))
static int KdcGetSystemRevokedPath(const char *pDataPath, char *pPath, SceSize size)
{
	int n = sceClibSnprintf(pPath, size, "%s/System/%s", pDataPath, "revoked");

	if (n >= 0 && n < size)
		return 0;
	return PSM_KDC_ERROR_INTERNAL;
}

/* the platform */

__attribute__((noinline))
static int KdcPlatformInit(int unused)
{
	return 0;
}

/* the console's NP environment ("np" for the public one) */
__attribute__((noinline))
static int KdcGetNpEnvironment(char *pEnvironment)
{
	return sceAppMgrSystemParamGetString(3, pEnvironment, 17);
}

__attribute__((noinline))
static int KdcPlatformTerm(void)
{
	return 0;
}

/* the secure clock: the vsh bridge's, else the DRM bridge's */
__attribute__((noinline))
static int KdcGetCurrentSecureTick(SceUInt64 *pTick)
{
	int ret;

	*pTick = 0;
	ret = KdcGetCurrentSecureTickVsh((SceRtcTick *)pTick);
	if (ret == SCE_ERROR_ERRNO_ENOSYS)
		ret = sceDrmBridgeGetCurrentSecureTick(pTick);
	if (ret == 0)
		return 0;
	return ret;
}

/* strtol: the digits of a number in the base (0: from its prefix), np_kdc's */
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
