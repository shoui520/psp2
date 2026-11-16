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
#define NP_BASIC_ERROR_DATA_NOT_FOUND		-2141905656	/* 0x80551d08 */
#define NP_BASIC_ERROR_BUSY			-2141905655	/* 0x80551d09 */
#define NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED	-2141905653	/* 0x80551d0b */

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

/* the requests the exports make over IPMI (npbasic/ipmi_client.cpp, ours) */
extern int npBasicGetFriendOnlineStatus(int a1, int a2, int a3);
extern int npBasicGetFriendContextState(int a1, int a2, int a3);
extern int npBasicGetFriendListEntryCount(NpBasicClient *client, int a1, unsigned char *found);
extern int npBasicGetFriendListEntries(NpBasicClient *client, unsigned int a1, int a2, int a3, int a4, unsigned char *found);
extern int npBasicGetFriendRequestEntryCount(NpBasicClient *client, int a1, unsigned char *found);
extern int npBasicGetFriendRequestEntries(NpBasicClient *client, unsigned int a1, int a2, int a3, int a4, unsigned char *found);
extern int npBasicGetRequestedFriendRequestEntryCount(NpBasicClient *client, int a1, unsigned char *found);
extern int npBasicGetRequestedFriendRequestEntries(NpBasicClient *client, unsigned int a1, int a2, int a3, int a4,
	unsigned char *found);
extern int npBasicGetBlockListEntryCount(NpBasicClient *client, int a1, unsigned char *found);
extern int npBasicGetBlockListEntries(NpBasicClient *client, unsigned int a1, int a2, int a3, int a4, unsigned char *found);
extern int npBasicCheckIfPlayerIsBlocked(int a1, int a2, int a3, int a4);
extern int npBasicGetGameJoiningPresence(int a1, int a2, int a3);

#endif
