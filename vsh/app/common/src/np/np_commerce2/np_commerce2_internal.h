/*
 * SceNpCommerce2: the NP commerce client, a library of the PlayStation Store's web API.
 * Sony built it with SNC 3.57. No debug record names its files or functions: the exports'
 * names are the SDK's (np/np_commerce2.h) where it has one, the rest are the NID's; the
 * internal names are ours, taken from what the code does.
 */
#ifndef NP_COMMERCE2_INTERNAL_H
#define NP_COMMERCE2_INTERNAL_H

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
#include <np/np_commerce2.h>
#include <np_common/np_common.h>

/* the libkernel mspace calls the tree has no header of yet */
typedef void *SceClibMspace;
extern void *sceClibMspaceMalloc(SceClibMspace mspace, SceSize size);
extern void sceClibMspaceFree(SceClibMspace mspace, void *ptr);
extern SceClibMspace sceClibMspaceCreate(void *base, SceSize size);
extern int sceClibMspaceDestroy(SceClibMspace mspace);

extern int sceHttpsEnableOptionPrivate(int id, SceUInt32 sslFlags);
extern int sceHttpsDisableOptionPrivate(int id, SceUInt32 sslFlags);

/* the imports SceNpCommerce2 takes from libraries the tree has no header of yet */
extern void SceNpManager_9554802B(const void *data1, const void *data2, const void *data3);

/* the request kinds the Start and GetResult exports of one operation come in */
#define NP_COMMERCE2_VERSION_1		0
#define NP_COMMERCE2_VERSION_2		1
#define NP_COMMERCE2_VERSION_3		2

