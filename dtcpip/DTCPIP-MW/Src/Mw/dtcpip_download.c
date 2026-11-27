/*
 * swagner's parameter-table downloader (DTCPIP-MW/Src/Mw/dtcpip_download.c, from the trace
 * strings) and the network-state checker (SwModNsChecker) that precedes it in the binary:
 * NetCtl Wi-Fi security / connection checks, device unique ID hashing (SHA-256 + HMAC-SHA256),
 * server URL selection from /CONFIG/SYSTEM/UPDATE and /CONFIG/NP, and HTTP key download.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/openpsid.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libnetctl.h>
#include <libkernel/libkernel.h>
#include <sysmodule/sysmodule.h>

#define DTCPIP_DOWNLOAD_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\dtcpip_download.c"
#define DTCPIP_DOWNLOAD_TRACE(line)						\
	sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(" #line ") : TRACE : %s()\n", __FUNCTION__)

#define SCE_SYSMODULE_HTTP			0x0012
#define SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND	(-2143084507)

/* SceVshBridge, SceRegMgr, private HTTP and statically linked SHA-256 / HMAC-SHA256 helpers */
int vshSblAimgrIsCEX(void);
int vshSblAimgrIsDEX(void);
int vshSblAimgrIsTool(void);
int sceRegMgrGetKeyStr(const char *category, const char *name, char *buffer, int size);
int sceHttpRequestGetAllHeaders(int reqId, void *headers, int *headerCount, void *buffer,
				int *bufferSize);
int sceSha256Digest(const void *source, unsigned int size, unsigned char *digest);
int sceHmacSha256BlockInit(void *context, const void *key, unsigned int keyLen);
int sceHmacSha256BlockUpdate(void *context, const void *data, unsigned int dataLen);
int sceHmacSha256BlockResult(void *context, unsigned char *digest);

/* network-state checker cache */
static int g_nsCheckerConnected;
static int g_nsCheckerValidConnection;

/* the download's lock, the request it is waiting on, and the flag its cancel raises */
SceKernelLwMutexWork g_dtcpIpDownloadMutex;
int g_dtcpIpDownloadCancelled;
SceUID g_dtcpIpDownloadRequest;

static const unsigned char g_downloadHmacKey[32] = {
	0xfc, 0x87, 0x34, 0x0b, 0x8f, 0x64, 0x38, 0x97,
	0xe2, 0x42, 0x4b, 0xb8, 0x21, 0x10, 0xca, 0xde,
	0xc1, 0x25, 0x27, 0x78, 0x49, 0xba, 0x66, 0x07,
	0xc9, 0x39, 0xb5, 0x69, 0xd1, 0xc3, 0xb1, 0xd7,
};

