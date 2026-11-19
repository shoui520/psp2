/*
 * SceIofilemgr's definitions user code shares with the kernel (without the kernel's
 * prototypes, which the user SDK header declares otherwise).
 */

#pragma once

#include <scetypes.h>

/*
 * st_mode's access bits Sony's SDK does not name (it names SCE_STM_RWU and SCE_STM_RU), and
 * st_attr's directory bit, which a vnode's acl_data[0] also carries: our names.
 */
#define IO_STM_WU		(00200)		/* the user may write */
#define IO_STM_RS		(00004)		/* the system may read */
#define IO_STM_WS		(00002)		/* the system may write */
#define IO_STM_RWS		(00006)
#define IO_STA_FDIR		0x0010		/* a directory */

/* devctl 0x3001 ("get device information") fills it: a device's size and free space */
typedef struct SceIoDevInfo {
	SceOff maxSize;				/* bytes */
	SceOff freeSize;			/* bytes */
	SceSize clusterSize;			/* bytes */
	void *reserved;				/* NULL */
} SceIoDevInfo;
_Static_assert(sizeof(SceIoDevInfo) == 0x18, "SceIoDevInfo size");
