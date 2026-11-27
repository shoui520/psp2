#ifndef IFHANDLE_INT_H
#define IFHANDLE_INT_H

#include <stdint.h>
#include <stddef.h>
#include <common_imp.h>
#include "mbuf.h"

/* Imports */
int sceKernelInitDiscImage(void);
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int state);
void Kprintf(const char *fmt, ...);
int sceKernelQueryModuleInfo(int modid, void *info);
int sceKernelSearchModuleByName(const char *name);
int sceKernelGetUIDcontrolBlock(int uid, void **cb);
void *sceKernelGetGameInfo(void);
void *memset(void *s, int c, size_t n);
int memcmp(const void *lhs, const void *rhs, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strncpy(char *dst, const char *src, size_t n);
int strcmp(const char *lhs, const char *rhs);
int sceKernelSetEventFlag(int evf, uint32_t bits);
int sceKernelDeleteSema(int semaid);
int sceKernelReferVplStatus(int vplid, void *info);
int sceKernelSignalSema(int semaid, int signal);
int sceKernelWaitSema(int semaid, int signal, uint32_t *timeout);
int sceKernelCreateVpl(const char *name, int part, int attr, uint32_t size, void *opt);
int sceKernelPollSema(int semaid, int signal);
int sceKernelDeleteVpl(int vplid);
int sceKernelTryAllocateVpl(int vplid, uint32_t size, void **data);
int sceKernelFreeVpl(int vplid, void *data);
int sceKernelCreateSema(const char *name, uint32_t attr, int init_val, int max_val, void *opt);
int sceWlanDrv_driver_2519EAA7(void);
int sceWlanDrv_driver_325F7172(int wlanMode);
int sceWlanDrv_driver_B6A9700D(void *inBuf, void *outBuf);


typedef struct SceNetUidCB {
	uint32_t header[5];
	uint16_t size_and_type;
	uint16_t attr;
} SceNetUidCB;


typedef struct SceNetIfEvent {
	struct SceNetIfEvent *next; /* +0x00 */
	struct SceNetIfEvent *prev; /* +0x04 */
	int type;                   /* +0x08 */
	int arg;                    /* +0x0c */
} SceNetIfEvent;

typedef struct SceNetIf {
	uint8_t header_pad[0x14];
	char name[16];              /* +0x14 */
	uint8_t state_pad[0xec - 0x24];
	uint8_t mac[6];             /* +0xec */
	uint8_t addr_pad[0x100 - 0xf2];
	int ioctl_sema;             /* +0x100 */
	int ioctl_ok_sema;          /* +0x104 */
	int ioctl_pad;              /* +0x108 */
	int ioctl_res;              /* +0x10c */
	SceNetIfEvent *ev_head;     /* +0x110 */
	SceNetIfEvent *ev_tail;     /* +0x114 */
	int evf_id;                 /* +0x118 */
	uint32_t evf_bits;          /* +0x11c */
} SceNetIf;

typedef struct SceNetIfHandle {
	SceNetIf *ifp;                                             /* +0x00 */
	void *priv;                                                /* +0x04 */
	void (*cb_up)(struct SceNetIfHandle *ifh);                 /* +0x08 */
	void (*cb_down)(struct SceNetIfHandle *ifh);               /* +0x0c */
	void (*start_tx)(struct SceNetIfHandle *ifh);              /* +0x10 */
	void (*ioctl)(struct SceNetIfHandle *ifh, uint32_t cmd, void *arg, void **ptrs); /* +0x14 */
	SceNetMbuf *tx_head;                                       /* +0x18 */
	SceNetMbuf *tx_tail;                                       /* +0x1c */
	int tx_count;                                              /* +0x20 */
	int tx_max;                                                /* +0x24 */
	int tx_drops;                                              /* +0x28 */
} SceNetIfHandle;

typedef struct SceNetMbufQueue {
	SceNetMbuf *head; /* +0x00 */
	SceNetMbuf *tail; /* +0x04 */
	int count;        /* +0x08 */
	int max_count;    /* +0x0c */
	int drops;        /* +0x10 */
} SceNetMbufQueue;

typedef struct GameWorkaround {
	const char *title_id;
	uint32_t mode_mask;
	uint32_t params[5];
} GameWorkaround;


extern int g_vpl_uid;
extern uint32_t g_vpl_max_used;
extern uint32_t g_mbuf_cfg0;
extern uint32_t g_mbuf_cfg1;
extern uint32_t g_mbuf_cfg2;
extern uint32_t g_mbuf_cfg3;
extern uint32_t g_copym_drops;
extern uint32_t g_pullup_drops;

/* Forward declarations */
void *sceNetIfhandle_lib_15CFE3C0(uint32_t size);
void sceNetIfhandle_lib_76BAD213(void *ptr);
SceNetMbuf *sceNetIfhandle_lib_F8825DC4(SceNetMbuf *m);
void sceNetIfhandle_lib_F56FAC82(SceNetMbuf *m);
SceNetMbuf *sceNetIfhandle_lib_59F0D619(int how, uint16_t type);
void sceNetIfhandle_lib_9A6261EC(SceNetMbuf *m, int off, int len, void *cp);
void sceNetIfhandle_lib_BFF3CEA5(SceNetMbuf *mp, int req_len);
SceNetMbuf *sceNetIfhandle_lib_456E3146(SceNetMbuf *m, int start_off, int len, int wait);
void sceNetIfhandle_lib_35FAB6A2(void);
void sceNetIfhandle_lib_0B258B5E(void);
uint32_t sceNetIfhandle_lib_07505747(int idx);


#endif /* IFHANDLE_INT_H */
