#ifndef SCE_NETCTL_H
#define SCE_NETCTL_H

/*
 * SceNetCtl (vs0:sys/external/libnetctl.suprx): official names from the SDK
 * libnetctl.h that the vitasdk headers lack.  Public structures come from
 * <psp2/net/netctl.h>; SceNetCtlIfStat follows the SDK layout.
 */

#include <psp2/types.h>
#include <psp2/net/netctl.h>
#include <psp2/rtc.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/threadmgr/thread.h>

#define SCE_NET_CTL_ERROR_NOT_INITIALIZED		0x80412101
#define SCE_NET_CTL_ERROR_NOT_TERMINATED		0x80412102
#define SCE_NET_CTL_ERROR_CALLBACK_MAX			0x80412103
#define SCE_NET_CTL_ERROR_ID_NOT_FOUND			0x80412104
#define SCE_NET_CTL_ERROR_INVALID_ID			0x80412105
#define SCE_NET_CTL_ERROR_INVALID_CODE			0x80412106
#define SCE_NET_CTL_ERROR_INVALID_ADDR			0x80412107
#define SCE_NET_CTL_ERROR_NOT_CONNECTED			0x80412108
#define SCE_NET_CTL_ERROR_NOT_AVAIL			0x80412109
#define SCE_NET_CTL_ERROR_INVALID_SIZE			0x80412111
#define SCE_NET_CTL_ERROR_INSUFFICIENT_MEMORY		0x8041211d

#define SCE_NET_CTL_EVENT_TYPE_DISCONNECTED		1
#define SCE_NET_CTL_EVENT_TYPE_DISCONNECT_REQ_FINISHED	2
#define SCE_NET_CTL_EVENT_TYPE_IPOBTAINED		3

#define SCE_NET_CTL_CALLBACK_MAX			8

#define SCE_NET_CTL_DOWNLOADABLE_SIZE_UNLIMITED		0x7fffffffffffffffLL

typedef struct SceNetCtlIfStat {
	SceSize size;
	SceUInt32 totalSec;
	SceUInt64 txBytes;
	SceUInt64 rxBytes;
	SceRtcTick resetTick;
	SceUInt32 reserved[8];
} SceNetCtlIfStat;

/*
 * Imports of SceNetCtl that the vitasdk headers lack or declare differently,
 * under the names of the project NID database (unknown functions by NID).
 */
extern int SceShellSvc_1FAB9A1E(int eventId);
extern int SceShellSvc_7CBF442B(int eventId);
extern int SceShellSvc_D943CE15(int eventId, void (*handler)(int eventId), void *arg);
extern int SceShellSvc_FB03FBC9(int eventId);
extern int sceRegMgrUtilityGetBin(SceUInt32 id, void *buf, SceSize size);
extern int sceRegMgrUtilityGetStr(SceUInt32 id, char *buf, SceSize size);
extern int sceAppMgrGetSystemDataFile(int id, void *buf, SceSize size);
extern int sceKernelGetSystemSwVersion(void *version);
extern int SceNetInternal_0DFC23D6(SceUID pid, int a1, int a2);
extern int SceNetInternal_333F9CB3(int sock, unsigned int request, void *arg);
extern void *SceNetInternal_A2C2BC5F(SceSize size, int a1);
extern void SceNetInternal_E62A2FAA(void *ptr);
extern int SceLibKernel_9F793F84(void);
/* ad-hoc authentication primitives (not reverse engineered by project rule) */
extern int SceLibKernel_C8082804(void);
extern int SceLibKernel_211BEDE8(void *a0);
extern int SceLibKernel_DEAD6277(void *a0, const void *a1);
extern int SceLibKernel_104D802F(void *a0, const void *a1);
extern int SceLibKernel_65126005(void *a0, const void *a1, void *a2);
extern int SceLibKernel_4AC7EFC9(void *a0, const void *a1);
extern int SceLibKernel_AC57B6A4(const void *a0, void *a1, void *a2, void *a3);

int sceNetCtlGetIfStat(int device, SceNetCtlIfStat *ifstat);
int sceNetCtlGetPhoneMaxDownloadableSize(SceInt64 *maxDownloadableSize);

#endif
