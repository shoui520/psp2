/*
 * SceDisplay: imported functions vitasdk does not declare (or declares
 * differently from how SceDisplay calls them). Unknown Sony names are kept
 * as library/NID identifiers.
 */
#pragma once

#include <psp2kern/types.h>
#include <oled/oled.h>
#include <lcd/lcd.h>
#include <sysmem/sysmem.h>
#include <lowio/dsi.h>
#include <lowio/iftu.h>
#include <lowio/pervasive.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/utils.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot_init.h>
#include <processmgr/processmgr.h>
#include <acmgr/acmgr.h>
#include <sysmem/sysroot.h>

struct ThreadmgrWaitableClass;
int sceKernelUIDInheritedEventClassInit(struct ThreadmgrWaitableClass *cls,
	const char *name, SceSize size, SceClassCallback create, SceClassCallback destroy);
int sceKernelEventObjectInit(SceUID event, SceUInt32 attr, SceUInt64 userData,
	SceUInt32 pattern);

/* SceSysclibForDriver */
unsigned int __aeabi_uidiv(unsigned int n, unsigned int d);

/* SceSysrootForDriver / SceSysrootForKernel */

/* SceProcessmgrForDriver */

/* SceThreadmgrForDriver */

/* SceSblACMgrForDriver / SceSblAIMgrForDriver */

/* SceDsiForDriver */

/* ScePervasiveForDriver */

/* SceIftuForDriver */

/* SceHdmiForDriver */
int SceHdmiForDriver_08AFFB03(int enable);
int SceHdmiForDriver_5A212BB3(const void *mode);
int SceHdmiForDriver_7AD0F6C9(void *info, int count, void *mode);
int SceHdmiForDriver_99A3CABE(int enable);
