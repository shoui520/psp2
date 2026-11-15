/*
 * SceNpUtility's NP lookup library: the word filter, the profile lookup and the bandwidth
 * test on one HTTP client. Each of its three services (internal, lookup, censor) has its own
 * heap and its own lists of clients, title contexts and transactions, each list named by
 * Sony's mutex ("SceNpLookupClientCensor", "SceNpLookupTitleCensor", "SceNpLookupTransCensor").
 * No debug record names the module's files or functions; the type names come from those
 * mutex names, the rest are ours.
 */
#ifndef NP_UTILITY_INTERNAL_H
#define NP_UTILITY_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np/np_word_filter.h>
#include <np/np_lookup.h>
#include <np/np_bandwidth_test.h>
#include <np_common/np_common.h>
#include "np_utility.h"

/* the imports SceNpUtility takes from libraries the tree has no header of yet */
extern int SceNpManager_96DF1B76(SceNpCommunicationConfig *commConf);
extern void SceNpManager_9554802B(const void *data1, const void *data2, const void *data3);
extern int sceHttpsEnableOptionPrivate(int id, SceUInt32 sslFlags);
extern int sceHttpsDisableOptionPrivate(int id, SceUInt32 sslFlags);
extern int sceHttpSetAcceptEncodingGZIPEnabled(int id, SceBool enable);
extern int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

/* the lookup server answered 404 to a request it names a user by (operation 10) */
#define NP_LOOKUP_ERROR_NOT_FOUND	-2141911160	/* 0x80550788 */

/* a server name and the domain did not fit in a URL of 256 bytes */
#define NP_LOOKUP_ERROR_URL_TOO_LONG	-2141911167	/* 0x80550781 */

/* the largest response body or signed information read */
#define NP_LOOKUP_RESPONSE_MAX_SIZE	0x18000

/* the avatar size of the N sizes (SceNpUtilityAvatarN), next to the SDK's three */
#define NP_LOOKUP_AVATAR_SIZE_N		3

/* the auth callback's argument: the community servers' realm, or none */
#define NP_LOOKUP_AUTH_NONE		1
#define NP_LOOKUP_AUTH_COMMUNITY	2

/* a transaction's operation, the request it runs (0 for none yet) */
#define NP_LOOKUP_OPERATION_PROFILE	1
#define NP_LOOKUP_OPERATION_NP_ID	3
#define NP_LOOKUP_OPERATION_AVATAR	4
#define NP_LOOKUP_OPERATION_PROFILES	10

/* a request's type: its server and path (request.c's table) */
#define NP_LOOKUP_REQUEST_NONE		0
#define NP_LOOKUP_REQUEST_PROFILE	1	/* getprof.<region>.<environment> */
#define NP_LOOKUP_REQUEST_SEARCH_SERVERS 2	/* the searchjid servers' list */
#define NP_LOOKUP_REQUEST_SEARCH_JID	3	/* one of the searchjid servers */
#define NP_LOOKUP_REQUEST_AVATAR	4	/* an avatar's URL */
#define NP_LOOKUP_REQUEST_URL		5
#define NP_LOOKUP_REQUEST_CENSOR	6	/* filter.<region>.<environment> */
#define NP_LOOKUP_REQUEST_SANITIZE	7
#define NP_LOOKUP_REQUEST_PROFILES	8
#define NP_LOOKUP_REQUEST_NUM		9

typedef struct NpLookupRequestType {
	int withUrl;				/* 0x00: the server's name is a URL */
	int needsName;				/* 0x04: the caller names the server */
	const char *scheme;			/* 0x08 */
	int method;				/* 0x0C: SCE_HTTP_METHOD_* */
	const char *host;			/* 0x10: before the server's name */
	const char *path;			/* 0x14 */
	const char *reserved;			/* 0x18: "" in every type, never read */
} NpLookupRequestType;

/* a user's profile as the getprof server describes it */
typedef struct NpLookupProfile {
	SceNpId npId;				/* 0x000 */
	char onlineName[48];			/* 0x024 */
	SceUInt8 reserved1[4];			/* 0x054 */
	SceNpAvatarUrl icon;			/* 0x058 */
	SceNpCountryCode countryCode;		/* 0x0D8 */
	SceNpMyLanguages languages;		/* 0x0DC */
	int iconId;				/* 0x0EC */
	SceNpAboutMe aboutMe;			/* 0x0F0 */
	SceUInt64 color;			/* 0x130 */
	SceUInt8 reserved2[0x90];		/* 0x138 */
} NpLookupProfile;

