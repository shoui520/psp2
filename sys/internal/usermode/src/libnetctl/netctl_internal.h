#ifndef NETCTL_INTERNAL_H
#define NETCTL_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <net.h>
#include <rtc.h>
#include <libnetctl.h>

/* the imports SceNetCtl takes from libraries the tree has no header of yet */
extern int SceShellSvc_1FAB9A1E(int eventId);
extern int SceShellSvc_7CBF442B(int eventId);
extern int SceShellSvc_D943CE15(int eventId, void (*handler)(int eventId), void *arg);
extern int SceShellSvc_FB03FBC9(int eventId);
extern int sceAppMgrGetSystemDataFile(int id, void *buf, SceSize size);
extern int SceNetInternal_0DFC23D6(SceUID pid, int a1, int a2);
extern int SceNetInternal_333F9CB3(int sock, unsigned int request, void *arg);
extern void *SceNetInternal_A2C2BC5F(SceSize size, int a1);
extern void SceNetInternal_E62A2FAA(void *ptr);
extern int SceLibKernel_C8082804(void);
extern int SceLibKernel_211BEDE8(void *context, void *output);
extern int SceLibKernel_DEAD6277(void *context, const void *input, void *output);
extern int SceLibKernel_104D802F(void *context, const void *input, void *output);
extern int SceLibKernel_65126005(void *context, const void *input, void *output);
extern int SceLibKernel_4AC7EFC9(void *context, const void *input);
extern int SceLibKernel_AC57B6A4(const void *context, void *key, void *sendKey, void *receiveKey);
extern int sceAppMgrSendNotificationRequest(void *request, int flags);
/* vitasdk: sceKernelGetSystemTimeLow */
extern SceUInt32 SceThreadmgr_47F6DE49(void);
extern int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
extern int sceRegMgrUtilityGetBin(unsigned int id, void *buf, SceSize size);
extern int sceRegMgrUtilityGetStr(unsigned int id, char *buf, SceSize size);

/* Error codes the SDK headers do not name (names unknown). */
#define NETCTL_ERROR_INVALID_SSID	0x80412190	/* empty or over-long SSID */
#define NETCTL_ERROR_TOO_MANY_SSIDS	0x80412191	/* too many SSIDs */
#define NETCTL_ERROR_INVALID_SCAN_PARAM	0x8041219e	/* scan parameter size mismatch */
#define NETCTL_ERROR_RESTRICTED_FIELD	0x804121cd	/* reserved fields not zero */
#define NETCTL_ERROR_RESERVED_FLAGS	0x804121ce	/* reserved flag bits set */

/*
 * ShellSvc client object (SceShellSvc_B31E7F1C).  Requests go through the
 * sixth virtual method: command, input buffers, result word, output buffers.
 * Output descriptors carry a third word the client never touches.
 */
struct NetCtlSvc;

typedef struct NetCtlSvcInBuffer {
	const void *address;
	SceSize size;
} NetCtlSvcInBuffer;

typedef struct NetCtlSvcOutBuffer {
	void *address;
	SceSize size;
	SceSize reserved;
} NetCtlSvcOutBuffer;

typedef struct NetCtlSvcVtable {
	void *reserved[5];		/* slots this library never calls */
	int (*request)(struct NetCtlSvc *svc, SceUInt32 command,
		       const NetCtlSvcInBuffer *in, SceUInt32 inCount, int *result,
		       NetCtlSvcOutBuffer *out, SceUInt32 outCount);
} NetCtlSvcVtable;

typedef struct NetCtlSvc {
	const NetCtlSvcVtable *vtable;
} NetCtlSvc;

extern NetCtlSvc *SceShellSvc_B31E7F1C(void);

/* One registered NetCtl callback (cid is the slot index). */
typedef struct NetCtlCallbackSlot {
	int cid;
	SceNetCtlCallback func;
	void *arg;
} NetCtlCallbackSlot;

/*
 * Optional override table installed by SceNetCtl_C61F3E96: when an entry is
 * set, the matching export calls it instead of the ShellSvc request.
 */
