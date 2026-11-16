/*
 * SceNpBasic: the NP Basic client library. Sony built it with SNC 3.57 in C++; its asserts name
 * npbasic/setup.cpp and npbasic/jga_response.cpp. The exports' names are the SDK's (np/np_basic.h)
 * where it has one; the internal names are ours, taken from what the code does.
 */
#ifndef NPBASIC_INTERNAL_H
#define NPBASIC_INTERNAL_H

#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <np/np_common.h>
#include <np/np_basic.h>

/* SCE_NP_BASIC_ERROR_* (SDK np/np_basic.h), as signed values */
#define NP_BASIC_ERROR_UNKNOWN			-2141905663	/* 0x80551d01 */
#define NP_BASIC_ERROR_INVALID_ARGUMENT		-2141905662	/* 0x80551d02 */
#define NP_BASIC_ERROR_OUT_OF_MEMORY		-2141905661	/* 0x80551d03 */
#define NP_BASIC_ERROR_NOT_INITIALIZED		-2141905660	/* 0x80551d04 */
#define NP_BASIC_ERROR_ALREADY_INITIALIZED	-2141905659	/* 0x80551d05 */
#define NP_BASIC_ERROR_SIGNED_OUT		-2141905658	/* 0x80551d06 */
#define NP_BASIC_ERROR_NOT_ONLINE		-2141905657	/* 0x80551d07 */
#define NP_BASIC_ERROR_DATA_NOT_FOUND		-2141905656	/* 0x80551d08 */
#define NP_BASIC_ERROR_BUSY			-2141905655	/* 0x80551d09 */
#define NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED	-2141905653	/* 0x80551d0b */
#define NP_BASIC_ERROR_NO_NP_COMM_ID_IN_PARAM_SFO	-2141905650	/* 0x80551d0e */

/* the IPMI client the library talks to the NP service through, the library's state */
extern unsigned char g_initialized;		/* 0x81007018 */
extern int g_ipmiClient;		/* 0x81007028 */

/* the NP service's client object (0 when there is none) */
typedef struct NpBasicClient NpBasicClient;

/* the SceIpmi imports (client calls), nameless: the NID is all there is */
#ifdef __cplusplus
extern "C" {
#endif
extern NpBasicClient *SceIpmi_9EE9496B(int client);
#ifdef __cplusplus
}
#endif

/* an IPMI error of the service as the error of the library */
extern int npBasicConvertError(int error);

/* the buffers of an IPMI call: the input ones, and the output ones that the service fills in */
typedef struct NpBasicIpmiIn {
	const void *data;
	SceSize size;
} NpBasicIpmiIn;

typedef struct NpBasicIpmiOut {
	void *data;
	SceSize size;
	SceSize actualSize;
} NpBasicIpmiOut;

/* the client object of the service: slot 5 of its table makes a synchronous call */
typedef struct NpBasicClientVtbl {
	void *slots[5];
	int (*invoke)(NpBasicClient *client, int command, const NpBasicIpmiIn *in, int inNum, int *result,
		NpBasicIpmiOut *out, int outNum);
} NpBasicClientVtbl;

struct NpBasicClient {
	const NpBasicClientVtbl *vtbl;
};

/* the requests the exports make over IPMI (npbasic/ipmi_client.cpp, ours) */
extern int npBasicRegisterHandler(NpBasicClient *client, const void *a2);
extern int npBasicUnregisterHandler(NpBasicClient *client);
extern int npBasicGetFriendListEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetFriendListEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicGetFriendPresence(NpBasicClient *client, const void *a2, void *a3);
extern int npBasicGetBlockListEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetBlockListEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicCheckIfPlayerIsBlocked(NpBasicClient *client, const void *a2, unsigned char *a3, unsigned char *a4);
extern int npBasicRequest_9000D(NpBasicClient *client, const void *a2);
extern int npBasicRequest_9000E(NpBasicClient *client);
extern int npBasicRequest_9000F(NpBasicClient *client, const void *a2);
extern int npBasicRequest_90010(NpBasicClient *client);
extern int npBasicRequest_90011(NpBasicClient *client, const void *a2);
extern int npBasicRequest_90012(NpBasicClient *client);
extern int npBasicRequest_90013(NpBasicClient *client, const void *a2, const void *a3, const void *a4, int a5);
extern int npBasicRequest_90014(NpBasicClient *client, const void *a2, const void *a3, const void *a4, const void *a5);
extern int npBasicRequest_90015(NpBasicClient *client, int a2, void *a3, int *a4);
extern int npBasicRequest_90016(NpBasicClient *client, int a2, void *a3, int a4, void *a5, int a6, int *a7);
extern int npBasicRequest_9002B(NpBasicClient *client);
extern int npBasicGetFriendOnlineStatus(NpBasicClient *client, const void *a2, int *a3);
extern int npBasicGetGameJoiningPresence(NpBasicClient *client, const void *a2, void *a3);
extern int npBasicGetFriendRequestEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetFriendRequestEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicGetRequestedFriendRequestEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetRequestedFriendRequestEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicRequest_90035(NpBasicClient *client, int a2);
extern int npBasicRequest_90036(NpBasicClient *client, int a2);
extern int npBasicRequest_90037(NpBasicClient *client, int a2, const void *a3, int a4);
extern int npBasicGetOwnInfo(NpBasicClient *client, const void *a2, void *a3);
extern int npBasicRequest_9003B(NpBasicClient *client, int a2);
extern int npBasicRequest_9003C(NpBasicClient *client, int *a2, void *a3);
extern int npBasicRequest_9003D(NpBasicClient *client, const void *a2, int *a3);
extern int npBasicRequest_9003E(NpBasicClient *client, const void *a2, int a3, void *a4);

#endif
