/* Full SCE native timer owner, adapted from pristine IMG osfunc.c. */
#ifndef GPU_OSFUNC_SCE_H
#define GPU_OSFUNC_SCE_H

#include <gpu/sce_ddk_port.h>
#include <systimer/systimer.h>

typedef IMG_VOID (*PFN_TIMER_FUNC)(IMG_VOID *pvData);

typedef enum _img_verify_test {
	PVR_VERIFY_WRITE = 0,
	PVR_VERIFY_READ
} IMG_VERIFY_TEST;

IMG_BOOL OSAccessOK(IMG_VERIFY_TEST eVerification, IMG_VOID *pvUserPtr,
		IMG_SIZE_T ui32Bytes);

typedef struct TIMER_CALLBACK_DATA_TAG {
	PFN_TIMER_FUNC pfnTimerFunc;
	IMG_VOID *pvData;
	/* SCE replaces Linux timer_list with a native system timer id. */
	SceSysTimerId sTimer;
	IMG_BOOL bActive;
} TIMER_CALLBACK_DATA;

_Static_assert(sizeof(TIMER_CALLBACK_DATA) == 16, "SCE timer owner");

#endif
