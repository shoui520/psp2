/*
 * access_mode.c: the monitor calls with which the kernel sets the access mode
 * of the Pervasive, TAS, Pervasive2 and SonyRegbus registers. They are taken
 * only in a product mode that allows it (the PmMgr's product mode byte).
 */

#include "intrmgr.h"
#include <lowio/pervasive_regs.h>
#include <gpu/sgx_regs.h>

#define ACCESS_MODE_MONCALL_PERVASIVE	0x104
#define ACCESS_MODE_MONCALL_TAS		0x105
#define ACCESS_MODE_MONCALL_PERVASIVE2	0x106
#define ACCESS_MODE_MONCALL_REGBUS	0x107

/* bits of mask below limit, one at a time */
static inline unsigned int AccessModeBits(int mask, int limit)
{
	unsigned int bits = 0;
	int i;

	for (i = 0; i < limit; i++) {
		if ((mask >> i) & 1)
			bits |= 1 << i;
	}
	return bits;
}

/* mask: bit n selects regbus word n; mode: 0-2 */
static int SetRegbusAccessMode(int mask, int mode, int arg2, int arg3)
{
	SceProductMode productMode;
	int ret;

	ret = sceSblPmMgrGetProductMode(&productMode);
	if (ret < 0)
		return ret;
	if (!productMode)
		return SCE_ERROR_ERRNO_ENOSYS;
	if ((mask & 0x1f) == 0 || (unsigned int)mode > 2)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (mask & 0x01)
		SONY_REGBUS->accessMode[0] = (SONY_REGBUS->accessMode[0] & ~0xf) | mode | mode << 2;
	if (mask & 0x02)
		SONY_REGBUS->accessMode[1] = (SONY_REGBUS->accessMode[1] & ~0xf) | mode | mode << 2;
	if (mask & 0x04)
		SONY_REGBUS->accessMode[2] = (SONY_REGBUS->accessMode[2] & ~0xf) | mode | mode << 2;
	if (mask & 0x08)
		SONY_REGBUS->accessMode[3] = (SONY_REGBUS->accessMode[3] & ~0xf) | mode | mode << 2;
	if (mask & 0x10)
		SONY_REGBUS->accessMode[4] = (SONY_REGBUS->accessMode[4] & ~0xf) | mode | mode << 2;
	return 0;
}

/* mode 1 sets the masked bits, 0 clears them */
static int SetPervasive2AccessMode(int mask, int mode, int arg2, int arg3)
{
	SceProductMode productMode;
	unsigned int bits;
	int ret;

	ret = sceSblPmMgrGetProductMode(&productMode);
	if (ret < 0)
		return ret;
	if (!productMode)
		return SCE_ERROR_ERRNO_ENOSYS;
	if ((mask & 0x3ffff) == 0 || (unsigned int)mode > 1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	bits = AccessModeBits(mask, 18);
	if (mode == 1)
		PERVASIVE2->accessMode |= bits;
	else
		PERVASIVE2->accessMode &= ~bits;
	return 0;
}

static int SetTasAccessMode(int mask, int mode, int arg2, int arg3)
{
	SceProductMode productMode;
	unsigned int bits;
	int ret;

	ret = sceSblPmMgrGetProductMode(&productMode);
	if (ret < 0)
		return ret;
	if (!productMode)
		return SCE_ERROR_ERRNO_ENOSYS;
	if ((mask & 0xff) == 0 || (unsigned int)mode > 1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	bits = AccessModeBits(mask, 8);
	if (mode == 1)
		PERVASIVE_MISC->tasAccessMode |= bits;
	else
		PERVASIVE_MISC->tasAccessMode &= ~bits;
	return 0;
}

static int SetPervasiveAccessMode(int mask, int mode, int arg2, int arg3)
{
	SceProductMode productMode;
	unsigned int bits;
	int ret;

	ret = sceSblPmMgrGetProductMode(&productMode);
	if (ret < 0)
		return ret;
	if (!productMode)
		return SCE_ERROR_ERRNO_ENOSYS;
	if ((mask & 0x7ffff) == 0 || (unsigned int)mode > 1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	bits = AccessModeBits(mask, 19);
	if (mode == 1)
		PERVASIVE_MISC->pervasiveAccessMode |= bits;
	else
		PERVASIVE_MISC->pervasiveAccessMode &= ~bits;
	return 0;
}

int RegisterAccessModeMonitorCall(void)
{
	int ret;

	ret = sceKernelRegisterMonitorCallHandler(ACCESS_MODE_MONCALL_PERVASIVE,
						  SetPervasiveAccessMode);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 234,
			       "register pervasive access mode setting handler failed. 0x%08x\n",
			       ret);
		return ret;
	}
	ret = sceKernelRegisterMonitorCallHandler(ACCESS_MODE_MONCALL_TAS, SetTasAccessMode);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 240,
			       "register TAS access mode setting handler failed. 0x%08x\n", ret);
		return ret;
	}
	ret = sceKernelRegisterMonitorCallHandler(ACCESS_MODE_MONCALL_PERVASIVE2,
						  SetPervasive2AccessMode);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 246,
			       "register pervasive2 access mode setting handler failed. 0x%08x\n",
			       ret);
		return ret;
	}
	ret = sceKernelRegisterMonitorCallHandler(ACCESS_MODE_MONCALL_REGBUS,
						  SetRegbusAccessMode);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 252,
			       "register regbus access mode setting handler failed. 0x%08x\n", ret);
		return ret;
	}
	return 0;
}
