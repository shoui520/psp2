#ifndef NETCTL_INTERNAL_H
#define NETCTL_INTERNAL_H

#include <stdint.h>
#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/clib.h>
#include <psp2/net/net.h>
#include <sce/netctl.h>

/* Error codes the SDK headers do not name (names unknown). */
#define NETCTL_ERROR_UNK_80412190	0x80412190	/* empty or over-long SSID */
#define NETCTL_ERROR_UNK_80412191	0x80412191	/* too many SSIDs */
#define NETCTL_ERROR_UNK_8041219E	0x8041219e	/* scan parameter size mismatch */
#define NETCTL_ERROR_UNK_804121CD	0x804121cd	/* reserved fields not zero */
#define NETCTL_ERROR_UNK_804121CE	0x804121ce	/* reserved flag bits set */

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
	SceSize unk08;
} NetCtlSvcOutBuffer;

typedef struct NetCtlSvcVtable {
	void *unk00;
	void *unk04;
	void *unk08;
	void *unk0C;
	void *unk10;
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

/* Network configuration handed to the service (0x608 bytes; layout mostly unknown). */
typedef struct NetCtlNetConfig {
	char unk000[0x71];
	unsigned char unk71;
	char unk072[0x420 - 0x72];
	char unk420[0x62];
	char unk482[0x62];
	char unk4E4[0x62];
	char unk546[0x62];
	int unk5A8;
	int unk5AC;
	char unk5B0[8];
	char unk5B8[16];
	const void *data1;
	SceSize size1;
	const void *data2;
	SceSize size2;
	int unk5D8;
	char unk5DC[0x2c];
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
	char unk004[0x474];
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
	char unk4E[0xb2];
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
