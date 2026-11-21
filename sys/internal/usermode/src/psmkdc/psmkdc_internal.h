/*
 * SceDrmPsmKdc's own types, data and helpers. No debug record names them; the names are ours,
 * taken from what the code does, after np_kdc's (SceNpKdc), whose request code this module's
 * copies.
 */
#ifndef _PSMKDC_INTERNAL_H
#define _PSMKDC_INTERNAL_H

#include <scetypes.h>
#include <npdrm/psmdrm.h>

/* Sony's name for the one value its messages give (NPXS10074 1.80: "SCE_PSM_DRM_KDC_ERROR_REVOKED") */
#define SCE_PSM_DRM_KDC_ERROR_REVOKED		((int)0x808F040E)	/* the content is revoked */

#define PSM_KDC_ERROR_INVALID_ARG		((int)0x808F0401)
#define PSM_KDC_ERROR_NOT_INITIALIZED		((int)0x808F0402)
#define PSM_KDC_ERROR_BUSY			((int)0x808F0403)
#define PSM_KDC_ERROR_ACCOUNT_MISMATCH		((int)0x808F0404)
#define PSM_KDC_ERROR_ACTIVATION_EXPIRED	((int)0x808F0405)
#define PSM_KDC_ERROR_LICENSE_EXPIRED		((int)0x808F0406)
#define PSM_KDC_ERROR_LICENSE_NOT_STARTED	((int)0x808F0407)
#define PSM_KDC_ERROR_DIRECTORY			((int)0x808F0409)
#define PSM_KDC_ERROR_NO_SECURE_CLOCK		((int)0x808F040A)
#define PSM_KDC_ERROR_NOTHING_TO_ABORT		((int)0x808F040B)
#define PSM_KDC_ERROR_ALREADY_INITIALIZED	((int)0x808F040C)
#define PSM_KDC_ERROR_NOT_FOUND			((int)0x808F040D)
#define PSM_KDC_ERROR_ABORTED			((int)0x808F0411)
#define PSM_KDC_ERROR_C1_EXPIRED		((int)0x808F0460)
#define PSM_KDC_ERROR_C1_FAILED			((int)0x808F0461)
#define PSM_KDC_ERROR_BAD_BODY			((int)0x808F0462)
#define PSM_KDC_ERROR_CONNECTION		((int)0x808F0470)
#define PSM_KDC_ERROR_BAD_STATUS_CODE		((int)0x808F0472)
#define PSM_KDC_ERROR_BAD_RESPONSE		((int)0x808F0473)
#define PSM_KDC_ERROR_SERVER_REASON		((int)0x808F0480)	/* the first of the reasons' */
#define PSM_KDC_ERROR_UNKNOWN_REASON		((int)0x808F0494)
#define PSM_KDC_ERROR_RVK_READ			((int)0x808F04B0)
#define PSM_KDC_ERROR_RVK_CONNECTION		((int)0x808F04B1)
#define PSM_KDC_ERROR_RVK_STATUS		((int)0x808F04B2)
#define PSM_KDC_ERROR_RVK_WRITE			((int)0x808F04B3)
#define PSM_KDC_ERROR_RVK_RENAME		((int)0x808F04B4)
#define PSM_KDC_ERROR_RVK_SCAN			((int)0x808F04B5)
#define PSM_KDC_ERROR_RVK_BAD_LOCAL_SIGNATURE	((int)0x808F04B6)
#define PSM_KDC_ERROR_RVK_FORMAT		((int)0x808F04B7)
#define PSM_KDC_ERROR_RVK_BAD_SIGNATURE		((int)0x808F04B8)
#define PSM_KDC_ERROR_RVK_LENGTH		((int)0x808F04B9)
#define PSM_KDC_ERROR_RVK_NOT_NEWER		((int)0x808F04BA)
#define PSM_KDC_ERROR_RVK_OUTDATED		((int)0x808F04BB)
#define PSM_KDC_ERROR_INTERNAL			((int)0x808F04FF)

/* ScePsmDrm's errors that pass through or are made here */
#define PSM_DRM_ERROR_FIRST			((int)0x808F0102)
#define PSM_DRM_ERROR_BAD_LICENSE_SIZE		((int)0x808F0105)
#define PSM_DRM_ERROR_NOT_ACTIVATED		((int)0x808F0106)
#define PSM_DRM_ERROR_ACCOUNT_MISMATCH		((int)0x808F0108)

/* GcAuthMgr's errors of a C1 */
#define GCAUTHMGR_ERROR_NO_SECURE_CLOCK		((int)0x80251002)
#define GCAUTHMGR_ERROR_PSMACT_FAILED		((int)0x808A0800)
#define GCAUTHMGR_ERROR_PSMACT_EXPIRED		((int)0x808A080F)

/* np_kdc's request errors, which the copied request code keeps */
#define KDC_ERROR_BUSY				((int)0x80558204)
#define KDC_ERROR_BAD_RESPONSE			((int)0x80558205)
#define KDC_ERROR_NO_CONSOLE_INFO		((int)0x8055820A)
#define KDC_ERROR_SSL_UNTRUSTED			((int)0x80558220)
#define KDC_ERROR_SSL_EXPIRED			((int)0x80558221)
#define KDC_ERROR_SSL_NAME			((int)0x80558222)
#define KDC_ERROR_SSL_UNKNOWN_CA		((int)0x80558223)
#define KDC_ERROR_SSL_BAD_CERT			((int)0x80558224)
#define KDC_ERROR_SERVER_BASE			((int)0x80558300)

