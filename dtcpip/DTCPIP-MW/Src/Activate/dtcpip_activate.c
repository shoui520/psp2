/*
 * swagner's activation logic (DTCPIP-MW/Src/Activate/dtcpip_activate.c, from the trace
 * strings): device unique ID formatting, key ID lookup, HTTPS request to the Vita DTCP-IP
 * activation server, SSL verification callback and certificate storage in /CONFIG/DTCPIP.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <scetypes.h>
#include <kernel/openpsid.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libssl.h>
#include <libkernel/libkernel.h>
#include <sysmodule/sysmodule.h>

#define DTCPIP_ACTIVATE_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Activate\\dtcpip_activate.c"
#define DTCPIP_ACTIVATE_TRACE(line)						\
	sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(" #line ") : TRACE : %s()\n", __FUNCTION__)

#define SCE_SYSMODULE_HTTPS			0x0013
#define SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND	(-2143084507)
#define SCE_HTTP_ERROR_SSL			(-2143088523)
#define SCE_HTTP_ERROR_SSL_OUT_OF_MEMORY	(-2143088606)
#define SCE_SSL_ERROR_CERT_VERIFY		(-2143072160)
#define SCE_SSL_ERROR_HANDSHAKE			(-2143072159)
#define SCE_SSL_ERROR_IO			(-2143072158)
#define SCE_SSL_ERROR_INTERNAL			(-2143072157)

/* SceVshBridge, SceRegMgr and private HTTP/key-store helpers */
int vshSblAimgrIsCEX(void);
int vshSblAimgrIsDEX(void);
int vshSblAimgrIsTool(void);
int sceRegMgrGetKeyBin(const char *category, const char *name, void *buffer, int size);
int sceRegMgrSetKeyBin(const char *category, const char *name, const void *buffer, int size);
int sceHttpsEnableOptionPrivate(int tmplId, unsigned int sslFlags);
int sceHttpRequestGetAllHeaders(int reqId, void *headers, int *headerCount, void *buffer,
				int *bufferSize);
int swKeyStoreGetKeyId(unsigned short *keyId);

/* the activation's lock, the request it is waiting on, and the flag its cancel raises */
SceKernelLwMutexWork g_dtcpIpActivateMutex;
int g_dtcpIpActivateCancelled;
SceUID g_dtcpIpActivateRequest;

static const char *const g_activateResponseHeaders[13] = {
	"DATE",
	"Transfer-Encoding",
	"Pragma",
	"Cache-control",
	"Server",
	"X-AV-Server-Info",
	"Connection",
	"Content-Range",
	"Content-Range.dtcp.com",
	"Content-Type",
	"transferMode.dlna.org",
	"availableSeekRange.dlna.org",
	"TimeSeekRange.dlna.org",
};

struct SceHttpHeaderPair {
	const char *name;
	const char *value;
};

__attribute__((noinline)) static int ActivateLock(void)
{
	return sceKernelLockLwMutex(&g_dtcpIpActivateMutex, 1, NULL);
}

__attribute__((noinline)) static int ActivateUnlock(void)
{
	return sceKernelUnlockLwMutex(&g_dtcpIpActivateMutex, 1);
}

/* log known HTTP response headers */
static void printActivateResponseHeader(int reqId)
{
	char *header;
	unsigned int headerSize;
	const char *fieldValue;
	SceSize valueLen;
	char valueCopy[2048];
	int ret;
	unsigned int i;

	headerSize = 0;
	ret = sceHttpGetAllResponseHeaders(reqId, &header, &headerSize);
	if (ret >= 0) {
		sceClibPrintf("\n*** HTTP Response Header Start ***\n");
		for (i = 0; i < 13; ++i) {
			ret = sceHttpParseResponseHeader(header, headerSize,
							 g_activateResponseHeaders[i],
							 &fieldValue, &valueLen);
			if (ret == SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND || ret >= 0) {
				if (ret > 0) {
					sceClibStrncpy(valueCopy, fieldValue, valueLen);
					valueCopy[valueLen] = '\0';
					sceClibPrintf("\t%s :%s\n",
						      g_activateResponseHeaders[i], valueCopy);
				}
			} else {
				sceClibPrintf("%s :\n", g_activateResponseHeaders[i]);
				sceClibPrintf("sceHttpParseResponseHeader() error: 0x%08X\n", ret);
			}
		}
		sceClibPrintf("*** HTTP Response Header End ***\n\n");
	} else {
		sceClibPrintf("sceHttpGetAllResponseHeaders() error: 0x%08X\n", ret);
	}
}