typedef struct NetCtlHandlers {
	int (*inetGetInfo)(int code, SceNetCtlInfo *info);			/* 0x00 */
	int (*inetGetState)(int *state);					/* 0x04 */
	int (*handler_E8E8DF18)(int *out);					/* 0x08 */
	int (*handler_CDEBA500)(int code, void *info);				/* 0x0C */
	int (*adhocGetState)(int *state);					/* 0x10 */
	int (*handler_0CC36330)(int *out);					/* 0x14 */
	int (*adhocGetInAddr)(SceNetInAddr *inaddr);				/* 0x18 */
	int (*getNatInfo)(SceNetCtlNatInfo *natinfo);				/* 0x1C */
	int (*handler_F30365D6)(void *info);					/* 0x20 */
	int (*handler_6A56FC3A)(const char **ssids, SceUInt32 count, void *param); /* 0x24 */
	int (*handler_47980B9B)(void);						/* 0x28 */
	int (*handler_92EB6B1A)(int a0, void *out);				/* 0x2C */
	int (*handler_A840C74C)(void *buf, int count);				/* 0x30 */
	int (*handler_1A254169)(void *a0, void *a1, int a2, int *a3);		/* 0x34 */
	int (*handler_27932A1B)(void *a0, int a1, int *a2);			/* 0x38 */
	int (*handler_484EE0B2)(void *buf, int count);				/* 0x3C */
	int (*handler_A59146E1)(void *a0, void *a1, int a2, int *a3);		/* 0x40 */
	int (*handler_147B054C)(int *out);					/* 0x44 */
	int (*handler_C15740E9)(int *out);					/* 0x48 */
	int (*handler_CCF30B92)(int a0);					/* 0x4C */
	int (*getIfStat)(int device, SceNetCtlIfStat *ifstat);			/* 0x50 */
	int (*handler_8F2C2053)(int device, SceNetCtlIfStat *ifstat);		/* 0x54 */
	int (*handler_777FE4C2)(int a0, int a1);				/* 0x58 */
	int (*handler_8B2FEFAA)(int a0, int a1);				/* 0x5C */
} NetCtlHandlers;

/*
 * Network configuration handed to the service (0x608 bytes). The library reads only the
 * fields SceNetCtl_EA423A96 requires to be zero ("restricted") and the two buffers it
 * passes on; the rest goes to the service unread.
 */
typedef struct NetCtlNetConfig {
	char settings[0x71];
	unsigned char restrictedFlag;
	char settings2[0x420 - 0x72];
	char restrictedText[4][0x62];
	int restrictedValue[2];
	char restrictedBytes[8];
	char restrictedBytes2[16];
	const void *data1;
	SceSize size1;
	const void *data2;
	SceSize size2;
	int restrictedValue2;
	char restrictedTail[0x2c];
} NetCtlNetConfig;

#define NETCTL_SSID_SIZE		33
#define NETCTL_SCAN_SSID_MAX		10

/* Scan parameters of SceNetCtl_6A56FC3A (0x38 bytes). */
typedef struct NetCtlScanParam {
	SceSize size;
	SceUInt32 flags;
	char reserved08[32];
	const void *data;
	SceSize dataSize;
	char reserved30[8];
} NetCtlScanParam;

/* Entry of the SceNetCtl_3C35D284 / SceNetCtl_1EA25873 tables (0x478 bytes). */
#define NETCTL_ENTRY_MAX		30

typedef struct NetCtlEntry {
	int id;
	char data[0x474];
} NetCtlEntry;

/* value reported when the phone link gives no limit: 20 MiB */
#define NETCTL_PHONE_DEFAULT_DOWNLOADABLE_SIZE	0x1400000

/* Ad-hoc peer information record (adhoc_info.c, 0x100 bytes, serialised field by field). */
#define NETCTL_PEER_NAME_SIZE			36
#define NETCTL_PEER_VERSION_STRING_SIZE		28
typedef struct NetCtlAdhocPeerRecord {
	SceUInt8 format;
	SceUInt8 revision;
	char name[NETCTL_PEER_NAME_SIZE];
	SceUInt8 swVersion[8];		/* big endian */
	char swVersionString[NETCTL_PEER_VERSION_STRING_SIZE];
	SceUInt8 appVersion[4];		/* big endian */
	char reserved[0xb2];
} NetCtlAdhocPeerRecord;

int netCtlAdhocInfoPack(NetCtlAdhocPeerRecord *record);
int netCtlAdhocInfoUnpack(const NetCtlAdhocPeerRecord *record, char *name, SceUInt64 *swVersion,
			  char *swVersionString, SceUInt32 *appVersion);

/* netctl.c */
extern int g_netCtlIsInitialized;
extern SceKernelLwMutexWork g_netCtlMutex;

/* ShellSvc requests (netctl_ipc.c), named by command where the purpose is unknown. */
int netCtlIpcInit(NetCtlSvc *svc, int arg);
int netCtlIpcTerm(NetCtlSvc *svc, int arg);
int netCtlIpcInetGetResult(NetCtlSvc *svc, int eventType, int *errorCode);
int netCtlIpc20003(NetCtlSvc *svc, int a0, int *value);
int netCtlIpcAdhocGetResult(NetCtlSvc *svc, int eventType, int *errorCode);
int netCtlIpc20005(NetCtlSvc *svc, int a0, int *out);
int netCtlIpc20006(NetCtlSvc *svc, int a0);
int netCtlIpc20007(NetCtlSvc *svc, int a0);
int netCtlIpc20008(NetCtlSvc *svc, const void *param, const void *data1, SceSize size1,
		   const void *data2, SceSize size2);
