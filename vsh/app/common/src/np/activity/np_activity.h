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

/* a comment as the network lists it */
#define NP_ACTIVITY_COMMENT_MESSAGE_SIZE	824

typedef struct NpActivityComment {
	int commentId;
	int reserved[3];
	NpActivityOwner owner;
	SceUInt64 creationTime;
	char message[NP_ACTIVITY_COMMENT_MESSAGE_SIZE];
} NpActivityComment;

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

/* the library's own errors (no SDK header names them; the names are ours) */
#define NP_ACTIVITY_ERROR_INVALID_ARGUMENT	((int)0x80558901)
#define NP_ACTIVITY_ERROR_INVALID_STATE		((int)0x80558902)
#define NP_ACTIVITY_ERROR_ABORTED		((int)0x80558903)
#define NP_ACTIVITY_ERROR_OUT_OF_MEMORY		((int)0x80558904)
#define NP_ACTIVITY_ERROR_NOT_INITIALIZED	((int)0x80558907)
#define NP_ACTIVITY_ERROR_ALREADY_INITIALIZED	((int)0x80558908)
#define NP_ACTIVITY_ERROR_INVALID_PARAMETER	((int)0x80558923)	/* a component's argument */

/* sets up the library: its heap from the caller's functions, or in the caller's memory block */
int SceNpActivityNet_348268A0(void *(*mallocFunc)(SceSize size), void (*freeFunc)(void *ptr));
int SceNpActivityNet_5C07C7E5(SceSize size, void *memblock);
int SceNpActivityNet_2351D75A(void);
/* *initialized 1 while the library is set up */
int SceNpActivityNet_26B9D4A3(int *initialized);

/* opens and closes the connection to the activity service */
int SceNpActivityNet_F8049E41(int *connection);
int SceNpActivityNet_72346DA7(int connection);

/* begins and ends a request on the connection; aborts the request running on it */
int SceNpActivityNet_BE4CC2D1(int connection, int mode);
int SceNpActivityNet_F3586055(int connection);
int SceNpActivityNet_35B49CA4(int connection);

/* the signed-in user's account setting, or the setting of the user with the NP ID */
int SceNpActivityNet_48B5F8B3(int connection, const SceNpId *npId, void *setting);
int SceNpActivityNet_42CFC0DE(int connection, const SceNpId *npId, const void *setting);

/* a user's online ID (size bytes, with its NUL) from their NP ID, and the reverse */
int SceNpActivityNet_319CB4F4(const SceNpId *npId, char *onlineId, SceSize size);
int SceNpActivityNet_C35984F3(const char *onlineId, SceNpId *npId);

/* deletes a comment; gives and takes back a good job on a status */
int SceNpActivityNet_92CF4044(int connection, int flags, const NpActivityCommentKey *key,
	NpActivityRequest *request);
/* what a request on a status the network no longer has returns */
#define NP_ACTIVITY_ERROR_STATUS_NOT_FOUND	((int)0x80558A65)

/* what giving a good job returns when the user already gave one */
#define NP_ACTIVITY_ERROR_ALREADY_VOTED	((int)0x80558A66)

/* answers nothing names after which SceActivityDb reads the status's detail again, as after
   ALREADY_VOTED: the request found the status in another state than its caller's */
#define NP_ACTIVITY_ERROR_CONFLICT_1	((int)0x80558A67)
#define NP_ACTIVITY_ERROR_CONFLICT_2	((int)0x80558A70)
#define NP_ACTIVITY_ERROR_CONFLICT_3	((int)0x80558A71)

int SceNpActivityNet_75F6CD31(int connection, int flags, const NpActivityStatusKey *key,
	NpActivityRequest *request);
int SceNpActivityNet_690808DA(int connection, int flags, const NpActivityStatusKey *key,
	NpActivityRequest *request);

/* deletes one of the signed-in user's statuses */
int SceNpActivityNet_DA59D2CF(int connection, int flags, int statusId);

/* lists the statuses of count titles (NP title IDs), and every title's */
int SceNpActivityNet_40475264(int connection, int flags,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	const NpActivityRange *range, NpActivityStatusList *lists);
int SceNpActivityNet_99C86ADA(int connection, int flags, const NpActivityRange *range,
	NpActivityStatus *statuses, int max, int *count);

/* lists the statuses of count owners, at most *max each */
int SceNpActivityNet_2000C248(int connection, int flags, const NpActivityOwner *owners,
	SceSize count, int *max, NpActivityStatusList *lists);
/* the same, only the statuses of count titles */
int SceNpActivityNet_363CB63A(int connection, int flags, const NpActivityOwner *owners,
	SceSize count, const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], int *max,
	NpActivityStatusList *lists);

/* what the network added since a time: all 0 when nothing changed */
typedef struct NpActivityUpdates {
	int counts[3];
} NpActivityUpdates;

/* what changed since *since, for count titles (NP title IDs; 0: every title) */
int SceNpActivityNet_C55B9299(int connection, int flags, const SceUInt64 *since,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	NpActivityUpdates *updates);

