/*
 * np_auth_core.h: internal SceNpAuth ticket request engine (0x8132475C..0x81325EC3).
 */
#ifndef SHELL_NP_AUTH_CORE_H
#define SHELL_NP_AUTH_CORE_H

#include <stddef.h>
#include <scetypes.h>
#include "../ipc/np_ipc_internal.h"

/* SceNpAuth internal error codes */
#define NP_AUTH_ERR_NOT_INIT			-2141912318	/* 0x80550302 */
#define NP_AUTH_ERR_INVALID_ARG			-2141912317	/* 0x80550303 */
#define NP_AUTH_ERR_ABORTED			-2141912313	/* 0x80550307 */
#define NP_AUTH_ERR_INVALID_SERVICE_ID		-2141912312	/* 0x80550308 */
#define NP_AUTH_ERR_INVALID_CREDENTIAL		-2141912311	/* 0x80550309 */
#define NP_AUTH_ERR_INVALID_ENTITLEMENT		-2141912310	/* 0x8055030A */
#define NP_AUTH_ERR_INVALID_COOKIE		-2141912309	/* 0x8055030B */
#define NP_AUTH_ERR_UNSUPPORTED_VERSION		-2141912308	/* 0x8055030C */
#define NP_AUTH_ERR_INVALID_STATUS		-2141912306	/* 0x8055030E */
#define NP_AUTH_ERR_NO_FREE_ID			-2141912191	/* 0x80550381 */
#define NP_AUTH_ERR_DUPLICATE_PARAM		-2141912190	/* 0x80550382 */
#define NP_AUTH_ERR_PLATFORM_PASSPHRASE		-2141912182	/* 0x8055038A */

/* Ticket version pair passed to auth request creation */
struct NpAuthTicketVersion {
	SceUInt16 major;
	SceUInt16 minor;
};

/* Async request lifecycle callbacks */
typedef void (*NpAuthConnCallback)(int reqId, int connId, void *userArg);
typedef int (*NpAuthResultCallback)(int reqId, int resultOrSize, void *userArg);

/* Lifecycle & request APIs (0x81324786..0x81324D76) */
int npAuthCoreInit(int unusedMode, SceSize stackSize, int threadPriority);
int npAuthCoreGetRequestStatus(int reqId);
int npAuthCoreAbortRequest(int reqId);
int npAuthCoreDestroyRequest(int reqId);
int npAuthCoreStartAsync(int reqId, NpAuthConnCallback connCb,
			 NpAuthResultCallback resultCb, int skipNetInternal,
			 void *userArg);
int npAuthCoreStartAsyncDefault(int reqId, NpAuthConnCallback connCb,
				NpAuthResultCallback resultCb, void *userArg);
int npAuthCoreSetCredentials(int reqId, const char *loginId, const char *password);
int npAuthCoreCreateRequest(int *outReqId, const NpAuthTicketVersion *version,
			    const char *serviceId, SceUInt8 isFirst);
int npAuthCoreCreateRequestWithMiniTicket(int *outReqId, const NpAuthTicketVersion *version,
					  const char *serviceId, SceUInt8 isFirst,
					  SceUInt32 miniTicket);
int npAuthCoreCreateAsyncRequest(const NpAuthInternalParam *param);
int npAuthCoreCreateAsyncRequestEx(const NpAuthInternalParam *param);
int npAuthCoreGetTicket(int reqId, void *outBuf, SceSize bufSize);

#endif /* SHELL_NP_AUTH_CORE_H */