int netCtlIpc20009(NetCtlSvc *svc, const void *param, const void *data1, SceSize size1,
		   const void *data2, SceSize size2, int a6);
int netCtlIpc2000A(NetCtlSvc *svc, int a0);
int netCtlIpcInetGetInfo(NetCtlSvc *svc, int code, SceNetCtlInfo *info);
int netCtlIpc2000C(NetCtlSvc *svc, void *out);
int netCtlIpcInetGetState(NetCtlSvc *svc, int *state);
int netCtlIpc2000E(NetCtlSvc *svc, int *out);
int netCtlIpcGetPhoneMaxDownloadableSize(NetCtlSvc *svc, SceInt64 *size);
int netCtlIpc20010(NetCtlSvc *svc, const void *ssids, SceUInt32 count, const void *param,
		   int a4, int a5);
int netCtlIpc20011(NetCtlSvc *svc, int a0);
int netCtlIpc20012(NetCtlSvc *svc, int a0, void *out);
int netCtlIpc20013(NetCtlSvc *svc, void *buf, int count, int *outCount);
int netCtlIpc20014(NetCtlSvc *svc, void *a0, void *a1, void *a2, int size, int *outSize);
int netCtlIpc20015(NetCtlSvc *svc, void *a0, void *a1, int size, int *outSize);
int netCtlIpc20016(NetCtlSvc *svc, void *buf, int count, int *outCount);
int netCtlIpc20017(NetCtlSvc *svc, void *a0, void *a1, void *a2, int size, int *outSize);
int netCtlIpc20018(NetCtlSvc *svc, int *out);
int netCtlIpc20019(NetCtlSvc *svc, int *out);
int netCtlIpc2001A(NetCtlSvc *svc, int a0);
int netCtlIpc2001B(NetCtlSvc *svc, const void *name);
int netCtlIpc2001C(NetCtlSvc *svc, const void *name);
int netCtlIpc2001D(NetCtlSvc *svc, const void *name);
int netCtlIpc2001E(NetCtlSvc *svc, const void *name, int a1);
int netCtlIpcAdhocDisconnect(NetCtlSvc *svc, int arg);
int netCtlIpcAdhocGetState(NetCtlSvc *svc, int *state);
int netCtlIpc20021(NetCtlSvc *svc, int code, void *info);
int netCtlIpc20022(NetCtlSvc *svc, int *out);
int netCtlIpcAdhocGetInAddr(NetCtlSvc *svc, SceNetInAddr *inaddr);
int netCtlIpcGetNatInfo(NetCtlSvc *svc, SceSize size, SceNetCtlNatInfo *natinfo);
int netCtlIpc20025(NetCtlSvc *svc, SceSize size, void *info);
int netCtlIpc20026(NetCtlSvc *svc, int *out);
int netCtlIpc20027(NetCtlSvc *svc);
int netCtlIpc20028(NetCtlSvc *svc);
int netCtlIpc20029(NetCtlSvc *svc);
int netCtlIpc2002A(NetCtlSvc *svc);
int netCtlIpc2002B(NetCtlSvc *svc);
int netCtlIpc2002C(NetCtlSvc *svc);
int netCtlIpc2002D(NetCtlSvc *svc);
int netCtlIpc2002E(NetCtlSvc *svc, void *out);
int netCtlIpc2002F(NetCtlSvc *svc, int a0);
int netCtlIpcGetIfStat(NetCtlSvc *svc, int device, SceSize size, SceNetCtlIfStat *ifstat);
int netCtlIpc20031(NetCtlSvc *svc, int device, SceSize size, SceNetCtlIfStat *ifstat);
int netCtlIpc20032(NetCtlSvc *svc, int a0);
int netCtlIpc20033(NetCtlSvc *svc);
int netCtlIpc20034(NetCtlSvc *svc, int a0, int a1);
int netCtlIpc20035(NetCtlSvc *svc, int a0, int a1);
int netCtlIpc20036(NetCtlSvc *svc, const void *entries, int count);
int netCtlIpc20037(NetCtlSvc *svc, const void *ids, void *entries, int count);
int netCtlIpc20038(NetCtlSvc *svc, const void *entry);
int netCtlIpc20039(NetCtlSvc *svc, void *entry);
int netCtlIpc2003A(NetCtlSvc *svc);
int netCtlIpc2003B(NetCtlSvc *svc);
int netCtlIpc2003C(NetCtlSvc *svc, int *out);
int netCtlIpc2003D(NetCtlSvc *svc, int a0);
int netCtlIpc2003E(NetCtlSvc *svc);

#endif
