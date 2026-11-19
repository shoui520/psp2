/*
 * MarlinAppLib (vs0:sys/external/libmlnapplib.suprx): the contexts it keeps for the Marlin
 * DRM library (SceMarlin) and what it asks it.
 *
 * No debug record names its files, types or fields; the names are ours, taken from what the
 * code does with them.
 */

#ifndef _MLNAPPLIB_H
#define _MLNAPPLIB_H

#include <scetypes.h>

/* results of this library */
#define MLNAPPLIB_OK			0
#define MLNAPPLIB_ERROR_PARAMETER	1
#define MLNAPPLIB_ERROR_INVALID_ARG	2
#define MLNAPPLIB_ERROR_INVALID_STATE	3
#define MLNAPPLIB_ERROR_NO_LICENSE	4
#define MLNAPPLIB_ERROR_INVALID_CONTENT	5
#define MLNAPPLIB_ERROR_EXPIRED		6
#define MLNAPPLIB_ERROR_BAD_PADDING	7
#define MLNAPPLIB_ERROR_FAILED		8

/* the state of a context (MarlinAppContext.state) */
#define MLNAPPLIB_STATE_CLOSED		0
#define MLNAPPLIB_STATE_OPEN		1
#define MLNAPPLIB_STATE_CONTENT_SET	2
#define MLNAPPLIB_STATE_LICENSED	4
#define MLNAPPLIB_STATE_READY		8

/* the events that move it (the call that reports them) */
#define MLNAPPLIB_EVENT_INIT		2
#define MLNAPPLIB_EVENT_TERM		3
#define MLNAPPLIB_EVENT_OPEN		4
#define MLNAPPLIB_EVENT_CLOSE		5
#define MLNAPPLIB_EVENT_SET_CONTENT	6
#define MLNAPPLIB_EVENT_GET_LICENSE	7
#define MLNAPPLIB_EVENT_CHECK_LICENSE	8
#define MLNAPPLIB_EVENT_DECRYPT		9

#define MLNAPPLIB_NUM_CONTEXTS		2

/* a size and the bytes it counts (SceMarlin's buffer argument) */
typedef struct MarlinBuffer {
	SceUInt32 size;
	const void *pData;
} MarlinBuffer;

/* a date as SceMarlin gives it: milliseconds, not microseconds */
typedef struct MarlinDate {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int millisecond;
} MarlinDate;

/* a date as the library keeps it (its range checks compare as ints) */
typedef struct MarlinDateTime {
	unsigned short year;
	unsigned short month;
	unsigned short day;
	unsigned short hour;
	unsigned short minute;
	unsigned short second;
	int microsecond;
} MarlinDateTime;

/* what a context keeps of a license's validity (written back to the caller whole) */
typedef struct MarlinValidity {
	SceUInt32 flags;		/* 1: not-before, 2: not-after, 4: later not-before, 8: earlier not-after, 0x10: limit */
	MarlinDateTime dates[4];		/* not-before, not-after, later not-before, earlier not-after */
	int limit;
} MarlinValidity;

#define MARLIN_VALIDITY_NOT_BEFORE		0x01
#define MARLIN_VALIDITY_NOT_AFTER		0x02
#define MARLIN_VALIDITY_NOT_BEFORE_LATER	0x04
#define MARLIN_VALIDITY_NOT_AFTER_EARLIER	0x08
#define MARLIN_VALIDITY_LIMIT			0x10

/* SceMarlin's license information (SceMarlin_A2E65A7E) */
typedef struct MarlinLicenseInfo {
	SceUInt8 reserved1[60];
	SceUInt32 flags;		/* 1: not-before, 2: not-after, 8: limit */
	int numNotBefore;
	int numNotAfter;
	const MarlinDate *pNotBefore;
	const MarlinDate *pLaterNotBefore;
	const MarlinDate *pNotAfter;
	const MarlinDate *pEarlierNotAfter;
	SceUInt8 reserved2[8];
	int limit;
} MarlinLicenseInfo;

/* what a control or action says (SceMarlin_92A6A48F's lists point to it) */
typedef struct MarlinControlValue {
	int version;			/* 1 */
	SceUInt32 mask;			/* which of the values below are set: 2, 4, 0x10, 8, 0x20 */
	int copyControl;
	int outputLevel;
	int imageConstraint;
	int hdcpRequired;
	int analogProtection;
} MarlinControlValue;

/* one control a license sets (28 bytes, SceMarlin_92A6A48F's list) */
typedef struct MarlinControlEntry {
	int type;
	SceUInt32 flags;
	int reserved;
	const MarlinControlValue *pValue;
	SceUInt8 reserved2[12];
} MarlinControlEntry;

/* one more (16 bytes) */
typedef struct MarlinActionEntry {
	int type;
	SceUInt32 flags;
	int reserved;
	const MarlinControlValue *pValue;
} MarlinActionEntry;

