/* SCE: ARM EABI types for the PlayStation kernel port of the IMG DDK. */
#ifndef SCE_DDK_PORT_H
#define SCE_DDK_PORT_H

#include <gpu/img_bool.h>

typedef unsigned int IMG_UINT;
typedef int IMG_INT;
typedef unsigned char IMG_UINT8;
typedef signed char IMG_INT8;
typedef unsigned short IMG_UINT16;
typedef short IMG_INT16;
typedef unsigned int IMG_UINT32;
typedef int IMG_INT32;
typedef unsigned long long IMG_UINT64;
typedef long long IMG_INT64;
typedef char IMG_CHAR;
typedef char *IMG_PCHAR;
typedef void IMG_VOID;
typedef void *IMG_HANDLE;
typedef unsigned int IMG_SIZE_T;
typedef unsigned int IMG_UINTPTR_T;
typedef unsigned int IMG_SID;
typedef struct _IMG_DEV_VIRTADDR {
	IMG_UINT32 uiAddr;
} IMG_DEV_VIRTADDR;

typedef struct _IMG_SYS_PHYADDR {
	IMG_UINT32 uiAddr;
} IMG_SYS_PHYADDR;
typedef struct _IMG_CPU_PHYADDR {
	IMG_UINT32 uiAddr;
} IMG_CPU_PHYADDR;
typedef IMG_VOID *IMG_CPU_VIRTADDR;

typedef struct _IMG_DEV_PHYADDR {
	IMG_UINT32 uiAddr;
} IMG_DEV_PHYADDR;

#define IMG_CALLCONV

#include "servicesext.h"

typedef struct PVRSRV_BRIDGE_RETURN_TAG {
	PVRSRV_ERROR eError;
	IMG_VOID *pvData;
} PVRSRV_BRIDGE_RETURN;

typedef struct _PVRSRV_PER_PROCESS_DATA_ PVRSRV_PER_PROCESS_DATA;

#endif