/* format "<C|D|T>-<32-hex-openpsid>" */
static int GetDeviceUniqueID(char *buffer, SceSize bufferSize)
{
	char prefix;
	char *cursor;
	int written;
	int total;
	int ret;
	unsigned int i;
	SceKernelOpenPsId openPsId;

	DTCPIP_ACTIVATE_TRACE(211);
	cursor = buffer;
	written = 0;
	total = 0;
	prefix = 'T';
	if (bufferSize < 0x22)
		return -2;

	if (vshSblAimgrIsCEX() != 0)
		prefix = 'C';
	else if (vshSblAimgrIsDEX() != 0)
		prefix = 'D';
	else if (vshSblAimgrIsTool() != 0)
		prefix = 'T';

	written = sceClibSnprintf(cursor, bufferSize, "%c-", prefix);
	if (written < 2)
		return -4;

	cursor += written;
	total += written;
	ret = sceKernelGetOpenPsId(&openPsId);
	if (ret < 0)
		return ret;

	for (i = 0; i < 16; ++i) {
		written = sceClibSnprintf(cursor, bufferSize - total, "%02X", openPsId.id[i]);
		if (written < 2)
			return -4;
		cursor += written;
		total += written;
	}
	return 0;
}

/* read the DTCP key ID from the key store and map its error codes */
static int GetDtcpKeyID(unsigned short *keyId)
{
	int ret;

	DTCPIP_ACTIVATE_TRACE(266);
	ret = swKeyStoreGetKeyId(keyId);
	if (ret > -2) {
		if (ret < 0)
			return -3;
		if (ret <= 0)
			return 0;
		return -4;
	}
	if (ret != -3)
		return -4;
	return -5;
}

/* log HTTP request headers */
static void printActivateRequestHeader(int reqId)
{
	struct SceHttpHeaderPair headers[64];
	unsigned char buffer[128];
	int headerCount;
	int bufferSize;
	int ret;
	int i;

	i = 0;
	bufferSize = sizeof(buffer);
	headerCount = 64;
	ret = sceHttpRequestGetAllHeaders(reqId, headers, &headerCount, buffer, &bufferSize);
	if (ret >= 0) {
		sceClibPrintf("\n*** HTTP Request Header Start ***\n");
		for (i = 0; i < headerCount; ++i) {
			if (headers[i].name == NULL)
				break;
			if (headers[i].value == NULL)
				break;
			sceClibPrintf("\t%s :%s\n", headers[i].name, headers[i].value);
		}
		sceClibPrintf("*** HTTP Request Header End ***\n\n");
	} else {
		sceClibPrintf("sceHttpRequestGetAllHeaders() error: 0x%08X\n", ret);
	}
}

