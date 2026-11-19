/*
 * np_sns_facebook_internal.h: SceNpSnsFacebook's private types.
 *
 * The library is an IPMI client of the Facebook service ("SceNpSnsFb"). Each export checks that the
 * client exists, asks the client for a request slot, packs its arguments into IPMI buffers, sends
 * the call (synchronously, or asynchronously and then fetches the result) and unpacks the answer.
 */
#ifndef _NP_SNS_FACEBOOK_INTERNAL_H
#define _NP_SNS_FACEBOOK_INTERNAL_H

#include <scetypes.h>
#include <np/np_error.h>
#include <np/np_sns_facebook.h>

/* the library's own errors; ConvertError() maps them to the SCE_NP_SNS_FACEBOOK_ERROR_ codes */
#define SNSFB_INTERNAL_ERROR_BASE			((int)0x80559400)
#define SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED	((int)0x80559401)
#define SNSFB_INTERNAL_ERROR_NOT_INITIALIZED		((int)0x80559402)
#define SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT		((int)0x80559403)
#define SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY		((int)0x80559404)
#define SNSFB_INTERNAL_ERROR_EXCEEDS_MAX		((int)0x80559405)
#define SNSFB_INTERNAL_ERROR_ABORTED			((int)0x80559406)
#define SNSFB_INTERNAL_ERROR_REQUEST_FAILED		((int)0x80559407)
#define SNSFB_INTERNAL_ERROR_SERVER_ERROR_08		((int)0x80559408)
#define SNSFB_INTERNAL_ERROR_AUTH_ERROR_09		((int)0x80559409)
#define SNSFB_INTERNAL_ERROR_AUTH_ERROR_0A		((int)0x8055940A)
#define SNSFB_INTERNAL_ERROR_SERVER_ERROR_0B		((int)0x8055940B)
#define SNSFB_INTERNAL_ERROR_NOT_SIGNIN			((int)0x8055940C)
#define SNSFB_INTERNAL_ERROR_ACCESS_NOT_ALLOWED		((int)0x8055940E)
#define SNSFB_INTERNAL_ERROR_ACCOUNT_NOT_BOUND		((int)0x8055940F)
#define SNSFB_INTERNAL_ERROR_CONFIG_DISABLED		((int)0x80559410)
#define SNSFB_INTERNAL_ERROR_CANCELED_BY_SYSTEM		((int)0x80559411)
#define SNSFB_INTERNAL_ERROR_CHAT_RESTRICTION		((int)0x80559412)

/* what SceIpmi reports */
#define IPMI_ERROR_REFUSED		((int)0x80020580)
#define IPMI_ERROR_BUSY			((int)0x80020581)
#define IPMI_ERROR_UNAVAILABLE		((int)0x80020583)
#define IPMI_ERROR_NO_RESULT		((int)0x8002050A)
#define IPMI_ERROR_INVALID_ARGUMENT	((int)0x8010171F)
#define IPMI_ERROR_INVALID_ID		((int)0x80101720)
#define IPMI_ERROR_INVALID_REQUEST	((int)0x80101721)
#define IPMI_ERROR_REQUEST_BUSY		((int)0x80101722)
#define IPMI_ERROR_NO_FREE_REQUEST	((int)0x80101723)
#define IPMI_ERROR_ABORTED		((int)0x80101760)

#define SNSFB_MAX_CLIENTS		2
#define SNSFB_MAX_REQUESTS		4
#define SNSFB_SERVICE_NAME		"SceNpSnsFb"
#define SNSFB_MEMORY_NAME		"SceNpSnsFacebook"
#define SNSFB_MEMORY_SIZE		0x1000
#define SNSFB_MEMORY_TYPE		0x0C20D060

/* the service's commands: 0 and 1 open and close the session, then a sync or an async call and its result */
#define SNSFB_CMD_BASE			0x10010000

typedef struct IpmiInBuffer {
	const void *pBuf;
	SceSize size;
} IpmiInBuffer;

typedef struct IpmiOutBuffer {
	void *pBuf;
	SceSize size;
	SceSize *pOutSize;
} IpmiOutBuffer;

typedef struct IpmiClient IpmiClient;

