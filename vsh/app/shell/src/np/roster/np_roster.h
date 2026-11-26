/*
 * np_roster.h: SceNpRoster internal declarations (0x813083AC..0x8130C7E7).
 *
 * SceNpRoster implements the HTTPS/XML client for PSN friend-list and block-list
 * endpoints:
 *   - view host:   https://friend-list-view.<region>.<env>.communication.playstation.net
 *   - update host: https://friend-list-update.ww.<env>.communication.playstation.net
 *   - endpoints:   get_friend_list, get_block_list, request_friend, accept_friend,
 *                  delete_friend, update_block_list.
 */
#ifndef SHELL_NP_ROSTER_H
#define SHELL_NP_ROSTER_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <rtc.h>
#include "../imps/np_imps_svc.h"

/* SceNpRoster error codes (0x80550A00, 0x80550B00, 0x80550C00, 0x80558000 ranges) */
#define NP_ROSTER_ERROR_NOT_INITIALIZED		-2141910527	/* 0x80550A01 */
#define NP_ROSTER_ERROR_ALREADY_INITIALIZED	-2141910526	/* 0x80550A02 */
#define NP_ROSTER_ERROR_INVALID_ARGUMENT	-2141910525	/* 0x80550A03 */
#define NP_ROSTER_ERROR_INVALID_ID		-2141910524	/* 0x80550A04 */
#define NP_ROSTER_ERROR_TOO_MANY_REQUESTS	-2141910523	/* 0x80550A05 */
#define NP_ROSTER_ERROR_OUT_OF_MEMORY		-2141910521	/* 0x80550A07 */
#define NP_ROSTER_ERROR_INVALID_RESPONSE	-2141910518	/* 0x80550A0A */
#define NP_ROSTER_ERROR_EMPTY_RESPONSE		-2141910519	/* 0x80550A09 */
#define NP_ROSTER_ERROR_ABORTED			-2141910516	/* 0x80550A0C */
#define NP_ROSTER_ERROR_SERVICE_NOT_STARTED	-2141910515	/* 0x80550A0D */
#define NP_ROSTER_ERROR_SERVICE_MAINTENANCE	-2141910514	/* 0x80550A0E */
#define NP_ROSTER_ERROR_SERVICE_ENDED		-2141910513	/* 0x80550A0F */

#define NP_ROSTER_ERROR_HTTP_BASE		-2141910048	/* 0x80550BE0 */
#define NP_ROSTER_ERROR_HTTP_400		-2141910047	/* 0x80550BE1 */
#define NP_ROSTER_ERROR_HTTP_401		-2141910045	/* 0x80550BE3 */
#define NP_ROSTER_ERROR_HTTP_403		-2141910044	/* 0x80550BE4 */
#define NP_ROSTER_ERROR_HTTP_404		-2141910043	/* 0x80550BE5 */
#define NP_ROSTER_ERROR_HTTP_405		-2141910042	/* 0x80550BE6 */
#define NP_ROSTER_ERROR_HTTP_406		-2141910041	/* 0x80550BE7 */
#define NP_ROSTER_ERROR_HTTP_408		-2141910040	/* 0x80550BE8 */

#define NP_ROSTER_ERROR_INTERNAL_ARG		-2141880063	/* 0x80558001 */
#define NP_ROSTER_ERROR_INTERNAL_STATE		-2141880061	/* 0x80558003 */
#define NP_ROSTER_ERROR_BUFFER_TOO_SMALL	-2141880060	/* 0x80558004 */
#define NP_ROSTER_ERROR_INTERNAL_FAIL		-2141880059	/* 0x80558005 */
#define NP_ROSTER_ERROR_XML_ENCODE		-2141880058	/* 0x80558006 */

/* Option flags for npRosterGetFriendList */
#define NP_ROSTER_FRIEND_LIST_OPT_UPD_DATE	0x01
#define NP_ROSTER_FRIEND_LIST_OPT_MSG_FLAG	0x02

/* Allocator configuration passed to npRosterInit */
struct NpRosterInitParam {
	void *(*allocFn)(SceSize size);
	void (*freeFn)(void *ptr);
};

/* Friend entry returned by npRosterGetFriendList (56 bytes) */
struct NpRosterFriendEntry {
	SceNpId npId;			/* +0x00 (36 bytes) */
	int subscription;		/* +0x24 */
	SceRtcTick updateDate;		/* +0x28 (8 bytes) */
	int msgFlag;			/* +0x30 */
	int reserved;			/* +0x34 (alignment tail to 56 bytes) */
};

/* Public SceNpRoster API (0x813083AC..0x81308B07) */
extern int npRosterInit(const NpRosterInitParam *param);
extern int npRosterTerm(void);
extern int npRosterCreateRequest(void);
extern int npRosterDestroyRequest(int reqId);
extern int npRosterAbortRequest(int reqId);
extern int npRosterRequestFriend(int reqId, const SceNpId *npId, const char *comment);
extern int npRosterAcceptFriend(int reqId, const SceNpId *npId);
extern int npRosterDeleteFriend(int reqId, const SceNpId *npId);
extern int npRosterGetBlockList(int reqId, SceUInt32 startIndex, SceNpId *outEntries,
				SceUInt32 maxCount, SceUInt32 *outTotal);