/* the operations of the store API (the library's strings name them): the type of a request */
enum {
	NP_COMMERCE2_OP_CR_SESS = 1,
	NP_COMMERCE2_OP_DEL_SESS = 2,
	NP_COMMERCE2_OP_GET_CAT = 3,
	NP_COMMERCE2_OP_GET_PROD = 4,
	NP_COMMERCE2_OP_SRCH_PROD = 5,
	NP_COMMERCE2_OP_GET_WAL = 6,
	NP_COMMERCE2_OP_GET_DRM = 7,
	NP_COMMERCE2_OP_PUR_PREV = 8,
	NP_COMMERCE2_OP_PURCH = 9,
	NP_COMMERCE2_OP_GET_C_C = 10,
	NP_COMMERCE2_OP_TOPUP_C_C = 11,
	NP_COMMERCE2_OP_VCHR_RED_PREV = 12,
	NP_COMMERCE2_OP_VCHR_RED = 13,
	NP_COMMERCE2_OP_TOP_EDY = 14,
	NP_COMMERCE2_OP_TOP_EDY_RES = 15,
	NP_COMMERCE2_OP_ADD_CART = 16,
	NP_COMMERCE2_OP_REM_CART = 17,
	NP_COMMERCE2_OP_CLEAR_CART = 18,
	NP_COMMERCE2_OP_GET_CART = 19,
	NP_COMMERCE2_OP_GET_PROV = 20,
	NP_COMMERCE2_OP_GET_PAY = 21,
	NP_COMMERCE2_OP_GET_BILL = 22,
	NP_COMMERCE2_OP_REG_C_C = 23,
	NP_COMMERCE2_OP_GET_ADDR = 24,
	NP_COMMERCE2_OP_GET_RES_ADDR = 25,
	NP_COMMERCE2_OP_EVENT_LOG = 26,
	NP_COMMERCE2_OP_GET_COUNTRIES = 27,
	NP_COMMERCE2_OP_BIND_ACCOUNT = 28,
	NP_COMMERCE2_OP_START_REG = 29,
	NP_COMMERCE2_OP_GET_SEC_Q = 30,
	NP_COMMERCE2_OP_SET_CRED = 31,
	NP_COMMERCE2_OP_SET_LID = 32,
	NP_COMMERCE2_OP_UPD_PASS = 33,
	NP_COMMERCE2_OP_VERIFY_OID = 34,
	NP_COMMERCE2_OP_SET_OID = 35,
	NP_COMMERCE2_OP_GET_NAME = 36,
	NP_COMMERCE2_OP_SET_NAME = 37,
	NP_COMMERCE2_OP_SET_RES_ADDR = 38,
	NP_COMMERCE2_OP_SET_RRN = 39,
	NP_COMMERCE2_OP_GET_OPT_IN = 40,
	NP_COMMERCE2_OP_SET_OPT_IN = 41,
	NP_COMMERCE2_OP_AUTH_MASTER = 42,
	NP_COMMERCE2_OP_GET_BILL_INFO = 43,
	NP_COMMERCE2_OP_SET_SUB_CTRL = 44,
	NP_COMMERCE2_OP_RESET_SUB_PASS = 45,
	NP_COMMERCE2_OP_GET_WAL_OPT = 46,
	NP_COMMERCE2_OP_SET_WAL_OPT = 47,
	NP_COMMERCE2_OP_SUBMIT_REG = 48,
	NP_COMMERCE2_OP_AUTHENTICATE = 49,
	NP_COMMERCE2_OP_REM_PAY = 50,
	NP_COMMERCE2_OP_GET_TRAN_HIST = 51,
	NP_COMMERCE2_OP_GET_TRAN_DETAIL = 52,
	NP_COMMERCE2_OP_GET_SRVS = 53,
	NP_COMMERCE2_OP_GET_SRV_DETAIL = 54,
	NP_COMMERCE2_OP_GET_SUBSCR = 55,
	NP_COMMERCE2_OP_GET_SUBSCR_PLAN = 56,
	NP_COMMERCE2_OP_CHANGE_SUBSCR = 57,
	NP_COMMERCE2_OP_CANCEL_SUBSCR = 58,
	NP_COMMERCE2_OP_GET_CNSL_ACT = 59,
	NP_COMMERCE2_OP_ACPT_EULA = 60,
	NP_COMMERCE2_OP_VERIFY_DOB = 61,
	NP_COMMERCE2_OP_VERIFY_SEC_ANS = 62,
	NP_COMMERCE2_OP_SEND_PASS_RESET_MAIL = 63,
	NP_COMMERCE2_OP_VERIFY_MASTER_BILL = 64,
	NP_COMMERCE2_OP_GET_EPW = 65,
	NP_COMMERCE2_OP_CR_GUEST_SESS = 66,
	NP_COMMERCE2_OP_RESET_PASS = 67,
	NP_COMMERCE2_OP_GET_MASTER_BILL = 68,
	NP_COMMERCE2_OP_SEND_CONSENT_MAIL = 69,
	NP_COMMERCE2_OP_GET_MASTER_RES_ADDR = 70,
	NP_COMMERCE2_OP_GET_PROD_LIST = 71,
	NP_COMMERCE2_OP_GET_SKU_LIST = 72,
	NP_COMMERCE2_OP_GET_REWARD = 73,
	NP_COMMERCE2_OP_SET_SEC_Q = 74,
	NP_COMMERCE2_OP_SUBMIT_UPGRADE = 75,
	NP_COMMERCE2_OP_RECOM_PRODS = 76,
	NP_COMMERCE2_OP_VERIFY_BILL = 77,
	NP_COMMERCE2_OP_REG_DEVICE = 78,
	NP_COMMERCE2_OP_DEREG_DEVICE = 79,
	NP_COMMERCE2_OP_GET_PROD_IDS = 80,
	NP_COMMERCE2_OP_CR_SESS_TICKET = 81,
	NP_COMMERCE2_OP_GET_USERS_FROM_C_I_D = 82,
	NP_COMMERCE2_OP_GET_ACT_CODE = 83,
	NP_COMMERCE2_OP_SET_ACCOUNT_LANG = 84,
	NP_COMMERCE2_OP_GET_PIVOT_FOR_PROD = 85,
	NP_COMMERCE2_OP_GET_PROD_FOR_PIVOT = 86,
	NP_COMMERCE2_OP_CANCEL_ORDER = 87,
	NP_COMMERCE2_OP_CHECK_ACCOUNT_EULA = 88,
	NP_COMMERCE2_OP_USE_ENTLMT = 89,
	NP_COMMERCE2_OP_OP90 = 90,
	NP_COMMERCE2_OP_OP91 = 91,
	NP_COMMERCE2_OP_OP92 = 92,
	NP_COMMERCE2_OP_OP93 = 93,
	NP_COMMERCE2_OP_OP94 = 94,
	NP_COMMERCE2_OP_OP95 = 95,
	NP_COMMERCE2_OP_GET_ENT_STATE = 96,
	NP_COMMERCE2_OP_ESN_DUID = 97,
	NP_COMMERCE2_OP_CR_SESS_EXP_TICKET = 98,
	NP_COMMERCE2_OP_OP99 = 99,
	NP_COMMERCE2_OP_SEND_EMAIL_VER = 100,
	NP_COMMERCE2_OP_HE_REGISTRATION = 101,
	NP_COMMERCE2_OP_GET_ENT_REV = 102,
	NP_COMMERCE2_OP_CR_SESS_BY_TICK_LITE = 103,
	NP_COMMERCE2_OP_CR_SESS_EXP_TICK_LITE = 104,
	NP_COMMERCE2_OP_CR_SESS_BY_ACCESS_TOKEN = 105,
	NP_COMMERCE2_OP_OP106 = 106,
	NP_COMMERCE2_OP_OP107 = 107,
	NP_COMMERCE2_OP_OP108 = 108,
	NP_COMMERCE2_OP_OP109 = 109,
	NP_COMMERCE2_OP_OP110 = 110,
	NP_COMMERCE2_OP_OP111 = 111,
	NP_COMMERCE2_OP_CR_SESS_BY_ACCESS_TOKEN_LITE = 112,
	NP_COMMERCE2_OP_VERIFY_ACCOUNT_PASS = 113,
};

extern int npCommerce2CreateRequest(SceUInt32 ctxId, int type, SceUInt32 *reqId, int version);

/* a request kind the operation does not have */
#define NP_COMMERCE2_ERROR_INVALID_VERSION	-2141909100	/* 0x80550f94 */

/* the request does not name what it is for */
#define NP_COMMERCE2_ERROR_NO_TARGET	-2141908950	/* 0x805510aa */

/* a store check was asked for a target type it does not have */
#define NP_COMMERCE2_ERROR_INVALID_TARGET_TYPE	-2141909098	/* 0x80550f96 */

/* a name did not fit the buffer it was copied to */
#define NP_COMMERCE2_ERROR_BUF_TOO_SMALL	-2141909116	/* 0x80550f84 */

extern int SceNpCommon_F585C86C(void);
extern int SceNpCommon_187C0A8D(int *value);

/* the library's memory: a 8 KiB memory block with an mspace over it */
#define NP_COMMERCE2_HEAP_SIZE		0x2000
#define NP_COMMERCE2_HEAP_MEMTYPE	0x0C20D060