/* and one more (32 bytes) */
typedef struct MarlinOptionEntry {
	int type;
	SceUInt32 flags;
	SceUInt8 reserved[24];
} MarlinOptionEntry;

/* SceMarlin's rights (SceMarlin_92A6A48F, SceMarlin_F450D554) */
typedef struct MarlinRights {
	int reserved;
	int numControls;
	MarlinControlEntry *pControls;
	int numOptions;
	MarlinOptionEntry *pOptions;
	int numActions;
	MarlinActionEntry *pActions;
	SceUInt8 reserved2[16];
	SceUInt32 flagsA;		/* 1: bound to the service provider, 2, 0x20 */
	SceUInt8 reserved3[12];
	SceUInt32 flagsB;		/* 1, 2, 8 */
} MarlinRights;

/* what a license's controls come to (what the caller reads, 7 words) */
typedef struct MarlinControlResult {
	int copyControl;
	int outputLevel;
	int analogProtection;
	int imageConstraint;
	int hdcpRequired;
	int reserved1;
	int reserved2;
} MarlinControlResult;

/* SceMarlin's key information for a content (SceMarlin_A11B25C7) */
typedef struct MarlinContentInfo {
	const char *pContentId;
	int contentIdSize;
	SceUInt8 reserved1[8];
	const SceUInt8 *pIv;
	const SceUInt8 *pKey;
	int keySize;
	SceUInt8 reserved2[196];
} MarlinContentInfo;

/* what SceMarlin_102593F5 takes to bind a content */
typedef struct MarlinBindParam {
	const void *pContentId;
	int type;
	char flagA;
	char flagB;
	char encrypted;
} MarlinBindParam;

/* one of the two contexts the library keeps (976 bytes) */
typedef struct MarlinAppContext {
	int state;
	int validity;			/* the state the license last put the content in */
	int *pSession;			/* the session in this context (session) */
	int node;			/* the content's node, -1 when none */
	int session;			/* -1 when none */
	int *pReserved;
	int reserved1;
	int reserved2;
	int reserved3;
	void *pLicense;			/* SceMarlin_A2E65A7E's, kept */
	void *pRights;			/* SceMarlin_92A6A48F's, kept */
	char contentIdBuf[512];
	int contentIdBufSize;
	MarlinBuffer contentId;		/* its size and bytes (the 512 above) */
	SceUInt8 key[272];		/* the content type's 16 byte header and its URN */
	SceUInt32 keySize;
	SceUInt8 iv[32];
	SceUInt8 padding[4];
	SceUInt64 notBefore;		/* in minutes */
	SceUInt64 notAfter;
	MarlinValidity validityInfo;
	int *ppSlot;			/* its place in the table of open contexts */
	SceUInt16 isBkdt;		/* 1 for BKDT, 0 for audio */
	SceUInt16 padding2;
} MarlinAppContext;

/* SceMarlin (names are the NIDs: nothing names them) */
extern int SceMarlin_102593F5(int *pSession, const MarlinBindParam *pParam, int *pNode);
extern int SceMarlin_23838449(int *pSession);
extern int SceMarlin_2F1E4047(int *pSession, int node, const void *pInput, SceUInt32 inputSize, SceUInt32 *pOutSize, void *pOut);
extern void SceMarlin_2FD092BD(void *pObject);
extern int SceMarlin_3AA04BBF(int hContext, int a1, int a2, MarlinDate *pNotBefore, MarlinDate *pNotAfter);
extern int SceMarlin_49752AFE(int *pSession);
extern int SceMarlin_83B44D33(int a0, int a1, int a2, int a3);
extern int SceMarlin_8FEB4F94(int hContext);
extern int SceMarlin_92A6A48F(int *pSession, int a1, void **ppRights);
extern int SceMarlin_9CD4CE1E(int *pParam, int *pContext, int a2);
extern int SceMarlin_A11B25C7(const void *pData, int size, MarlinContentInfo *pInfo);
extern int SceMarlin_A2E65A7E(int *pSession, int a1, void **ppLicense);
extern int SceMarlin_A43BC777(int *pSession);
extern int SceMarlin_BCED6CC4(int hContext, const MarlinBuffer *pA, const MarlinBuffer *pB, int **ppResult);
extern int SceMarlin_C76DA6C0(void);
extern int SceMarlin_CCA99803(void *pObject);
extern int SceMarlin_EB4A1345(int *pSession, int node, int a2, SceUInt32 keySize, const void *pKey, int ivSize, const void *pIv);
extern int SceMarlin_F450D554(int *pSession, int a1, void **ppRights);
extern int SceMarlin_FB95B259(int *pSession, int a1, int a2);

#endif /* _MLNAPPLIB_H */