extern int npRosterUpdateBlockList(int reqId, const NpRosterBlockListEntry *entries,
				   SceSize count);
extern int npRosterGetFriendList(int reqId, int optionFlags, NpRosterFriendEntry *outEntries,
				 SceUInt32 maxCount, SceUInt32 *outTotal);

/* Internal allocator helpers (np_roster_client.cpp) */
extern int npRosterAllocatorInit(const NpRosterInitParam *param);
extern void npRosterAllocatorTerm(void);
extern void *npRosterMalloc(SceSize size);
extern void npRosterFree(void *ptr);

/* Internal XML builder & SAX parser bridge (np_roster_xml.cpp) */
extern const char *npRosterGetEnvironment(void);
extern int npRosterXmlInit(void);
extern void npRosterXmlTerm(void);
extern int npRosterXmlFreeSerialized(char *xmlText);
extern int npRosterXmlParseChunk(const void *chunk, SceSize chunkLen, SceUInt8 isFinal);
extern int npRosterXmlEndParse(void);

extern int npRosterXmlBuildRequestFriend(const SceNpId *npId, const char *comment,
					 const char *ticketBase64, char **outXml,
					 SceSize *outLen);
extern int npRosterXmlBeginParseRequestFriend(int *outResult);

extern int npRosterXmlBuildAcceptFriend(const SceNpId *npId, const char *ticketBase64,
					char **outXml, SceSize *outLen);
extern int npRosterXmlBeginParseAcceptFriend(int *outResult);

extern int npRosterXmlBuildDeleteFriend(const SceNpId *npId, const char *ticketBase64,
					char **outXml, SceSize *outLen);
extern int npRosterXmlBeginParseDeleteFriend(int *outResult);

extern int npRosterXmlBuildGetBlockList(SceUInt32 startIndex, SceUInt32 maxCount,
					const char *ticketBase64, char **outXml,
					SceSize *outLen);
extern int npRosterXmlBeginParseGetBlockList(int *outResult, SceNpId *outEntries,
					     SceUInt32 maxCount, SceUInt32 *outCount,
					     SceUInt32 *outTotal);

extern int npRosterXmlBuildUpdateBlockList(const NpRosterBlockListEntry *entries,
					   SceSize count, const char *ticketBase64,
					   char **outXml, SceSize *outLen);
extern int npRosterXmlBeginParseUpdateBlockList(int *outResult);

extern int npRosterXmlBuildGetFriendList(SceUInt32 maxCount, int optionFlags,
					 const char *ticketBase64, char **outXml,
					 SceSize *outLen);
extern int npRosterXmlBeginParseGetFriendList(int *outResult, NpRosterFriendEntry *outEntries,
					      SceUInt32 maxCount, SceUInt32 *outCount,
					      SceUInt32 *outTotal);

/* External NP auth & service status bridges (0x813268BC..0x8132A378, 0x8133888E) */
struct NpServiceStatusNode {
	SceUInt16 reservedHeader;
	SceUInt16 reservedFlags;
	SceUInt16 reservedSub;
	SceUInt16 serviceIndex;		/* 0 = update service, 1 = view service */
	int statusCode;			/* 1=maint, 2=ended, 3=online, 4=not started */
	NpServiceStatusNode *next;
};

struct NpAuthAccountCache {
	char rawAccountData[0xD8];	/* +0x000 */
	char regionCode[0xF0];		/* +0x0D8 (region string at +0xD8, matches 0x815C4958 - 0x815C4880) */
	SceUInt32 accountFlags;		/* +0x1C8 */
	SceUInt32 reservedTail;		/* +0x1CC */
};

extern int npAuthCoreCreateRequest(void);						/* 0x813268BC */
extern int npAuthCoreDestroyRequest(int reqId);						/* 0x813268DC */
extern int npAuthCoreGetTicket(int reqId, void *ticketBuf, SceSize *inOutSize);		/* 0x81326906 */
extern int npAuthCoreAbortRequest(int reqId);						/* 0x813269E2 */
extern int npAuthCoreGetAccountCache(int reqId, NpAuthAccountCache *outCache,
				     void *outExtra);					/* 0x81326B18 */

extern int npServiceStatusCreateRequest(const NpAuthAccountCache *accountCache,
					int serviceId, const char *reservedArg);	/* 0x8132A252 */
extern int npServiceStatusAbortRequest(int reqId);					/* 0x8132A322 */
extern int npServiceStatusDestroyRequest(int reqId);					/* 0x8132A378 */
extern int npServiceStatusQuery(int reqId, NpServiceStatusNode **outList);		/* 0x8133888E */

#endif