/* what the session result's accessor (export 64E27331) reads out of the response */
typedef struct NpCommerce2SessionData {
	SceNpCommerce2CommonData commonData;
	const char *currencyCode;
	const char *currencySymbol;
	SceUInt16 decimals;
	SceUInt16 padding1;
	SceUInt32 reserved1[3];
	SceUInt32 symbolPosition;
	SceUInt32 symbolWithSpace;
	SceUInt32 reserved2;
	const char *thousandSeparator;
	const char *decimalLetter;
} NpCommerce2SessionData;

extern int g_libInitialized;
extern SceUID g_heapMemBlock;
extern SceClibMspace g_heap;
extern int npCommerce2Initialize(const char *environment, int npCommonValue, int environmentType);
extern int npCommerce2Finalize(void);
extern int npCommerce2InitTable(const char *environment, int npCommonValue, int environmentType);
extern int npCommerce2TermTable(void);
extern SceKernelLwMutexWork g_tableLock;
extern int npCommerce2_8101D9AC(SceKernelLwMutexWork *lock);

/* exports the library's own exports call (commerce2.c) */
extern int SceNpCommerce2_1BFF9C73(int a1, void *a2, int a3);
extern int SceNpCommerce2_64E27331(int a1, int a2, int a3);
extern int SceNpCommerce2_F6A4FC50(int a1, int a2);
extern int SceNpCommerce2_EC09FF89(int a1, int a2);
extern int SceNpCommerce2_9E082DB2(int a1, int a2);
extern int SceNpCommerce2_F988A255(const SceNpCommerce2GetProductInfoResult *result,
				   SceNpCommerce2GameProductInfo *gameProductInfo);
extern int SceNpCommerce2_C4CC8E22(int a1, int a2, int a3);

#define NP_COMMERCE2_ERROR_NO_SHELL_SLOT	-2141909072	/* 0x80550fb0 */
#define NP_COMMERCE2_ERROR_NO_SUCH_SHELL_SLOT	-2141909071	/* 0x80550fb1 */
#define NP_COMMERCE2_SHELL_SLOT_NUM	16

typedef struct NpCommerce2ShellSlot {
	int used;
	int notified;
	int aborted;
	int waiting;
	int type;
	SceUID semaphore;
	int shellRequest;
} NpCommerce2ShellSlot;

extern NpCommerce2ShellSlot g_shellSlots[NP_COMMERCE2_SHELL_SLOT_NUM];
extern int npCommerce2CreateShellSlot(int type, int *slotId);
extern int npCommerce2WaitShellSlot(int slotId);
extern int npCommerce2_8101EAE6(int shell, int request);
extern int g_shellReady;
extern int g_shellLockReady;
extern SceKernelLwMutexWork g_shellLock;
extern int SceShellSvc_D943CE15(int event, int (*callback)(int), int arg);
extern int SceShellSvc_7CBF442B(int event);
extern int npCommerce2_81018A7E(int a1);
extern int npCommerce2_81018A88(int a1);
extern int npCommerce2_81017DCC(void);
extern void npCommerce2_81017EF8(int a1);
extern SceUInt32 g_checkCtxId;
extern SceUInt32 g_checkReqId;
extern int g_checkAborted;
extern int SceShellSvc_B31E7F1C(void);
extern int npCommerce2_8101EB8E(int a1, int a2);
extern int npCommerce2_8101EBDE(int a1);
extern int npCommerce2_81017F7A(void);
extern int npCommerce2_81017F9C(void);
extern int npCommerce2_81017FC2(int a1);

/* Init's settings and the heap (settings.c) */
#define NP_COMMERCE2_ERROR_NO_ENVIRONMENT	-2141909109	/* 0x80550f8b */
#define NP_COMMERCE2_ERROR_ENVIRONMENT_TOO_LONG	-2141909108	/* 0x80550f8c */
extern int g_npCommonValue;
extern int g_environmentType;
extern char g_npEnvironment[17];
extern int npCommerce2SetNpEnvironment(const char *environment);
extern int npCommerce2SetNpCommonValue(int value);
extern int npCommerce2SetEnvironmentType(int type);
extern void *npCommerce2Alloc(SceSize size);
extern void npCommerce2Free(void *ptr);

/* a request of a context: the HTTP request it runs and the shell slot it reports to */
typedef struct NpCommerce2Request {
	struct NpCommerce2Request *next;
	int refCount;
	SceUInt32 id;			/* the context's ID in the high half */
	SceUInt32 flags;		/* NP_COMMERCE2_REQUEST_FLAG_* */
	int httpRequest;
	SceUInt16 type;
	SceUInt16 reserved1;
	int shellSlot;
} NpCommerce2Request;

#define NP_COMMERCE2_REQUEST_FLAG_REMOVED	0x1
#define NP_COMMERCE2_REQUEST_FLAG_ABORTED	0x2

/* a context: the HTTP template and connection its requests run on, and their list */
typedef struct NpCommerce2Context {
	struct NpCommerce2Context *next;
	int refCount;
	SceUInt32 version;
	SceUInt32 id;
	SceUInt32 flags;		/* NP_COMMERCE2_CONTEXT_FLAG_* */
	SceUInt32 reserved1;
	int httpTemplate;
	int httpConnection;
	NpCommerce2Request *requests;
	char text[0x100];
	char response[0x400];
	SceUInt32 responseSize;
	SceUInt32 mode;
	SceUInt32 reserved2;
	SceUInt64 lastRequestTime;
	SceUInt32 kind;			/* 0: the context of the library, 1: the shell's */
	SceUInt32 reserved3;
	char currencyCode[4];
	char countryCode[4];
	SceUInt32 sslCreated;
	SceInt32 sslShortfall;
	SceInt32 httpShortfall;
	SceUInt32 shellSlot;
	SceUInt32 shellCounter;
	SceUInt32 reserved5;
} NpCommerce2Context;