static const char *const g_downloadResponseHeaders[13] = {
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

struct SceDownloadHeaderPair {
	const char *name;
	const char *value;
};

/* manager network-state checker init/term hooks */
int SwMwManagerMarkStarted(int *started)
{
	*started = 1;
	return 0;
}

int SwMwManagerStopStarted(int started)
{
	(void)started;
	return 0;
}

/* check whether the current network connection is wired/3G or secured Wi-Fi */
int SwModNsCheckerIsValidConnection(void)
{
	SceNetCtlInfo info;
	int ret;

	printf("SwModNsCheckerIsValidConnection is called\n");
	ret = sceNetCtlInetGetInfo(2, &info);
	if (ret >= 0) {
		if (info.device != 0) {
			printf("This net is wired or 3G\n");
			g_nsCheckerValidConnection = 1;
		} else {
			ret = sceNetCtlInetGetInfo(8, &info);
			if (ret >= 0) {
				if (info.wifi_security != 0) {
					printf("This network is secure Wi-Fi\n");
					g_nsCheckerValidConnection = 1;
				} else {
					printf("This network is no-secure Wi-Fi\n");
					g_nsCheckerValidConnection = 0;
				}
			} else {
				printf("sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_WIFI_SECURITY) is error (%08X)\n",
				       ret);
				g_nsCheckerValidConnection = 0;
			}
		}
	} else {
		printf("sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEVICE) is error (%08X)\n", ret);
		g_nsCheckerValidConnection = 0;
	}
	return g_nsCheckerValidConnection;
}

/* check whether NetCtl has obtained an IP address */
int SwModNsCheckerIsInterfaceConnected(int checker)
{
	int state;

	(void)checker;
	printf("SwModNsCheckerIsInterfaceConnected is called\n");
	sceNetCtlInetGetState(&state);
	if (state > 1) {
		if (state <= 2) {
			printf("SwModNsCheckerIsInterfaceConnected : SCE_NET_CTL_STATE_IPOBTAINING\n");
			g_nsCheckerConnected = 0;
			return g_nsCheckerConnected;
		}
		if (state <= 3) {
			printf("SwModNsCheckerIsInterfaceConnected : SCE_NET_CTL_STATE_IPOBTAINED\n");
			g_nsCheckerConnected = 1;
			return g_nsCheckerConnected;
		}
	} else if (state >= 0) {
		if (state > 0)
			printf("SwModNsCheckerIsInterfaceConnected : SCE_NET_CTL_STATE_CONNECTING\n");
		else
			printf("SwModNsCheckerIsInterfaceConnected : SCE_NET_CTL_STATE_DISCONNECTED\n");
		g_nsCheckerConnected = 0;
		return g_nsCheckerConnected;
	}
	printf("SwModNsCheckerIsInterfaceConnected : Fatal error\n");
	g_nsCheckerConnected = 0;
	return g_nsCheckerConnected;
}

/* override cached network checker flags */
int SwModNsCheckerSetState(int checker, int connected, int validConnection)
{
	(void)checker;
	g_nsCheckerConnected = connected;
	g_nsCheckerValidConnection = validConnection;
	return 0;
}

__attribute__((noinline)) static int DownloadLock(void)
{
	return sceKernelLockLwMutex(&g_dtcpIpDownloadMutex, 1, NULL);
}

__attribute__((noinline)) static int DownloadUnlock(void)
{
	return sceKernelUnlockLwMutex(&g_dtcpIpDownloadMutex, 1);
}

/* log HTTP request headers */
static void printDownloadRequestHeader(int reqId)
{
	struct SceDownloadHeaderPair headers[64];
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

/* log known HTTP response headers */
static void printDownloadResponseHeader(int reqId)
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
							 g_downloadResponseHeaders[i],
							 &fieldValue, &valueLen);
			if (ret == SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND || ret >= 0) {
				if (ret > 0) {
					sceClibStrncpy(valueCopy, fieldValue, valueLen);
					valueCopy[valueLen] = '\0';
					sceClibPrintf("\t%s :%s\n",
						      g_downloadResponseHeaders[i], valueCopy);
				}
			} else {
				sceClibPrintf("%s :\n", g_downloadResponseHeaders[i]);
				sceClibPrintf("sceHttpParseResponseHeader() error: 0x%08X\n", ret);
			}
		}
		sceClibPrintf("*** HTTP Response Header End ***\n\n");
	} else {
		sceClibPrintf("sceHttpGetAllResponseHeaders() error: 0x%08X\n", ret);
	}
}

/* format "<C|D|T>-<64-hex-sha256-of-openpsid>" */
static int GetDownloadDeviceUniqueID(char *buffer, SceSize bufferSize)
{
	char prefix;
	char *cursor;
	int written;
	int total;
	int ret;
	unsigned int i;
	SceKernelOpenPsId openPsId;
	unsigned char digest[32];

	sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(47) : TRACE : %s()\n", "GetDeviceUniqueID");
	cursor = buffer;
	written = 0;
	total = 0;
	prefix = 'T';
	if (buffer == NULL || bufferSize < 0x43)
		return -6;

	if (vshSblAimgrIsCEX() != 0)
		prefix = 'C';
	else if (vshSblAimgrIsDEX() != 0)
		prefix = 'D';
	else if (vshSblAimgrIsTool() != 0)
		prefix = 'T';

	written = sceClibSnprintf(cursor, bufferSize, "%c-", prefix);
	if (written < 2)
		return -7;

	cursor += written;
	total += written;
	ret = sceKernelGetOpenPsId(&openPsId);
	if (ret < 0)
		return ret;

	ret = sceSha256Digest(&openPsId, 16, digest);
	sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(90) : sceSha256Digest() : 0x%08X\n", ret);
	if (ret < 0)
		return ret;

	for (i = 0; i < 32; ++i) {
		written = sceClibSnprintf(cursor, bufferSize - total, "%02X", digest[i]);
		if (written < 2)
			return -7;
		cursor += written;
		total += written;
	}
	*cursor = '\0';
	return 0;
}