/* log SSL error and verification bits */
static void printSslError(int errNum, unsigned int verifyDetail)
{
	switch (errNum) {
	case SCE_SSL_ERROR_INTERNAL:
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(164) : ssl error: unexpcted error\n");
		break;
	case SCE_SSL_ERROR_IO:
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(158) : ssl error: io error\n");
		break;
	case SCE_SSL_ERROR_HANDSHAKE:
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(155) : ssl error: handshake error\n");
		break;
	case SCE_SSL_ERROR_CERT_VERIFY:
		if ((verifyDetail & 0x01) != 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(131) : ssl verify error: unexpcted error\n");
		if ((verifyDetail & 0x02) != 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(135) : ssl verify error: invalid server cert or CA cert\n");
		if ((verifyDetail & 0x04) != 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(139) : ssl verify error: invalid server hostname\n");
		if ((verifyDetail & 0x08) != 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(143) : ssl verify error: server cert or CA cert had expired\n");
		if ((verifyDetail & 0x10) != 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(147) : ssl verify error: server cert or CA cert isn't validated yet.\n");
		if ((verifyDetail & 0x20) != 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(151) : ssl verify error: unknown CA\n");
		break;
	case SCE_HTTP_ERROR_SSL_OUT_OF_MEMORY:
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(161) : ssl error: out of memory\n");
		break;
	default:
		break;
	}
}

/* log the serial number of one server certificate */
static void printCertSerial(SceSslCert *cert)
{
	const SceUChar8 *serialData;
	SceSize serialLen;
	int ret;
	unsigned int i;

	ret = sceSslGetSerialNumber(cert, &serialData, &serialLen);
	if (ret >= 0) {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(182) : Serial number=");
		for (i = 0; i < serialLen; ++i)
			sceClibPrintf("%02X", serialData[i]);
		sceClibPrintf("\n");
	} else {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
			      "(180) : sceSslGetSerialNumber() returns 0x%x\n", ret);
	}
}

/* SSL handshake verification callback */
static SceInt32 activateSslCallback(SceUInt32 verifyErr, SceSslCert *const sslCert[],
				    SceInt32 certNum, void *userArg)
{
	int i;

	sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(195) : Ssl callback:\n");
	sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(196) : \tbase tmpl[%x]\n",
		      (unsigned int)(uintptr_t)userArg);
	for (i = 0; i < certNum; ++i)
		printCertSerial(sslCert[i]);
	if (verifyErr != 0)
		printSslError(SCE_SSL_ERROR_CERT_VERIFY, verifyErr);
	return 0;
}

/* fetch the 3-byte device certificate over HTTPS */
static int GetCert(void *certOut, int certMaxLen, const char *url)
{
	int ret;
	int tmplId;
	int connId;
	int lockRet;
	int unlockRet;
	int done;
	int statusCode;
	SceInt32 sslErrNum;
	SceUInt32 sslDetail;
	SceUInt64 contentLength;
	unsigned char readBuf[16];

	(void)certMaxLen;
	DTCPIP_ACTIVATE_TRACE(300);
	tmplId = 0;
	connId = 0;
	done = 0;

	do {
		ret = sceHttpCreateTemplate("ActivationTool-DTCP", 1, 1);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(311) : sceHttpCreateTemplate() error: 0x%08X\n", ret);
			break;
		}
		tmplId = ret;

		ret = sceHttpsDisableOption(0x18);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(319) : sceHttpsDisableOption() error: 0x%08X\n", ret);
			break;
		}

		ret = sceHttpsEnableOptionPrivate(tmplId, 256);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(326) : sceHttpsEnableOption() error: 0x%08X\n", ret);
			break;
		}

		ret = sceHttpsSetSslCallback(tmplId, activateSslCallback,
					     (void *)(uintptr_t)tmplId);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(334) : sceHttpsSetSslCallback() error: 0x%08X\n", ret);
			break;
		}

		ret = sceHttpCreateConnectionWithURL(tmplId, url, 1);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(341) : sceHttpCreateConnectionWithURL() error: 0x%08X\n",
				      ret);
			break;
		}
		connId = ret;

		ret = sceHttpCreateRequestWithURL(connId, 0, url, 0);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(348) : sceHttpCreateRequestWithURL() error: 0x%08X\n",
				      ret);
			break;
		}

		printActivateRequestHeader(ret);
		lockRet = ActivateLock();
		if (lockRet < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(356) : Lock() error: 0x%08X\n", lockRet);
			break;
		}
		g_dtcpIpActivateRequest = ret;
		unlockRet = ActivateUnlock();
		if (unlockRet < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(364) : Unlock() error: 0x%08X\n", unlockRet);
			break;
		}
		if (g_dtcpIpActivateCancelled != 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(369) : CANCELED!!\n");
			ret = -1;
			break;
		}

		ret = sceHttpSendRequest(g_dtcpIpActivateRequest, NULL, 0);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(376) : sceHttpSendRequest() error: 0x%08X\n", ret);
			if (ret == SCE_HTTP_ERROR_SSL) {
				int sslRet = sceHttpsGetSslError(g_dtcpIpActivateRequest,
								 &sslErrNum, &sslDetail);
				if (sslRet >= 0)
					printSslError(sslErrNum, sslDetail);
				else
					sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
						      "(380) : sceHttpsGetSslError error: 0x%08X\n",
						      sslRet);
			}
			break;
		}

		printActivateResponseHeader(g_dtcpIpActivateRequest);
		ret = sceHttpGetStatusCode(g_dtcpIpActivateRequest, &statusCode);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(393) : sceHttpGetStatusCode() error: 0x%08X\n", ret);
			break;
		}
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(396) : response code = %d\n", statusCode);
		if (statusCode != 200) {
			ret = -4;
			break;
		}

		ret = sceHttpGetResponseContentLength(g_dtcpIpActivateRequest, &contentLength);
		if (ret >= 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(403) : Content-Length = %lld\n",
				      (long long)contentLength);
			if (contentLength > 3)
				break;
		} else {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(401) : sceHttpGetContentLength() error: 0x%08X\n", ret);
		}

		do {
			ret = sceHttpReadData(g_dtcpIpActivateRequest, readBuf, sizeof(readBuf));
			if (ret < 0)
				break;
			if (ret <= 0 || ret > 3)
				done = 1;
			else
				sceClibMemcpy_safe(certOut, readBuf, (SceSize)ret);
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(420) : \n sceHttpReadData() read %d bytes\n", ret);
		} while (done != 1);
	} while (0);

	lockRet = ActivateLock();
	if (lockRet < 0)
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(432) : Lock() error: 0x%08X\n", lockRet);
	if (g_dtcpIpActivateRequest > 0) {
		int reqId = g_dtcpIpActivateRequest;
		int delRet;

		g_dtcpIpActivateRequest = 0;
		delRet = sceHttpDeleteRequest(reqId);
		if (delRet < 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(440) : sceHttpDeleteRequest() error: 0x%08X\n", delRet);
	}
	unlockRet = ActivateUnlock();
	if (unlockRet < 0)
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(445) : Unlock() error: 0x%08X\n", unlockRet);

	if (connId > 0) {
		unlockRet = sceHttpDeleteConnection(connId);
		if (unlockRet < 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(451) : sceHttpDeleteConnection() error: 0x%08X\n",
				      unlockRet);
	}
	if (tmplId > 0) {
		unlockRet = sceHttpDeleteTemplate(tmplId);
		if (unlockRet < 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(457) : sceHttpDeleteTemplate() error: 0x%08X\n",
				      unlockRet);
	}

	sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
		      "(461) : ret = 0x%08X(%d), tmp = 0x%08X(%d), s_canecled = %d\n",
		      ret, ret, unlockRet, unlockRet, g_dtcpIpActivateCancelled);
	if (ret == 0 && unlockRet != 0) {
		ret = unlockRet;
	} else if (ret != 0 && g_dtcpIpActivateCancelled != 0) {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(466) : CANCELED!!\n");
		ret = -1;
	}
	return ret;
}