#define NP_COMMERCE2_CONTEXT_FLAG_REMOVED	0x1

/* the request starters (start_request.c) and the senders of each operation */
extern int npCommerce2Send_8101940E(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101950A(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_81019592(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101961A(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3, int x4);
extern int npCommerce2Send_810197A6(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101982E(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_810198B6(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_81019BEC(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101A378(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101A43E(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101A53A(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101A5C2(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101A688(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101A9D6(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3, int x4);
extern int npCommerce2Send_8101AFE4(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101B2A8(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101B3CC(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101B560(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101B684(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101B9B2(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101BA78(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101BBB8(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101BC40(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101BD76(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101C224(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101C2AC(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101C3E2(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101C818(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101C8F4(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101C9F2(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3, int x4);
extern int npCommerce2Send_8101CB3C(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3, int x4);
extern int npCommerce2Send_8101CE02(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101CECC(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101CF56(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest);
extern int npCommerce2Send_8101CFDE(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101D2B0(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);
extern int npCommerce2Send_8101D376(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2, int x3);
extern int npCommerce2Send_8101D4B6(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int x2);

extern int npCommerce2Start_8100673A(SceUInt32 reqId, int a2);
extern int npCommerce2Start_81006880(SceUInt32 reqId, int a2);
extern int npCommerce2Start_810069C0(SceUInt32 reqId);
extern int npCommerce2Start_81006B0E(SceUInt32 reqId, int a2, int a3, int a4);
extern int npCommerce2Start_81006C64(SceUInt32 reqId);
extern int npCommerce2Start_81006DA4(SceUInt32 reqId);
extern int npCommerce2Start_81006F00(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_810071EA(SceUInt32 reqId);
extern int npCommerce2Start_8100732A(SceUInt32 reqId, int a2);
extern int npCommerce2Start_81007470(SceUInt32 reqId, int a2);
extern int npCommerce2Start_810075B6(SceUInt32 reqId);
extern int npCommerce2Start_81007704(SceUInt32 reqId, int a2);
extern int npCommerce2Start_8100784A(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_81007B04(SceUInt32 reqId, int a2, int a3, int a4);
extern int npCommerce2Start_810080AE(SceUInt32 reqId);
extern int npCommerce2Start_81008358(SceUInt32 reqId, int a2);
extern int npCommerce2Start_8100849E(SceUInt32 reqId, int a2, int a3, int a4);
extern int npCommerce2Start_81008606(SceUInt32 reqId, int a2);
extern int npCommerce2Start_8100874C(SceUInt32 reqId);
extern int npCommerce2Start_81008A0A(SceUInt32 reqId, int a2);
extern int npCommerce2Start_81008B50(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_81008C9C(SceUInt32 reqId);
extern int npCommerce2Start_81008DDC(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_81008F28(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_810092EE(SceUInt32 reqId);
extern int npCommerce2Start_8100942E(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_8100957A(SceUInt32 reqId);
extern int npCommerce2Start_8100999A(SceUInt32 reqId, int a2);
extern int npCommerce2Start_81009AE2(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_81009C2E(SceUInt32 reqId, int a2, int a3, int a4);
extern int npCommerce2Start_81009D84(SceUInt32 reqId, int a2, int a3, int a4);
extern int npCommerce2Start_81009EDA(SceUInt32 reqId, int a2);
extern int npCommerce2Start_8100A020(SceUInt32 reqId, int a2);
extern int npCommerce2Start_8100A166(SceUInt32 reqId);
extern int npCommerce2Start_8100A4B6(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_8100A602(SceUInt32 reqId);
extern int npCommerce2Start_8100A83A(SceUInt32 reqId, int a2);
extern int npCommerce2Start_8100A982(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_8100AC38(SceUInt32 reqId, int a2);
extern int npCommerce2Start_8100AD7E(SceUInt32 reqId, int a2, int a3);
extern int npCommerce2Start_8100AECA(SceUInt32 reqId, int a2);
extern int npCommerce2CheckRequest(NpCommerce2Context *ctx, NpCommerce2Request *req);
extern int npCommerce2_81017FE8(int slot, int *counter);
extern int npCommerce2GetSystemTime(SceUInt64 *time);
extern int npCommerce2RenewConnection(int httpTemplate, int *httpConnection, int timeout);
#define NP_COMMERCE2_ERROR_SHELL_CHANGED	-2141909099	/* 0x80550f95 */

/* the header of a response */
typedef struct NpCommerce2ResponseHeader {
	SceUInt32 version;
	SceUInt32 reserved1;
	SceUInt32 length;
	SceUInt32 status;
	char reserved2[0x54];
} NpCommerce2ResponseHeader;

extern int npCommerce2ParseResponseHeader(const char *data, SceSize size, SceUInt32 *version,
					  SceUInt32 *dataLength, SceUInt32 *status);
extern int npCommerce2ResponseHasRecord78(const char *data, SceSize size, int *has);

/* a result or an item of a response: the common part and what its operation reads out */
typedef struct NpCommerce2Item {
	SceNpCommerce2CommonData commonData;
	SceUInt32 value[2];
} NpCommerce2Item;

/* a bigger item: eleven values after the common part */
typedef struct NpCommerce2LargeItem {
	SceNpCommerce2CommonData commonData;
	SceUInt32 value[11];
} NpCommerce2LargeItem;

extern int npCommerce2ReadItem0x15(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2Item *item);
extern int npCommerce2ReadLargeItem0x16(const NpCommerce2Item *result, SceUInt32 index,
					NpCommerce2LargeItem *item);

/* the Init...Result exports and the items read out of a response (response.c) */
extern int npCommerce2InitResult0x78(NpCommerce2Item *result, const char *data, SceSize size);
extern int npCommerce2InitResult0x16(NpCommerce2Item *result, const char *data, SceSize size);
extern int npCommerce2ReadItem0x57(const NpCommerce2Item *result, NpCommerce2Item *item);
extern int npCommerce2ReadItem0x78(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2Item *item);

/* the store's response records (tlv.c) */
#define NP_COMMERCE2_ERROR_RECORD_NOT_FOUND	-2141909113	/* 0x80550f87 */
#define NP_COMMERCE2_ERROR_RECORD_CUT_OFF	-2141909112	/* 0x80550f88 */
#define NP_COMMERCE2_ERROR_RECORD_TOO_SHORT	-2141909111	/* 0x80550f89 */
#define NP_COMMERCE2_ERROR_RECORD_TRUNCATED	-2141909110	/* 0x80550f8a */
extern int npCommerce2FindRecord(const char *data, SceSize size, SceUInt16 tag, SceUInt16 minLength,
				 SceUInt16 *length, int partial);
extern int npCommerce2FindRecordWhole(const char *data, SceSize size, SceUInt16 tag, SceUInt16 minLength,
				      SceUInt16 *length);
#define NP_COMMERCE2_ERROR_RECORD_INDEX		-2141909242	/* 0x80550f06 */
/* what a table record's header holds */
typedef struct NpCommerce2TableHeader {
	SceUInt32 cells;
	SceUInt32 rows;
	SceUInt32 rowSize;
	char reserved[0x20];
} NpCommerce2TableHeader;

extern int npCommerce2FindTableRecord(const char *data, SceSize size, SceUInt16 tag, SceUInt16 *length,
				      NpCommerce2TableHeader *table, int partial);
extern int npCommerce2FindTableRow(const char *data, SceSize size, SceUInt16 tag, SceUInt16 minLength,
				   SceUInt32 index, SceUInt16 *length, int partial);
extern int npCommerce2ReadCategoryInfo(const char *data, SceSize length, const SceUChar8 *bufHead,
				       SceUInt32 version, int dataType, SceNpCommerce2CategoryInfo *info);
/* a result whose records are a table */
typedef struct NpCommerce2TableResult {
	SceNpCommerce2CommonData commonData;
	NpCommerce2TableHeader table;
	char reserved[0x20];
} NpCommerce2TableResult;

extern int npCommerce2InitCategoryContentsResult(NpCommerce2TableResult *result, const char *data, SceSize size);
extern int npCommerce2ReadTableItem(const NpCommerce2TableResult *result, SceUInt32 index, NpCommerce2LargeItem *item);
extern int npCommerce2GetCategoryOfResult(const NpCommerce2Item *result, SceNpCommerce2CategoryInfo *info);
extern int npCommerce2GetCategoryOfContent(const NpCommerce2Item *content, SceNpCommerce2CategoryInfo *info);

/* the product record of a response, read out of its records (product.c) */
typedef struct NpCommerce2ProductItem {
	SceNpCommerce2CommonData commonData;
	const char *productId;
	SceUInt32 skuCount;
	SceUInt32 padding;
	SceUInt64 releaseDate;
	SceUInt32 status;
	SceUInt32 kind;
	SceUInt32 downloadable;
	SceUInt32 price;
	SceUInt32 annotation;
	const char *name;
	const char *shortDescription;
	const char *imageUrl;
	const char *spName;
	SceUInt32 countOfRewards;
	const char *rewards;
	const char *contentRatings;
} NpCommerce2ProductItem;

extern int npCommerce2ReadItem0x28(const NpCommerce2Item *result, NpCommerce2Item *item);
extern int npCommerce2ReadRecord0x11(const NpCommerce2Item *result, SceUInt32 index, const char **record);
extern int npCommerce2ReadProductId(const char *buf, const char **productId, SceUInt32 *skuCount,
				    SceUInt64 *releaseDate);
extern int npCommerce2ReadProductValues(const SceUChar8 *header, const char *buf, const char **productId,
					SceUInt32 *skuCount, SceUInt64 *releaseDate, SceUInt32 *status,
					SceUInt32 *kind, SceUInt32 *downloadable, SceUInt32 *price,
					SceUInt32 *annotation);
extern int npCommerce2ReadProductTexts(const SceNpCommerce2CommonData *item, const char *buf, SceUInt16 length,
				       const char **name, const char **shortDescription,
				       const char **imageUrl, const char **spName);
extern int npCommerce2ReadProductItem(const NpCommerce2Item *result, NpCommerce2ProductItem *item, int kind);
extern int npCommerce2ReadGameProductInfo(const char *data, SceSize length, const SceUChar8 *bufHead,
					  SceUInt32 version, int dataType, SceNpCommerce2GameProductInfo *info);
extern int npCommerce2ReadContentRatingInfo(const char *data, SceSize length, const SceUChar8 *bufHead,
					    SceUInt32 version, SceNpCommerce2ContentRatingInfo *info);
extern int npCommerce2GetGameProductInfoOfItem(const NpCommerce2Item *item, SceNpCommerce2GameProductInfo *info);
extern int npCommerce2GetContentRatingInfoOfItem(const NpCommerce2Item *item,
						 SceNpCommerce2ContentRatingInfo *info);
extern int npCommerce2GetContentRatingInfoAt(const NpCommerce2Item *item, SceUInt32 index,
					     SceNpCommerce2ContentRatingInfo *info);
extern int npCommerce2ReadContentRatingDescriptor(const NpCommerce2Item *info, SceUInt32 index,
						  SceNpCommerce2ContentRatingDescriptor *descriptor);
extern SceUInt64 sceNetNtohll(SceUInt64 net64);
extern int npCommerce2FindNestedRecord(const char *data, SceSize size, SceUInt16 tag, SceUInt16 *length,
				       SceUInt32 *count, int partial);
extern int npCommerce2FindNestedRecordWhole(const char *data, SceSize size, SceUInt16 tag,
					    SceUInt16 *length, SceUInt32 *count);
extern int npCommerce2FindIndexedRecord(const char *data, SceSize size, SceUInt16 tag, SceUInt16 minLength,
					SceUInt32 index, SceUInt16 *length);
extern SceUInt32 sceNetNtohl(SceUInt32 net32);
extern SceUInt16 sceNetNtohs(SceUInt16 net16);

/* the object table (object_table.c, request_table.c) */
extern int npCommerce2AddContext(SceKernelLwMutexWork *lock, NpCommerce2Context *ctx, SceUInt32 *ctxId, int kind);
extern int npCommerce2GetContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId, NpCommerce2Context **ctx, int notRemoved);
extern int npCommerce2ReleaseContext(SceKernelLwMutexWork *lock, NpCommerce2Context *ctx);
extern int npCommerce2UnlinkContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId);
extern int npCommerce2DestroyAllContexts(SceKernelLwMutexWork *lock);
extern int npCommerce2CreateHttpTemplate(int *httpTemplate, int *httpConnection);
extern int npCommerce2DeleteHttpTemplate(int httpTemplate, int httpConnection);
extern int npCommerce2GetRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId, NpCommerce2Request **req);
extern void npCommerce2ReleaseRequest(SceKernelLwMutexWork *lock, NpCommerce2Request *req);
extern int npCommerce2RemoveRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId);
extern int npCommerce2RemoveAllRequests(SceKernelLwMutexWork *lock, SceUInt32 ctxId);
extern int npCommerce2DeleteHttpRequest(int httpRequest);
extern int npCommerce2AbortHttpRequest(int httpRequest);
extern int npCommerce2DeleteShellSlot(int slot);
extern int npCommerce2AbortShellSlot(int slot);
extern NpCommerce2Context *g_contextList;
extern SceUInt32 g_lastContextId;

#define NP_COMMERCE2_ERROR_CTX_MAX_REACHED	SCE_NP_COMMERCE2_ERROR_CTX_MAX

/* set by Init, cleared by Term */
extern int g_initialized;

/* the context and request tables (commerce2.c) */
extern int npCommerce2CreateContext(SceUInt32 version, SceUInt32 *ctxId, int shell);
extern int npCommerce2DestroyContext(SceUInt32 ctxId);
extern int npCommerce2DestroyRequest(SceUInt32 reqId);
extern int npCommerce2AbortRequest(SceUInt32 reqId);
extern int npCommerce2DestroyAllContextsLocked(void);

/* request builders and result readers not understood well enough to name yet; named by address */
extern int npCommerce2_8100D4EC(int a1, void *a2, int a3);
extern int npCommerce2_8100D61A(int a1, int a2, int a3);
extern int npCommerce2_81005E40(int type, SceUInt32 reqId, void *buf, SceSize bufLen,
				SceSize *fillSize);
extern int npCommerce2_810062FA(SceUInt32 reqId, const char *id, int a3, int a4, int a5, int a6);
extern int npCommerce2GetResultData(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize);
extern int npCommerce2_8100DAB0(SceNpCommerce2GetCategoryContentsResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2GetEnvironment(void);
extern int npCommerce2_8100DDCA(const SceNpCommerce2GetCategoryContentsResult *result,
				SceUInt32 index, SceNpCommerce2ContentInfo *contentInfo);
extern int npCommerce2_8100ED8C(int a1, void *a2, int a3);
extern int npCommerce2_8101059C(int a1, int a2, int a3);
extern int npCommerce2_810102FC(int a1, int a2, int a3);
extern int npCommerce2_81010620(int a1, int a2, int a3);
extern int npCommerce2_810100C2(int a1, int a2, int a3);
extern int npCommerce2_8100F1D6(int a1, int a2);
extern char g_shortText[0x39];
extern char g_longText[0xFA1];
extern int npCommerce2CreateReqGetProd(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100647A(SceUInt32 reqId, const char *categoryId, const char *productId,
				int a4, int a5);
extern int npCommerce2_8100F316(SceNpCommerce2GetProductInfoResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2_8100F38C(int a1, int a2);
extern int npCommerce2_8100F416(int a1, void *a2, int a3);
extern int npCommerce2_8100F740(const SceNpCommerce2GetProductInfoResult *result,
				SceNpCommerce2GameProductInfo *gameProductInfo);
extern int npCommerce2_8100F798(int a1, int a2, int a3);
extern int npCommerce2_8100F8D8(int a1, int a2, void *a3, int a4);
extern int npCommerce2_8101144E(int a1, int a2, int a3, int a4);
extern int npCommerce2_810114C6(int a1, int a2, int a3);
extern int npCommerce2_8100FE34(int a1, int a2, int a3);
extern int npCommerce2_8100FE7C(int a1, int a2, int a3);
extern int npCommerce2_8100FF6E(int a1, int a2, int a3);
extern int npCommerce2_81010B14(int a1, int a2);
extern int npCommerce2_81010BEC(int a1, int a2);
extern int npCommerce2_81010DD8(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100FDBC(const SceNpCommerce2GameProductInfo *gameProductInfo, int dataType,
				SceUInt32 idx, SceNpCommerce2GameSkuInfo *gameSkuInfo);
extern int npCommerce2CreateReqAddCart(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810065DA(int a1, int a2, int a3, int a4);
extern int npCommerce2_81010938(int a1, void *a2, int a3);
extern int npCommerce2_81010C98(int a1, int a2, int a3);
extern int npCommerce2_81010FFA(int a1, int a2);
extern int npCommerce2_8101117E(int a1, void *a2, int a3);
extern int npCommerce2_81010710(int a1, int a2, void *a3, int a4);
extern int npCommerce2CreateReqRemCart(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101150E(int a1, void *a2, int a3);
extern int npCommerce2_810115FC(int a1, int a2);
extern int npCommerce2_81011604(int a1, int a2);
extern int npCommerce2CreateReqClearCart(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetCart(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101160C(int a1, void *a2, int a3);
extern int npCommerce2_810116FA(int a1, int a2);
extern int npCommerce2_81011702(int a1, int a2);
extern int npCommerce2CreateReqPurPrev(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101170A(int a1, void *a2, int a3);
extern int npCommerce2_810117FC(int a1, int a2);
extern int npCommerce2_810118B0(int a1, int a2);
extern int npCommerce2_810118A2(int a1, int a2);
extern int npCommerce2CreateReqPurch(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810118B8(int a1, void *a2, int a3);
extern int npCommerce2_810119A0(int a1, int a2);
extern int npCommerce2_810119A8(int a1, int a2);
extern int npCommerce2_810119B0(int a1, int a2);
extern int npCommerce2CreateReqGetCC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810119BE(int a1, void *a2, int a3);
extern int npCommerce2_81011C24(int a1, int a2);
extern int npCommerce2CreateReqGetProv(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81011C2C(int a1, void *a2, int a3);
extern int npCommerce2_81011D84(int a1, int a2, int a3);
extern int npCommerce2_81011D8C(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetPay(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81011E42(int a1, void *a2, int a3);
extern int npCommerce2_81011FC8(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetDrm(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007046(int a1, int a2, int a3, int a4, int a5);
extern int npCommerce2_81011FD0(int a1, void *a2, int a3);
extern int npCommerce2_8101209C(int a1, int a2, int a3);
extern int npCommerce2_8101284E(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetBill(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810128C8(int a1, void *a2, int a3);
extern int npCommerce2CreateReqRegCC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetAddr(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012A5C(int a1, void *a2, int a3);
extern int npCommerce2CreateReqGetWal(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012B60(int a1, void *a2, int a3);
extern int npCommerce2_81012BD6(int a1, int a2);
extern int npCommerce2CreateReqTopupCC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012BDE(int a1, void *a2, int a3);
extern int npCommerce2_81012CE8(int a1, int a2);
extern int npCommerce2CreateReqTopEdy(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012CF0(int a1, void *a2, int a3);
extern int npCommerce2_81012D66(int a1, int a2);
extern int npCommerce2CreateReqTopEdyRes(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqVchrRedPrev(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810079A4(int a1, int a2, int a3, int a4);
extern int npCommerce2_81012E3C(int a1, void *a2, int a3);
extern int npCommerce2_81012F80(int a1, int a2);
extern int npCommerce2_81013012(int a1, int a2);
extern int npCommerce2_8101301A(int a1, int a2, int a3);
extern int npCommerce2_81013362(int a1, void *a2, int a3);
extern int npCommerce2_81013432(int a1, int a2, int a3);
extern int npCommerce2CreateReqVchrRed(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81013952(int a1, void *a2, int a3);
extern int npCommerce2_81013A22(int a1, int a2);
extern int npCommerce2_81013A2A(int a1, int a2);
extern int npCommerce2CreateReqGetResAddr(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81013A32(int a1, void *a2, int a3);
extern int npCommerce2CreateReqEventLog(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqSrchProd(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007C5A(int a1, int a2, int a3, int a4);
extern int npCommerce2_81013BC6(int a1, void *a2, int a3);
extern int npCommerce2_81013C92(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetCountries(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007DC4(SceUInt32 reqId);
extern int npCommerce2_81013D24(int a1, void *a2, int a3);
extern int npCommerce2_81013DAE(int a1, int a2, int a3);
extern int npCommerce2_81013E86(int a1, int a2, int a3);
extern int npCommerce2_81013ECE(int a1, int a2, int a3);
extern int npCommerce2_8101413A(int a1, int a2);
extern int npCommerce2CreateReqBindAccount(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007EDA(int a1, int a2, int a3);
extern int npCommerce2_81013F58(int a1, void *a2, int a3);
extern int npCommerce2_810141C2(int a1, int a2, int a3);
extern int npCommerce2CreateReqStartReg(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014250(int a1, void *a2, int a3);
extern int npCommerce2CreateReqGetSecQ(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810144A0(int a1, void *a2, int a3);
extern int npCommerce2_810145CE(int a1, int a2, int a3);
extern int npCommerce2CreateReqSetCred(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqSetLid(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007FBE(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_810081EE(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2CreateReqUpdPass(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqVerifyAccountPass(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqVerifyOid(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqSetOid(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetName(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101461E(int a1, void *a2, int a3);
extern int npCommerce2_81014694(int a1, int a2);
extern int npCommerce2CreateReqSetName(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqSetResAddr(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqSetRrn(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetOptIn(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100888C(int a1, int a2, int a3, int a4);
extern int npCommerce2_8101478E(int a1, void *a2, int a3);
extern int npCommerce2_81014804(int a1, int a2);
extern int npCommerce2CreateReqSetOptIn(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqAuthMaster(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetBillInfo(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810148A2(int a1, void *a2, int a3);
extern int npCommerce2_81014A92(int a1, int a2, int a3);
extern int npCommerce2_81014A9A(int a1, int a2, int a3);
extern int npCommerce2_81014AA2(int a1, int a2);
extern int npCommerce2CreateReqSetSubCtrl(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009074(int a1, int a2, int a3, int a4);
extern int npCommerce2CreateReqResetSubPass(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810091DE(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetWalOpt(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014AAA(int a1, void *a2, int a3);
extern int npCommerce2_81014B20(int a1, int a2);
extern int npCommerce2CreateReqSetWalOpt(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqSubmitReg(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014BCE(int a1, void *a2, int a3);
extern int npCommerce2CreateReqAuthenticate(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014C44(int a1, void *a2, int a3);
extern int npCommerce2_81014D62(int a1, int a2, int a3);
extern int npCommerce2_81014D6A(int a1, int a2, int a3);
extern int npCommerce2_81014EB4(int a1, int a2, int a3);
extern int npCommerce2_810096BA(int a1, int a2, int a3, int a4);
extern int npCommerce2CreateReqRemPay(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetTranHist(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014F08(int a1, void *a2, int a3);
extern int npCommerce2_81014F92(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetTranDetail(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810151B8(int a1, void *a2, int a3);
extern int npCommerce2_8101522E(int a1, int a2);
extern int npCommerce2_8101545C(int a1, int a2, int a3);
extern int npCommerce2_81015606(int a1, int a2, int a3);
extern int npCommerce2_81015818(int a1, int a2, int a3);
extern int npCommerce2_810097E0(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_8100F2F6(int a1, int a2);
extern int npCommerce2CreateReqGetSrvs(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101591C(int a1, void *a2, int a3);
extern int npCommerce2_810159E8(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetSrvDetail(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81015BCE(int a1, void *a2, int a3);
extern int npCommerce2_81015C9A(int a1, int a2, int a3);
extern int npCommerce2CreateReqGetSubscr(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81015EB4(int a1, void *a2, int a3);
extern int npCommerce2_81015F80(int a1, int a2, int a3);
extern int npCommerce2_810162BA(int a1, int a2, int a3);
extern int npCommerce2CreateReqChangeSubscr(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqCancelSubscr(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetCnslAct(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810163AC(int a1, void *a2, int a3);
extern int npCommerce2_81016436(int a1, int a2, int a3);
extern int npCommerce2CreateReqAcptEula(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqVerifyDob(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101652E(int a1, void *a2, int a3);
extern int npCommerce2_810165A4(int a1, int a2);
extern int npCommerce2CreateReqVerifySecAns(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810165F0(int a1, void *a2, int a3);
extern int npCommerce2_8100A2B4(int a1, int a2, int a3, int a4, int a5);
extern int npCommerce2_8100A38E(int a1, int a2, int a3, int a4);
extern int npCommerce2CreateReqSendPassResetMail(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqVerifyMasterBill(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetEpw(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81016666(int a1, void *a2, int a3);
extern int npCommerce2CreateReqSendEmailVer(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B182(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqCrGuestSess(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqResetPass(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetMasterBill(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101670C(int a1, void *a2, int a3);
extern int npCommerce2_810168FC(int a1, int a2, int a3);
extern int npCommerce2_81016904(int a1, int a2, int a3);
extern int npCommerce2_8101690C(int a1, int a2);
extern int npCommerce2CreateReqSendConsentMail(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A742(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_810169A8(int a1, int a2, void *a3);
extern int npCommerce2_81016914(SceNpCommerce2GetProductInfoListResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2CreateReqGetMasterResAddr(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqGetProdList(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqRecomProds(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100AACE(int a1, int a2, int a3, int a4, SceUInt32 a5, SceUInt32 a6);
extern int npCommerce2_81016A3A(int a1, void *a2, int a3);
extern int npCommerce2CreateReqVerifyBill(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81016B50(int a1, void *a2, int a3);
extern int npCommerce2CreateReqGetProdIds(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81016BC6(int a1, void *a2, int a3);
extern int npCommerce2_81016C50(int a1, int a2, int a3);
extern int npCommerce2CreateReqSetAccountLang(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2CreateReqCancelOrder(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B010(SceUInt32 reqId);
extern int npCommerce2CreateReqCheckAccountEula(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B238(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B3C6(int a1, int a2);
extern int npCommerce2_8100B292(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100B372(int a1, int a2);
extern int npCommerce2_8100B41A(int a1, const char *a2);
extern int npCommerce2_8100B7A0(SceUInt32 ctxId, SceInt32 *shortfall);
extern int npCommerce2_8100B7F4(SceUInt32 ctxId, SceInt32 *shortfall);
extern int npCommerce2_8100D6BC(int a1, void *a2, int a3);
extern int npCommerce2_8100D8E4(int a1, int a2, int a3);
extern int npCommerce2_8100D8EC(int a1, int a2);
extern int npCommerce2_81013FEE(int a1, int a2);
extern int npCommerce2_810142C6(int a1, int a2);
extern int npCommerce2_8100D980(int a1, int a2, int a3);
extern int npCommerce2_810060C0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810060CE(SceUInt32 reqId);
extern int npCommerce2_810062EC(SceUInt32 ctxId, SceUInt32 *reqId);

/* the session requests (CreateSession) */
extern int npCommerce2CreateSessionRequest(SceUInt32 ctxId, SceUInt32 *reqId, int version);
extern int npCommerce2StartSessionRequest(SceUInt32 reqId);
extern int npCommerce2GetResult(int type, SceUInt32 reqId, void *buf, SceSize bufLen,
				SceSize *fillSize);

#endif