/* the services, each with its own heap and lists */
#define NP_LOOKUP_SERVICE_INT		0
#define NP_LOOKUP_SERVICE_LOOKUP	1
#define NP_LOOKUP_SERVICE_CENSOR	2
#define NP_LOOKUP_SERVICE_NUM		3

/* a service's heap: where its allocations come from */
#define NP_LOOKUP_HEAP_MSPACE		0	/* an mspace on the caller's memory */
#define NP_LOOKUP_HEAP_MEMBLOCK		1	/* an mspace on a memory block of its own */
#define NP_LOOKUP_HEAP_CALLBACK		2	/* the caller's allocator */
#define NP_LOOKUP_HEAP_NP_COMMON	3	/* SceNpCommon's heap */

typedef struct NpLookupHeap {
	SceUInt32 type;				/* 0x00: NP_LOOKUP_HEAP_* */
	SceUID memblock;			/* 0x04 */
	void *mspace;				/* 0x08 */
	void *(*malloc)(SceSize size);		/* 0x0C */
	void (*free)(void *ptr);		/* 0x10 */
	SceSize count;				/* 0x14: the allocations not freed */
	SceSize total;				/* 0x18: the bytes ever allocated */
} NpLookupHeap;

/* memory a list member owns, freed with it */
typedef struct NpLookupAllocation {
	void *ptr;
	struct NpLookupAllocation *next;
} NpLookupAllocation;

/* a server of a client's list (the request code reads it), each freed by the client */
typedef struct NpLookupServer {
	const char *name;			/* 0x00 */
	SceUInt8 reserved[8];			/* 0x04 */
	struct NpLookupServer *next;		/* 0x0C */
} NpLookupServer;

/* a client: the HTTP template of one NP ID */
typedef struct NpLookupClient {
	int id;					/* 0x00 */
	int refCount;				/* 0x04 */
	int deleted;				/* 0x08 */
	SceNpOnlineId onlineId;			/* 0x0C */
	int httpTemplate;			/* 0x20 */
	SceUID mutex;				/* 0x24 */
	char environment[16];			/* 0x28 */
	char region[8];				/* 0x38: opt[2..3] of the NP ID */
	NpLookupServer *servers;		/* 0x40 */
	SceUInt32 service;			/* 0x44: compared unsigned */
	NpLookupAllocation *allocations;	/* 0x48 */
	struct NpLookupClient *next;		/* 0x4C */
	struct NpLookupClient *prev;		/* 0x50 */
} NpLookupClient;

/* a title context: a communication ID and NP ID on a client */
typedef struct NpLookupTitle {
	struct NpLookupTitle *next;		/* 0x00 */
	struct NpLookupTitle *prev;		/* 0x04 */
	SceNpCommunicationId commId;		/* 0x08 */
	int refCount;				/* 0x14 */
	int deleted;				/* 0x18 */
	SceUInt8 reserved[4];			/* 0x1C */
	int id;					/* 0x20 */
	SceNpId npId;				/* 0x24 */
	int resolveRetry;			/* 0x48 */
	SceUInt32 resolveTimeout;		/* 0x4C */
	SceUInt32 connectTimeout;		/* 0x50 */
	SceUInt32 sendTimeout;			/* 0x54 */
	SceUInt32 recvTimeout;			/* 0x58 */
	int service;				/* 0x5C */
	NpLookupClient *client;			/* 0x60 */
} NpLookupTitle;

/* a transaction's state */
#define NP_LOOKUP_TRANS_IDLE		0	/* made, or back from a failed queueing */
#define NP_LOOKUP_TRANS_QUEUED		1	/* on the worker's queue */
#define NP_LOOKUP_TRANS_DONE		2	/* its result is set */