/* execute device activation and persist /CONFIG/DTCPIP/devcert */
static int Activate(void)
{
	int ret;
	unsigned int written;
	unsigned short keyId;
	unsigned char cert[4];
	char deviceId[35];
	char url[112];

	DTCPIP_ACTIVATE_TRACE(475);
	ret = GetDeviceUniqueID(deviceId, sizeof(deviceId));
	if (ret >= 0) {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
			      "(490) : GetDeviceUniqueID() deviceID: %s\n", deviceId);
		ret = GetDtcpKeyID(&keyId);
		if (ret >= 0) {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(498) : GetDtcpKeyID() keyid: 0x%04X\n", keyId);
			written = (unsigned int)sceClibSnprintf(
				url, 0x6E, "%s?id=%s?keyid=%04X",
				"https://pspgate1.dnas.playstation.org/activation/vita_dtcpip",
				deviceId, keyId);
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(501) : writen: %d\n", written);
			if (written < 0x6D) {
				ret = -4;
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(525) : _sceDtcpIpActivate() ret = 0x%08X(%d)\n",
					      ret, ret);
				return ret;
			}
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(506) : url: %s\n", url);
			sceClibMemset(cert, 0, sizeof(cert));
			ret = GetCert(cert, 3, url);
			if (ret >= 0) {
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(515) : GetCert() ret : 0x%08X(%d), cert: %02X %02X %02X \n",
					      ret, ret, cert[0], cert[1], cert[2]);
				ret = sceRegMgrSetKeyBin("/CONFIG/DTCPIP", "devcert", cert, 3);
				if (ret < 0)
					sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
						      "(520) : ERROR!! ret=0x%08x(%d)\n",
						      ret, ret);
			} else {
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(512) : ERROR!! ret=0x%08x(%d)\n", ret, ret);
			}
		} else {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(495) : ERROR!! ret=0x%08x(%d)\n", ret, ret);
		}
	} else {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
			      "(487) : ERROR!! ret=0x%08x(%d)\n", ret, ret);
	}
	sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
		      "(525) : _sceDtcpIpActivate() ret = 0x%08X(%d)\n", ret, ret);
	return ret;
}