/* compute hex HMAC-SHA256 over (deviceId, keyId) */
static int computeDownloadHash(const char *deviceId, int deviceIdLen, unsigned short keyId,
			       char *hashOut, SceSize hashOutSize)
{
	int ret;
	char *cursor;
	SceSize remaining;
	unsigned int i;
	int written;
	unsigned short keyIdLocal;
	unsigned char digest[32];
	unsigned char hmacKey[32];
	unsigned char hmacContext[208];

	keyIdLocal = keyId;
	if (hashOut == NULL || hashOutSize < 0x41)
		return -6;

	memcpy(hmacKey, g_downloadHmacKey, sizeof(hmacKey));
	ret = sceHmacSha256BlockInit(hmacContext, hmacKey, sizeof(hmacKey));
	if (ret < 0) {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(298) : sceHmacSha256BlockInit() error: 0x%08X\n", ret);
		return ret;
	}
	ret = sceHmacSha256BlockUpdate(hmacContext, deviceId, (unsigned int)deviceIdLen);
	if (ret < 0) {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(304) : sceHmacSha256BlockUpdate() error: 0x%08X\n", ret);
		return ret;
	}
	ret = sceHmacSha256BlockUpdate(hmacContext, &keyIdLocal, sizeof(keyIdLocal));
	if (ret < 0) {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(310) : sceHmacSha256BlockUpdate() error: 0x%08X\n", ret);
		return ret;
	}
	ret = sceHmacSha256BlockResult(hmacContext, digest);
	if (ret < 0) {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(316) : sceHmacSha256BlockResult() error: 0x%08X\n", ret);
		return ret;
	}

	cursor = hashOut;
	remaining = hashOutSize;
	for (i = 0; i < 32; ++i) {
		written = sceClibSnprintf(cursor, remaining, "%02X", digest[i]);
		if (written < 2)
			return -7;
		cursor += written;
		remaining -= (SceSize)written;
	}
	*cursor = '\0';
	return 0;
}

/* resolve the parameter-table server URL from registry or NP environment */
static int getDownloadServerUrl(char *urlOut, SceSize urlOutSize, int *outLen)
{
	int ret;
	unsigned int written;
	const char *envName;
	char prefix[4];
	char npEnv[17];
	char serverUrl[1024];

	serverUrl[0] = '\0';
	ret = sceRegMgrGetKeyStr("/CONFIG/SYSTEM/UPDATE", "server_url", serverUrl, 1024);
	if (ret != 0 && serverUrl[0] == '\0') {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(355) : sceRegMgrGetKeyStr(/CONFIG/SYSTEM/UPDATE/server_url) failed: %x\n",
			      ret);
	} else {
		SceSize urlLen;
		int copied;

		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(348) : sceRegMgrGetKeyStr(/CONFIG/SYSTEM/UPDATE/server_url) OK: %s\n",
			      serverUrl);
		urlLen = sceClibStrnlen(serverUrl, 0x400);
		copied = (int)sceClibStrlcpyChk(urlOut, serverUrl, urlLen + 1, urlOutSize);
		if ((unsigned int)copied < 0x400 && copied > 0) {
			*outLen = copied;
			return 0;
		}
	}

	npEnv[0] = '\0';
	envName = "np";
	ret = sceRegMgrGetKeyStr("/CONFIG/NP/", "env", npEnv, 17);
	if (ret == 0 && npEnv[0] != '\0') {
		SceSize prefixLen;

		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(367) : sceRegMgrGetKeyStr(/CONFIG/NP/env) OK: %s\n", npEnv);
		memcpy(prefix, "e1-", 4);
		prefixLen = sceClibStrnlen(prefix, 4);
		if (sceClibStrncmp(npEnv, prefix, prefixLen) == 0)
			envName = "e1-np";
	} else {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(365) : sceRegMgrGetKeyStr(/CONFIG/NP/env) failed: %x\n", ret);
	}

	written = (unsigned int)sceClibSnprintf(
		urlOut, urlOutSize,
		"http://ps4-system.sec.%s.dl.playstation.net/ps4-system/vitadtcpip/%s/v00/vita-param_tbl.env",
		envName, envName);
	if (written < 4)
		return -7;
	*outLen = (int)written;
	return 0;
}