/* SceIpmi's IPMI::Client interface: the *List methods take arrays of buffers */
typedef struct IpmiClientVtbl {
	int (*destroy)(IpmiClient *pClient);
	int (*connect)(IpmiClient *pClient);
	int (*disconnect)(IpmiClient *pClient);
	int (*terminateConnection)(IpmiClient *pClient);
	int (*invokeSyncMethod)(void);
	int (*invokeSyncMethodList)(IpmiClient *pClient, SceUInt32 methodId, const IpmiInBuffer *pIn, SceUInt32 numIn,
		int *pResult, const IpmiOutBuffer *pOut, SceUInt32 numOut);
	int (*invokeAsyncMethod)(void);
	int (*invokeAsyncMethodList)(IpmiClient *pClient, SceUInt32 methodId, const IpmiInBuffer *pIn, SceUInt32 numIn,
		int *pRequestId, const void *pEventFlag);
	int (*tryGetResult)(void);
	int (*tryGetResultList)(IpmiClient *pClient, SceUInt32 methodId, int requestId, int *pResult,
		const IpmiOutBuffer *pOut, SceUInt32 numOut);
} IpmiClientVtbl;

struct IpmiClient {
	const IpmiClientVtbl *pVtbl;
};

typedef struct IpmiCallConfig {
	SceUInt32 count;
	SceUInt32 maxInSize;
	SceUInt32 maxOutSize;
} IpmiCallConfig;

typedef struct IpmiQueueConfig {
	SceUInt32 bufferSize;
	SceUInt32 depth;
} IpmiQueueConfig;

/* IPMI::Client::Config as the library fills it (the fields the library does not set are not initialized) */
typedef struct IpmiClientConfig {
	char serviceName[16];
	SceUInt32 reserved0[3];
	SceUInt32 numThreads;
	IpmiCallConfig syncCall;
	IpmiCallConfig asyncCall;
	IpmiCallConfig resultCall;
	SceUInt32 reserved1;
	SceUInt32 numEventFlags;
	IpmiQueueConfig queues[32];
	SceUInt32 maxClients;
	SceUInt32 reserved2;
} IpmiClientConfig;

/* the SceIpmi client handle and its functions */
typedef struct IpmiHandle IpmiHandle;

extern SceSize SceIpmi_296D44D4(const IpmiClientConfig *pConfig);
extern int SceIpmi_BC3A3031(IpmiHandle **ppHandle, const IpmiClientConfig *pConfig, void *pMemory);
extern int SceIpmi_2C6DB642(IpmiHandle *pHandle);
extern IpmiClient *SceIpmi_1372C7B7(IpmiHandle *pHandle);
extern int SceIpmi_3554D4B0(IpmiHandle *pHandle);
extern int SceIpmi_367CEF82(IpmiHandle *pHandle, int *pRequestId, SceUInt32 flags, void *pReserved);
extern int SceIpmi_48E072C8(IpmiHandle *pHandle, int requestId);
extern int SceIpmi_5ECD40E9(IpmiHandle *pHandle, int option);
extern int SceIpmi_7C002FEC(IpmiHandle *pHandle, int requestId, void *pEvent);
extern int SceIpmi_7E079E19(IpmiHandle *pHandle, void *pEvent, int timeout);
extern int SceIpmi_8A0BAC4E(IpmiHandle *pHandle, int requestId);
extern int SceIpmi_A20BB95B(IpmiHandle *pHandle);

typedef void *SceClibMspace;

/* the room sceClibMspaceMallocStats gets: 32 bytes (the library's mspace record is larger than the 16 bytes it reports) */
typedef struct SnsFbMspaceStats {
	struct MspaceStats stats;
	SceUInt32 reserved[4];
} SnsFbMspaceStats;

/* the access token the service answers with (the SDK's result without its reserved bytes) */
typedef struct SnsFbAccessToken {
	SceUInt64 expiration;
	char accessToken[256];
} SnsFbAccessToken;

typedef void *(*SnsFbAllocFunc)(SceSize size);
typedef void (*SnsFbFreeFunc)(void *ptr);

/* the allocator a client was created with (the library's own, or the application's) */
typedef struct SnsFbAllocator {
	SceInt32 useLibraryHeap;
	SnsFbAllocFunc pAlloc;
	SnsFbFreeFunc pFree;
} SnsFbAllocator;

#endif /* _NP_SNS_FACEBOOK_INTERNAL_H */