/* convert an activation result code */
static int ConvertResult(int src)
{
	int dst = 0;

	switch (src) {
	case 0:
	case -1:
	case -2:
	case -3:
	case -5:
	case -6:
	case -7:
		break;
	default:
		dst = src;
		break;
	}
	sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
		      "(547) : ConvertResult() src = 0x%08X(%d), dst = 0x%08X(%d)\n",
		      src, src, dst, dst);
	return dst;
}

/* load HTTPS/SSL/HTTP, run Activate and tear down */
int _sceDtcpIpActivate(int *outConverted)
{
	int result;
	int ret;

	DTCPIP_ACTIVATE_TRACE(570);
	result = 0;
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
	if (ret >= 0) {
		ret = sceSslInit(0x4B000);
		if (ret >= 0) {
			ret = sceHttpInit(0xA000);
			if (ret >= 0) {
				ret = Activate();
				if (ret < 0) {
					sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
						      "(598) : Activate() error: 0x%08X\n", ret);
					result = ret;
				}
				ret = sceHttpTerm();
				if (ret < 0)
					sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
						      "(604) : sceHttpEnd() error: 0x%08X\n", ret);
			} else {
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(591) : sceHttpInit() error: 0x%08X\n", ret);
				result = ret;
			}
			ret = sceSslTerm();
			if (ret < 0)
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(610) : sceSslEnd() error: 0x%08X\n", ret);
		} else {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(584) : sceSslInit() error: 0x%08X\n", ret);
			result = ret;
		}
		ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
		if (ret < 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(616) : sceSysmoduleUnloadModule() error: 0x%08X\n", ret);
	} else {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
			      "(577) : sceSysmoduleLoadModule() error: 0x%08X\n", ret);
		result = ret;
	}
	if (outConverted != NULL)
		*outConverted = ConvertResult(result);
	return result;
}

int _sceDtcpIpActivateInit(void)
{
	g_dtcpIpActivateCancelled = 0;
	return sceKernelCreateLwMutex(&g_dtcpIpActivateMutex, "SceDtcpIpActivateLwMutex", 0, 0,
				      NULL);
}

int _sceDtcpIpActivateTerm(void)
{
	return sceKernelDeleteLwMutex(&g_dtcpIpActivateMutex);
}

/* whether the device certificate is present: the activation's state */
int _sceDtcpIpCheckActivation(int *activated)
{
	unsigned char cert[3];
	int result;
	int i;

	DTCPIP_ACTIVATE_TRACE(660);
	result = 0;
	cert[0] = 0;
	cert[1] = 0;
	cert[2] = 0;
	i = 0;
	if (activated != NULL) {
		result = sceRegMgrGetKeyBin("/CONFIG/DTCPIP", "devcert", cert, 3);
		if (result >= 0) {
			for (i = 0; i < 3; ++i) {
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(679) : cert[%d] = %02d\n", i, (char)cert[i]);
				if (cert[i] != 0)
					break;
			}
			if (i < 3) {
				*activated = 1;
			} else {
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(686) : Need Activate\n");
				*activated = 0;
			}
		} else {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(673) : ERROR!! ret=0x%08x(%d)\n", result, result);
		}
	} else {
		result = -2;
	}
	return result;
}

int _sceDtcpIpCancelActivation(void)
{
	int result;

	DTCPIP_ACTIVATE_TRACE(629);
	result = ActivateLock();
	if (result >= 0) {
		g_dtcpIpActivateCancelled = 1;
		if (g_dtcpIpActivateRequest > 0) {
			int abortResult;

			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(641) : sceHttpAbortRequest()\n");
			abortResult = sceHttpAbortRequest(g_dtcpIpActivateRequest);
			if (abortResult < 0)
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(644) : sceHttpAbortRequest() error: 0x%08X\n",
					      abortResult);
		}
		result = ActivateUnlock();
		if (result < 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(650) : Unlock() error: 0x%08X\n", result);
	} else {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(635) : Lock() error: 0x%08X\n",
			      result);
	}
	return result;
}