/* the online ID of the user a status's action URL names */
int SceNpActivityNet_C27E99D3(const char *actionUrl, char *onlineId, SceSize size);

/* the work area and the parser state the detail request reads its reply in */
#define NP_ACTIVITY_DETAIL_WORK_SIZE	3624

typedef struct NpActivityDetailState {
	int state;			/* 0: a new reply */
	int reserved[2];
} NpActivityDetailState;

/*
 * A status's details, as the detail request hands them to its callback. The text fields
 * follow the reply's attributes (titleid, title-name, icon-url, action-url, from-name,
 * avatar-url, panel-url); each is NUL-terminated.
 */
typedef struct NpActivityStatusInfo {
	int reserved[2];
	int type;
	char titleId[12];		/* the NP title ID */
	SceUInt64 creationTime;
	SceUInt64 updateTime;
	char message[1025];
	int numOfComments;
	int numOfVotes;
	char titleName[129];
	char contentId[48];
	char iconUrl[257];
	char actionUrl[1025];
	char reserved2[256];		/* cleared when a status is posted, never read */
	int reserved3;
	char fromName[257];		/* who a title's status is from */
	char avatarUrl[259];
	SceUInt32 panelBgColor;		/* 0: none given */
	char panelUrl[257];
} NpActivityStatusInfo;			/* 3576 bytes */

/* NpActivityStatusInfo::type of a status whose action URL names a user (pspr:show?jid=...) */
#define NP_ACTIVITY_STATUS_TYPE_USER	5

/* reads the details of count statuses into infos */
int SceNpActivityNet_AD0B8FDD(int connection, int flags, const NpActivityStatusKey *keys,
	SceSize count, const int *locale, void *work, NpActivityDetailState *state,
	NpActivityStatusInfo *infos);

/* the online ID of the user a status's action URL names, as SceNpActivityNet_C27E99D3 */
int SceNpActivityNet_FE7F0822(const NpActivityStatusInfo *info, char *onlineId, SceSize size);

typedef struct NpActivityStatusDetail {
	NpActivityStatusKey key;
	int reserved;
	NpActivityStatusInfo info;
} NpActivityStatusDetail;

typedef void (*NpActivityStatusDetailCallback)(const struct NpActivityStatusDetail *detail,
	void *arg);

/* reads the details of count statuses (at most 50), in the locale, one callback each */
int SceNpActivityNet_7E58E9B9(int connection, int flags, const NpActivityStatusKey *keys,
	SceSize count, const int *locale, void *work, NpActivityDetailState *state,
	NpActivityStatusDetailCallback callback, void *arg);

/* what one status's detail request reads: the locale and its comments' and voters' ranges */
typedef struct NpActivityDetailParam {
	int locale;
	NpActivityRange comments;
	NpActivityRange voters;
} NpActivityDetailParam;

/*
 * Reads one status's details, its comments (at most maxComments), the users who gave it a
 * good job (at most maxVoters), and whether the signed-in user gave one (*isVoted 1).
 */
int SceNpActivityNet_7E3116FB(int connection, int flags, const NpActivityStatusKey *key,
	const NpActivityDetailParam *param, NpActivityStatusInfo *info,
	NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
	SceNpId *voters, int maxVoters, SceUInt32 *numVoters);

/* a comment as it is posted */
#define NP_ACTIVITY_COMMENT_POST_MESSAGE_SIZE	561

typedef struct NpActivityCommentPost {
	SceUInt64 creationTime;
	char message[820];		/* at most NP_ACTIVITY_COMMENT_POST_MESSAGE_SIZE bytes */
	int reserved;
} NpActivityCommentPost;

/* a posted comment's request handle */
typedef struct NpActivityCommentRequest {
	SceUInt8 reserved[24];
} NpActivityCommentRequest;

/* posts a comment on a status */
int SceNpActivityNet_747FBB53(int connection, int flags, const NpActivityStatusKey *key,
	const NpActivityCommentPost *comment, NpActivityCommentRequest *request);

/* what a posted status's request returns for it */
#define NP_ACTIVITY_ERROR_POST_REJECTED	((int)0x8055890B)

typedef struct NpActivityPostResult {
	int result;			/* 0, or why the network did not take the status */
	int reserved[3];
} NpActivityPostResult;

/* posts count statuses of the signed-in user */
int SceNpActivityNet_5A0524AD(int connection, int flags, const NpActivityStatusInfo *infos,
	SceSize count, NpActivityPostResult *results);

/* what a request's error means: 0 the request can be sent again later, else it failed for
   good (NP_ACTIVITY_ERROR_KIND_DISCARD: the requests waiting to be sent fail with it) */
#define NP_ACTIVITY_ERROR_KIND_RETRY	0
#define NP_ACTIVITY_ERROR_KIND_DISCARD	2

int SceNpActivityNet_447AC73E(int error);

#ifdef __cplusplus
}
#endif

#endif
