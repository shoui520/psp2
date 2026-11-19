/*
 * SceNpActivityNet's exports (vs0:sys/external/np_activity.suprx): the NP activity feed's
 * network requests, as SceActivityDb calls them. The library's NIDs reproduce with no known
 * name, so the link names are SceNpActivityNet_<NID>; Sony's messages in the library name its
 * calls sceNpActivityNet* (Init, Term, OpenConnection, CloseConnection, CreateContext,
 * DestroyContext, Abort).
 */
#ifndef NP_ACTIVITY_NP_ACTIVITY_H
#define NP_ACTIVITY_NP_ACTIVITY_H

#include <scetypes.h>
#include <np/np_common.h>

/* who wrote a status or a comment (SceNpActivityNetOwner::type) */
#define NP_ACTIVITY_OWNER_USER		0	/* a user, by NP ID */
#define NP_ACTIVITY_OWNER_TITLE		1	/* a title, by its NP title ID */

/* the size of a title ID in an owner */
#define NP_ACTIVITY_TITLE_ID_SIZE	10

typedef struct NpActivityOwner {
	int type;			/* NP_ACTIVITY_OWNER_* */
	union {
		SceNpId npId;
		char titleId[NP_ACTIVITY_TITLE_ID_SIZE];
	};
} NpActivityOwner;

/* what names a status on the network */
typedef struct NpActivityStatusKey {
	NpActivityOwner owner;
	int statusId;
} NpActivityStatusKey;

/* what names a comment on the network: its status's key and its own ID */
typedef struct NpActivityCommentKey {
	NpActivityOwner owner;
	int statusId;
	int commentId;
} NpActivityCommentKey;

/* a status as the network lists it */
typedef struct NpActivityStatus {
	NpActivityOwner owner;
	int statusId;
	int reserved[2];
	int type;
	SceUInt64 creationTime;
	SceUInt64 updateTime;
} NpActivityStatus;

/* the statuses the network lists for one owner */
typedef struct NpActivityStatusList {
	int reserved;
	int count;			/* the statuses it filled */
	NpActivityStatus *statuses;
	int max;			/* the statuses there is room for */
} NpActivityStatusList;

/* which of a list's statuses a request reads: count of them from start (1: the newest) */
typedef struct NpActivityRange {
	int start;
	int count;
} NpActivityRange;

/* a request's handle on the network */
typedef struct NpActivityRequest {
	SceUInt8 reserved[8];
} NpActivityRequest;

#ifdef __cplusplus
extern "C" {
#endif

/* opens and closes the connection to the activity service */
int SceNpActivityNet_F8049E41(int *connection);
int SceNpActivityNet_72346DA7(void);

/* begins and ends a request on the connection; aborts the request running on it */
int SceNpActivityNet_BE4CC2D1(int connection, int mode);
int SceNpActivityNet_F3586055(void);
int SceNpActivityNet_35B49CA4(int connection);

/* a user's online ID (size bytes, with its NUL) from their NP ID, and the reverse */
int SceNpActivityNet_319CB4F4(const SceNpId *npId, char *onlineId, SceSize size);
int SceNpActivityNet_C35984F3(const char *onlineId, SceNpId *npId);

/* deletes a comment; gives and takes back a good job on a status */
int SceNpActivityNet_92CF4044(int connection, int flags, const NpActivityCommentKey *key,
	NpActivityRequest *request);
int SceNpActivityNet_75F6CD31(int connection, int flags, const NpActivityStatusKey *key,
	NpActivityRequest *request);
int SceNpActivityNet_690808DA(int connection, int flags, const NpActivityStatusKey *key,
	NpActivityRequest *request);

int SceNpActivityNet_DA59D2CF(int connection, int flags, int arg);

/* lists the statuses of count titles (NP title IDs), and every title's */
int SceNpActivityNet_40475264(int connection, int flags,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	const NpActivityRange *range, NpActivityStatusList *lists);
int SceNpActivityNet_99C86ADA(int connection, int flags, const NpActivityRange *range,
	NpActivityStatus *statuses, int max, int *count);

/* lists the statuses of count owners, at most *max each */
int SceNpActivityNet_2000C248(int connection, int flags, const NpActivityOwner *owners,
	SceSize count, int *max, NpActivityStatusList *lists);

#ifdef __cplusplus
}
#endif

#endif