/* fetch the parameter table payload over HTTP */
static int GetKey(char *bufferOut, unsigned int *sizeInOut, const char *url)
{
	int ret;
	int tmplId;
	int connId;
	int lockRet;
	int unlockRet;
	int done;
	unsigned int remaining;
	int statusCode;
	SceUInt64 contentLength;
	unsigned char chunk[128];

	DTCPIP_DOWNLOAD_TRACE(121);
	tmplId = 0;
	connId = 0;
	done = 0;

	do {
		ret = sceHttpCreateTemplate("DownloadTool-DTCP", 1, 1);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(131) : sceHttpCreateTemplate() error: 0x%08X\n", ret);
			break;
		}
		tmplId = ret;

		ret = sceHttpCreateConnectionWithURL(tmplId, url, 1);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(138) : sceHttpCreateConnectionWithURL() error: 0x%08X\n",
				      ret);
			break;
		}
		connId = ret;

		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(143) : retry: 3, timeout: 3.38sec.\n");
		ret = sceHttpSetResolveRetry(connId, 3);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(147) : sceHttpSetResolveRetry() error: 0x%08X\n", ret);
			break;
		}

		ret = sceHttpSetResolveTimeOut(connId, 0x339320);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(153) : sceHttpSetResolveTimeOut() error: 0x%08X\n", ret);
			break;
		}

		ret = sceHttpCreateRequestWithURL(connId, 0, url, 0);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(159) : sceHttpCreateRequestWithURL() error: 0x%08X\n",
				      ret);
			break;
		}

		printDownloadRequestHeader(ret);
		lockRet = DownloadLock();
		if (lockRet < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(167) : Lock() error: 0x%08X\n", lockRet);
			break;
		}
		g_dtcpIpDownloadRequest = ret;
		unlockRet = DownloadUnlock();
		if (unlockRet < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(175) : Unlock() error: 0x%08X\n", unlockRet);
			break;
		}
		if (g_dtcpIpDownloadCancelled != 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(180) : CANCELED!!\n");
			ret = -5;
			break;
		}

		ret = sceHttpSendRequest(g_dtcpIpDownloadRequest, NULL, 0);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(186) : sceHttpSendRequest() error: 0x%08X\n", ret);
			break;
		}

		printDownloadResponseHeader(g_dtcpIpDownloadRequest);
		ret = sceHttpGetStatusCode(g_dtcpIpDownloadRequest, &statusCode);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(195) : sceHttpGetStatusCode() error: 0x%08X\n", ret);
			break;
		}
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(198) : response code = %d\n", statusCode);
		if (statusCode != 200) {
			ret = -8;
			*sizeInOut = (unsigned int)contentLength;
			break;
		}

		ret = sceHttpGetResponseContentLength(g_dtcpIpDownloadRequest, &contentLength);
		if (ret >= 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(205) : Content-Length = %lld\n",
				      (long long)contentLength);
			if (contentLength > *sizeInOut) {
				ret = -7;
				break;
			}
		} else {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(203) : sceHttpGetContentLength() error: 0x%08X\n", ret);
		}

		remaining = *sizeInOut;
		for (;;) {
			ret = sceHttpReadData(g_dtcpIpDownloadRequest, chunk, sizeof(chunk));
			if (ret < 0)
				break;
			if (ret <= 0 || (unsigned int)ret > remaining) {
				done = 1;
			} else {
				sceClibMemcpy_safe(bufferOut, chunk, (SceSize)ret);
				bufferOut += ret;
				remaining -= (unsigned int)ret;
			}
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(225) : \n sceHttpReadData() read %d bytes\n", ret);
			if (done == 1) {
				*sizeInOut = (unsigned int)contentLength;
				break;
			}
		}
	} while (0);

	lockRet = DownloadLock();
	if (lockRet < 0)
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(237) : Lock() error: 0x%08X\n", lockRet);
	if (g_dtcpIpDownloadRequest > 0) {
		int reqId = g_dtcpIpDownloadRequest;
		int delRet;

		g_dtcpIpDownloadRequest = 0;
		delRet = sceHttpDeleteRequest(reqId);
		if (delRet < 0)
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(245) : sceHttpDeleteRequest() error: 0x%08X\n", delRet);
	}
	unlockRet = DownloadUnlock();
	if (unlockRet < 0)
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(250) : Unlock() error: 0x%08X\n", unlockRet);

	if (connId > 0) {
		unlockRet = sceHttpDeleteConnection(connId);
		if (unlockRet < 0)
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(256) : sceHttpDeleteConnection() error: 0x%08X\n",
				      unlockRet);
	}
	if (tmplId > 0) {
		unlockRet = sceHttpDeleteTemplate(tmplId);
		if (unlockRet < 0)
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(262) : sceHttpDeleteTemplate() error: 0x%08X\n",
				      unlockRet);
	}

	sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
		      "(266) : ret = 0x%08X(%d), tmp = 0x%08X(%d), s_canecled = %d\n",
		      ret, ret, unlockRet, unlockRet, g_dtcpIpDownloadCancelled);
	if (ret < 0 && g_dtcpIpDownloadCancelled != 0) {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(271) : CANCELED!!\n");
		ret = -5;
	}
	return ret;
}

