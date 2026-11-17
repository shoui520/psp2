/*
 * libnetctl/netctl_handlers.h: the table SceNetCtl_C61F3E96 installs (the shell gives its own
 * NetCtl server's functions). Names ours: no record names the table or its fields.
 */
#ifndef LIBNETCTL_NETCTL_HANDLERS_H
#define LIBNETCTL_NETCTL_HANDLERS_H

#include <scetypes.h>
#include <net.h>
#include <libnetctl.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/* installs the table (vitasdk: none) */
int SceNetCtl_C61F3E96(const NetCtlHandlers *handlers);

#ifdef __cplusplus
}
#endif

#endif