/* a transaction: one request of a title context */
typedef struct NpLookupTrans {
	int id;					/* 0x00 */
	int refCount;				/* 0x04 */
	int deleted;				/* 0x08 */
	int connection;				/* 0x0C */
	int request;				/* 0x10 */
	SceUInt32 service;			/* 0x14: compared unsigned */
	int aborted;				/* 0x18 */
	NpLookupAllocation *allocations;	/* 0x1C */
	NpLookupTitle *title;			/* 0x20 */
	int resolveRetry;			/* 0x24 */
	SceUInt32 resolveTimeout;		/* 0x28 */
	SceUInt32 connectTimeout;		/* 0x2C */
	SceUInt32 sendTimeout;			/* 0x30 */
	SceUInt32 recvTimeout;			/* 0x34 */
	struct NpLookupTrans *next;		/* 0x38 */
	struct NpLookupTrans *prev;		/* 0x3C */
	int result;				/* 0x40 */
	SceUInt32 state;			/* 0x44: NP_LOOKUP_TRANS_* */
	int operation;				/* 0x48: the request it runs, 0 for none yet */
	void *data;				/* 0x4C */
	SceUInt8 reserved[4];			/* 0x50 */
} NpLookupTrans;

/* a service's list of clients, title contexts or transactions, its IDs from idBase */
typedef struct NpLookupClientList {
	int initialized;			/* 0x00 */
	SceUID mutex;				/* 0x04 */
	const char *name;			/* 0x08 */
	int idBase;				/* 0x0C */
	int idMax;				/* 0x10 */
	NpLookupClient head;			/* 0x14: its next is the first client */
} NpLookupClientList;

typedef struct NpLookupTitleList {
	int initialized;			/* 0x00 */
	SceUID mutex;				/* 0x04 */
	const char *name;			/* 0x08 */
	int idBase;				/* 0x0C */
	int idMax;				/* 0x10 */
	NpLookupTitle head;			/* 0x14: its next is the first title context */
} NpLookupTitleList;

typedef struct NpLookupTransList {
	int initialized;			/* 0x00 */
	SceUID mutex;				/* 0x04 */
	const char *name;			/* 0x08 */
	int idBase;				/* 0x0C */
	int idMax;				/* 0x10 */
	NpLookupTrans head;			/* 0x14: its next is the first transaction */
} NpLookupTransList;

/* a job type the worker runs, and what it calls with the result when the job ends */
typedef struct NpLookupJobHandler {
	int type;
	int (*run)(SceUID thread, int arg, void *job, int flags);
	int (*done)(SceUID thread, int arg, void *job, int flags, int result);
} NpLookupJobHandler;

/* a queued job */
typedef struct NpLookupJob {
	int type;				/* 0x00: its handler's */
	int arg;				/* 0x04 */
	void *job;				/* 0x08 */
	int flags;				/* 0x0C */
} NpLookupJob;

/* the worker's event flag bits */
#define NP_LOOKUP_WORKER_EVENT_JOB	0x1	/* a job was queued */
#define NP_LOOKUP_WORKER_EVENT_STOP	0x2

/* the worker thread of a service's asynchronous requests and its job queue */
typedef struct NpLookupWorker {
	SceUID thread;				/* 0x00 */
	SceUID eventFlag;			/* 0x04 */
	NpLookupJob *queue;			/* 0x08 */
	SceUInt32 queueSize;			/* 0x0C */
	SceUInt32 count;			/* 0x10: the jobs queued */
	void *(*alloc)(SceSize size);		/* 0x14 */
	void (*free)(void *ptr);		/* 0x18 */
	int handlerCount;			/* 0x1C */
	const NpLookupJobHandler *handlers;	/* 0x20 */
	SceUID mutex;				/* 0x24 */
} NpLookupWorker;

/* the transaction ID's service by its ID range, -1 when no service's */
#define NP_LOOKUP_TRANS_SERVICE(id)						\
	(((id) & 0xF00000) == 0x200000 ? NP_LOOKUP_SERVICE_INT :		\
	 ((id) & 0xF0000000) == 0x20000000 ? NP_LOOKUP_SERVICE_LOOKUP :	\
	 ((id) & 0xF000000) == 0x2000000 ? NP_LOOKUP_SERVICE_CENSOR : -1)

/* SCE_NP_UTIL errors of the worker as the community's */
static inline int npLookupWorkerError(int ret)
{
	switch (ret) {
	case SCE_NP_UTIL_ERROR_INVALID_ARGUMENT:
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	case SCE_NP_UTIL_ERROR_INSUFFICIENT:
		return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	case SCE_NP_UTIL_ERROR_UNKNOWN:
		return SCE_NP_COMMUNITY_ERROR_NO_RESOURCE;
	}
	return ret;
}

#endif