/* assemble the download query string and fetch the parameter table */
static int download(unsigned short keyId, char *bufferOut, unsigned int *sizeInOut)
{
	int ret;
	int baseLen;
	char hashHex[65];
	char deviceId[67];
	char url[276];

	DTCPIP_DOWNLOAD_TRACE(386);
	baseLen = 0;
	ret = GetDownloadDeviceUniqueID(deviceId, sizeof(deviceId));
	if (ret >= 0) {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(402) : GetDeviceUniqueID() deviceID: %s\n", deviceId);
		ret = computeDownloadHash(deviceId, sizeof(deviceId), keyId,
					  hashHex, sizeof(hashHex));
		if (ret >= 0) {
			ret = getDownloadServerUrl(url, 0x111, &baseLen);
			if (ret >= 0) {
				if ((unsigned int)sceClibSnprintf(
					    &url[baseLen], (SceSize)(273 - baseLen),
					    "?id=%s?keyid=%04X&hash=%s",
					    deviceId, keyId, hashHex) > 0x110) {
					ret = -7;
					sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
						      "(437) : download() ret = 0x%08X(%d)\n",
						      ret, ret);
					return ret;
				}
				sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(426) : url: %s\n", url);
				ret = GetKey(bufferOut, sizeInOut, url);
				if (ret >= 0)
					sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
						      "(434) : GetKey() ret : 0x%08X(%d), size = %u\n",
						      ret, ret, *sizeInOut);
				else
					sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
						      "(431) : ERROR!! ret=0x%08x(%d)\n", ret, ret);
			} else {
				sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
					      "(414) : ERROR!! ret=0x%08x(%d)\n", ret, ret);
			}
		} else {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(407) : ERROR!! ret=0x%08x(%d)\n", ret, ret);
		}
	} else {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(399) : ERROR!! ret=0x%08x(%d)\n", ret, ret);
	}
	sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(437) : download() ret = 0x%08X(%d)\n", ret, ret);
	return ret;
}

/* load HTTP, download the parameter table, and unload HTTP */
int _sceDtcpIpDownload(unsigned short keyId, char *bufferOut, unsigned int *sizeInOut)
{
	int result;
	int ret;

	DTCPIP_DOWNLOAD_TRACE(459);
	result = 0;
	if (bufferOut == NULL || sizeInOut == NULL || *sizeInOut == 0)
		return -6;

	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
	if (ret < 0) {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(470) : sceSysmoduleLoadModule() error: 0x%08X\n", ret);
		return ret;
	}

	ret = sceHttpInit(0xA000);
	if (ret >= 0) {
		ret = download(keyId, bufferOut, sizeInOut);
		if (ret < 0) {
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(484) : Activate() error: 0x%08X\n", ret);
			result = ret;
		}
		ret = sceHttpTerm();
		if (ret < 0)
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(490) : sceHttpEnd() error: 0x%08X\n", ret);
	} else {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(477) : sceHttpInit() error: 0x%08X\n", ret);
		result = ret;
	}

	ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
	if (ret < 0)
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(496) : sceSysmoduleUnloadModule() error: 0x%08X\n", ret);
	return result;
}

int _sceDtcpIpDownloadInit(void)
{
	g_dtcpIpDownloadCancelled = 0;
	return sceKernelCreateLwMutex(&g_dtcpIpDownloadMutex, "SceDtcpIpDownloadLwMutex", 0, 0,
				      NULL);
}

int _sceDtcpIpDownloadTerm(void)
{
	int result;

	DTCPIP_DOWNLOAD_TRACE(536);
	result = sceKernelDeleteLwMutex(&g_dtcpIpDownloadMutex);
	if (result < 0)
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(542) : sceKernelDeleteLwMutex() error: 0x%08X\n", result);
	return result;
}

int _sceDtcpIpCancelDownload(void)
{
	int result;

	DTCPIP_DOWNLOAD_TRACE(506);
	result = DownloadLock();
	if (result >= 0) {
		g_dtcpIpDownloadCancelled = 1;
		if (g_dtcpIpDownloadRequest > 0) {
			int abortResult;

			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(518) : sceHttpAbortRequest()\n");
			abortResult = sceHttpAbortRequest(g_dtcpIpDownloadRequest);
			if (abortResult < 0)
				sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
					      "(521) : sceHttpAbortRequest() error: 0x%08X\n",
					      abortResult);
		}
		result = DownloadUnlock();
		if (result < 0)
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(527) : Unlock() error: 0x%08X\n", result);
	} else {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(512) : Lock() error: 0x%08X\n",
			      result);
	}
	return result;
}