/* SceHttp's: an SSL failure, and the certificate error sceHttpsGetSslError then gives */
#define KDC_HTTP_ERROR_SSL			((int)0x80431075)
#define KDC_SSL_ERROR_CERT			((int)0x80435060)

/* what the module is doing (the abort export stops the transfers) */
enum {
	KDC_STATE_IDLE,
	KDC_STATE_RUNNING,
	KDC_STATE_POSTING,
	KDC_STATE_DOWNLOADING,
	KDC_STATE_LOCAL
};

/* the server's reasons a refusal gives: the 0x2001 one still names the consoles */
#define KDC_REASON_CONSOLE_LIMIT	0x2001

/* five minutes of the secure clock (microseconds): a start this far ahead is accepted */
#define KDC_CLOCK_TOLERANCE		300000000ULL
/* the secure clock's ticks at 1970-01-01 */
#define KDC_UNIX_EPOCH_TICK		62135596800000000ULL

/* one field of the form; the PSM server takes the form in the body's parts instead */
typedef struct KdcParamDesc {
	SceSize length;
	const char *pName;
} KdcParamDesc;

typedef struct KdcParam {
	struct KdcParam *pNext;
	const KdcParamDesc *pDesc;
	SceSize size;
	char value[1];
} KdcParam;

typedef struct KdcParamList {
	KdcParam *pHead;
	KdcParam *pTail;
	SceUInt32 mask;
} KdcParamList;

typedef struct KdcHeaderField {
	const char *pName;
	const char *pValue;
} KdcHeaderField;

/* a request: its connection, and the server's answer (np_kdc's KdcRequest) */
typedef struct KdcRequest {
	SceInt32 connectionId;
	SceInt32 requestId;
	KdcParamList params;
	SceUInt32 flags;		/* KDC_FLAG_CONSOLE_INFO */
	char *pBody;
	SceSize bodySize;
	SceUInt32 maxConsole;
	SceUInt32 availableConsole;
	int reason;
	int refused;
} KdcRequest;

#define KDC_FLAG_CONSOLE_INFO	1

/* what a post sends: the parts of the body */
typedef struct KdcPostParam {
	const void *pTicket;
	SceSize ticketSize;
	const void *pC1;
	SceSize c1Size;
	const char *pContentId;
	SceSize contentIdSize;
} KdcPostParam;

/* what a post gives back */
typedef struct KdcPostResult {
	int wantConsoleInfo;
	SceUInt32 *pMaxConsole;
	SceUInt32 *pAvailableConsole;
	char *pBody;		/* the answer's body, read when it has this size */
	SceSize bodySize;
} KdcPostResult;

/* a server's reason and the error it is reported as */
typedef struct KdcReason {
	int reason;
	int error;
} KdcReason;

/* the revocation list (ur0:/psm/rvk.dat): this header (big-endian), its signature, the entries */
typedef struct KdcRevocationHeader {
	SceUInt64 magic;	/* "PSM-RVK\0" */
	SceUInt32 format;	/* 1 */
	SceUInt32 version;
	SceUInt32 size;		/* of the whole file */
	SceUInt32 count;	/* of the entries */
	SceUInt32 reserved[10];
} KdcRevocationHeader;

#define KDC_RVK_MAGIC		0x50534D2D52564B00ULL
#define KDC_RVK_FORMAT		1
#define KDC_RVK_HEADER_SIZE	64
#define KDC_RVK_SIGNATURE_SIZE	256
#define KDC_RVK_ENTRY_SIZE	64
#define KDC_RVK_DATA_OFFSET	(KDC_RVK_HEADER_SIZE + KDC_RVK_SIGNATURE_SIZE)

/* a revoked content: its content ID (the title ID at 7) */
typedef struct KdcRevocationEntry {
	char contentId[36];
	SceUInt8 reserved[28];
} KdcRevocationEntry;

#define KDC_CONTENT_ID_LENGTH	36
#define KDC_TITLE_ID_OFFSET	7
#define KDC_TITLE_ID_LENGTH	9

/* the other libraries' calls nothing declares */
int sceHttpsEnableOptionPrivate(int id, int option);
int sceHttpsDisableOptionPrivate(int id, int option);
int sceAppMgrSystemParamGetString(int id, char *buf, SceSize bufSize);
int sceAppMgrAppDataMountById(int id, const char *titleId, char *mountPoint);
int sceAppMgrUmount(const char *mountPoint);
int sceDrmBridgeGetCurrentSecureTick(void *tick);
int _vshRtcGetCurrentSecureTick(SceRtcTick *tick);

/* psm_drm.c: ScePsmDrm's calls, as the PSM applications link them */
int KdcRemoveActData(SceUInt64 *pAccountId);
int KdcGetActInfo(int *pActType, int *pVersion, SceUInt64 *pAccountId, SceUInt64 *pStartTime,
		  SceUInt64 *pEndTime);
int KdcGetRifInfo(const ScePsmDrmLicense *pLicense, char *pContentId, SceUInt64 *pAccountId,
		  SceUInt64 *pStartTime, SceUInt64 *pEndTime);
int KdcGetRifName(char *pName, SceUInt64 accountId);
int KdcGetRifNameForInstall(char *pName, const ScePsmDrmLicense *pLicense);
int KdcGetRifKey(const ScePsmDrmLicense *pLicense, ScePsmDrmKeySet *pKeySet, int *pFlags,
		 SceRtcTick *pStartTime, SceRtcTick *pEndTime);
int KdcGetCurrentSecureTickVsh(SceRtcTick *pTick);

#endif /* _PSMKDC_INTERNAL_H */
