/* WM1803 codec implementation. Internal names without debug records are unknown. */
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/lowio/i2c.h>
#include <syscon/syscon.h>
#include <psp2kern/kernel/aimgr.h>
#include <string.h>
#include <codec/codec.h>
#include <sysmem/debug.h>
#include <sysmem/sysevent_ids.h>

extern int sceKernelDelayThread(unsigned int delay);

/* name unknown: packed burst descriptors used by the initialization scripts. */
struct CodecBurst {
	const unsigned short *data;
	unsigned short reg;
	unsigned short count;
};
#include "registers.h"

/* name unknown: state immediately follows the 64-byte fast mutex. */
static struct {
	SceKernelFastMutex mutex;
	signed char busy;
	unsigned char flags;
	signed char volume;
	signed char initialized;
	signed char mode;
	signed char revision;
	unsigned char version;
	signed char route;
	unsigned char hardware;
	signed char hardware2;
	short modeValue;
	short level0;
	short level1;
} s_codec;
static const signed char *s_curves[3];

int wm1803ReadRegister(unsigned int reg)
{
	unsigned char command[2], value[2];
	int ret;

	command[0] = reg >> 8;
	command[1] = reg;
	ret = ksceI2cTransferWriteRead(0, 0x34, command, 2, 0x34, value, 2);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 292, "Error: wm1803ReadRegister(R%02d) 0x%08x\n", reg, ret);
		return ret;
	}
	return value[0] * 256 + value[1];
}

/* name unknown: busy register polling, or a fixed settling delay. */
static int waitReady(void)
{
	int ret;

	if (s_codec.busy < 0) {
		sceKernelDelayThread(1500);
		return 0;
	}
	if (s_codec.busy) {
		for (;;) {
			ret = wm1803ReadRegister(0x5d);
			if (ret < 0)
				return ret;
			if (!(ret & 1))
				break;
			sceKernelDelayThread(2000);
		}
	}
	return 0;
}

int wm1803WriteRegister(int reg, unsigned int value)
{
	unsigned char buffer[4];
	int ret, retry;

	buffer[0] = (unsigned int)reg >> 8;
	buffer[1] = reg;
	buffer[2] = value >> 8;
	buffer[3] = value;
	ret = waitReady();
	if (ret < 0)
		return ret;
	s_codec.busy = reg == 0x5a ? (unsigned char)value >> 7 : 0;
	if (reg == 0x57)
		s_codec.busy--;
	for (retry = 0; retry < 3; retry++) {
		ret = ksceI2cTransferWrite(0, 0x34, buffer, 4);
		if (ret >= 0 || retry == 2)
			break;
		sceKernelDelayThread(1000);
	}
	if (retry)
		SCE_DBG_PRINTF(0, 353, "### retry ... wm1803WriteRegister(R%02d,0x%04x) result=%08x\n",
			       reg, value & 0xffff, ret);
	return ret;
}

/* name unknown */
static int writeScript(const short *script)
{
	int ret;

	do {
		ret = wm1803WriteRegister(script[0], script[1]);
		script += 2;
	} while (ret >= 0 && script[0] != -1);
	return ret;
}

/* name unknown */
static int codecEvent(int resume, int event, void *arg, void *opt)
{
	(void)arg;
	(void)opt;
	if (!resume && event == SYSEVENT_SUSPEND_PREPARE(0)) {
		if (ksceKernelLockFastMutex(&s_codec.mutex) >= 0) {
			wm1803WriteRegister(0x5a, 0x9b);
			ksceKernelUnlockFastMutex(&s_codec.mutex);
		}
	} else if (resume == 1 && event == SYSEVENT_RESUME_DEVICE(0)) {
		s_codec.initialized = 0;
		s_codec.busy = 0;
		s_codec.flags = 0;
	}
	return 0;
}

/* name unknown */
static int getCached(int *level0, int *level1)
{
	int ret = ksceKernelLockFastMutex(&s_codec.mutex);

	if (ret >= 0) {
		if (level0)
			*level0 = s_codec.flags & 0x10 ? s_codec.level0 : -121;
		if (level1)
			*level1 = s_codec.level1;
		ret = ((s_codec.flags & 0x10) ? 0x1000 : 0) |
		      ((s_codec.flags & 8) ? 0x400 : 0) |
		      ((s_codec.flags & 4) ? 0x100 : 0);
		ksceKernelUnlockFastMutex(&s_codec.mutex);
	}
	return ret;
}

/* name unknown: preserve the original's positive error return on the second stage. */
static int setVolume(unsigned int volume)
{
	int value, ret, curve;

	if (volume > 30)
		return 0x80261000;
	curve = s_codec.hardware & 1 ? 0 : ((s_codec.flags & 0x1c) == 8 ? 2 : 0);
	value = 121 - s_curves[curve][volume];
	if (value >= 128)
		return 0x80261001;
	ret = ksceKernelLockFastMutex(&s_codec.mutex);
	if (ret < 0)
		return ret;
	if (value < 0)
		value = 0;
	ret = wm1803WriteRegister(2, value | 0x80);
	if (ret >= 0) {
		ret = wm1803WriteRegister(3, value | 0x180);
		if (ret >= 0)
			s_codec.level1 = value - 121;
	}
	ksceKernelUnlockFastMutex(&s_codec.mutex);
	if (ret < 0)
		return ret;
	value = 121 - s_curves[1][volume];
	if (value >= 128 || ksceKernelLockFastMutex(&s_codec.mutex) < 0)
		return 1;
	if (value < 0)
		value = 0;
	ret = wm1803WriteRegister(0x28, value | 0x80);
	if (ret >= 0) {
		ret = wm1803WriteRegister(0x29, value | 0x180);
		if (ret >= 0)
			s_codec.level0 = value - 121;
	}
	ksceKernelUnlockFastMutex(&s_codec.mutex);
	if (ret < 0)
		return 1;
	s_codec.volume = volume;
	return 0;
}

/* name unknown */
static int initializeCodec(int first)
{
	const struct CodecBurst *burst;
	const unsigned short *data;
	unsigned char buffer[64];
	unsigned int reg, count, chunk;
	int ret, i;

	s_codec.flags = 0;
	s_codec.route = 0;
	if (s_codec.initialized <= 0) {
		ret = writeScript(s_initial);
		if (ret < 0)
			return ret;
		s_codec.initialized = 1;
		if (first)
			return 0;
	}
	burst = s_codec.hardware2 == 0x20 ? s_burstHardware :
		(s_codec.revision == 1 ? s_burstRev1 : s_burstRev0);
	ret = waitReady();
	if (ret < 0)
		return ret;
	for (; burst->reg != 0xffff; burst++) {
		reg = burst->reg;
		count = burst->count;
		data = burst->data;
		if (!data) {
			ret = wm1803WriteRegister(reg, count);
			if (ret < 0)
				return ret;
			continue;
		}
		if ((unsigned int)data & 1)
			memset(buffer, 0, sizeof(buffer));
		do {
			chunk = count > 30 ? 31 : count;
			buffer[0] = reg >> 8;
			buffer[1] = reg;
			if (!((unsigned int)data & 1)) {
				for (i = 0; i < (int)chunk; i++) {
					buffer[2 + 2 * i] = data[i] >> 8;
					buffer[3 + 2 * i] = data[i];
				}
				data += chunk;
			}
			ret = ksceI2cTransferWrite(0, 0x34, buffer, (chunk + 1) * 2);
			if (ret < 0)
				return ret;
			count -= chunk;
			reg += chunk;
		} while (count);
	}
	ret = writeScript(s_postBurst);
	if (ret < 0)
		return ret;
	if (s_codec.hardware & 1)
		wm1803WriteRegister(0x61, 0x80);
	return writeScript(s_finish);
}

/* name unknown */
static int startCodec(void)
{
	unsigned char info[16];
	int ret;

	if (ksceSblAimgrIsGenuineDolce())
		return 0;
	SceClockgenForDriver_5130682D();
	ScePervasiveForDriver_925D9D24(0);
	ret = wm1803ReadRegister(1);
	if (ret < 0)
		return 2;
	s_codec.revision = (ret >> 12) & 0xf;
	s_codec.version = (ret >> 9) & 7;
	s_codec.hardware = (ksceSysconGetHardwareInfo() >> 3) & 1;
	ret = ksceSysconGetHardwareInfo2(info);
	s_codec.hardware2 = ret == 0 && (info[0] & 4) ? 0x20 : 0;
	s_curves[0] = s_curve0;
	s_curves[1] = s_codec.hardware2 == 0x20 ? s_curveDolce : s_curve1;
	s_curves[2] = s_curve2;
	ret = ksceKernelInitializeFastMutex(&s_codec.mutex, "SceCodec", 0x2000, 0);
	if (ret < 0)
		return 2;
	initializeCodec(1);
	s_codec.modeValue = 0x127;
	ksceKernelRegisterSysEventHandler("SceCodec", codecEvent, NULL);
	return 0;
}

/* name unknown: update the three output routes and input mode. */
static int configureRoutes(int output0, int output1, int output2,
			   unsigned int mode, int initialize)
{
	unsigned int flags = s_codec.flags, power0, power1, changed;
	int ret;

	if (initialize < 0) {
		output0 = output1 = output2 = -1;
		flags = 0;
	} else {
		if (initialize && !(flags & 1)) {
			flags |= 1;
			initializeCodec(0);
		}
		if (!mode && (flags & 2))
			mode = s_codec.mode | 0x80;
	}
	if ((int)mode > 0 && initialize >= 0) {
		flags |= 2;
		power0 = (0x28 >> (mode & 1)) + ((mode & 0xf) == 1 ? 2 : 0);
	} else {
		flags &= ~2;
		power0 = 0;
	}
	if (output0 > 0 || (!output0 && (flags & 4)))
		flags |= 4;
	else
		flags &= ~4;
	if (output1 > 0 || (!output1 && (flags & 8)))
		flags |= 8;
	else
		flags &= ~8;
	power1 = flags & 0xc ? 0x1e0 : 3;
	if (output2 > 0 || (!output2 && (flags & 0x10))) {
		flags |= 0x10;
		power1 |= 0x198;
	} else {
		flags &= ~0x10;
	}
	power0 |= flags & 0x1e ? 0xc0 : 0x140;
	changed = flags ^ s_codec.flags;
	if (changed & 0x1c) {
		ret = wm1803WriteRegister(5, 0x18);
		if (ret < 0)
			return ret;
	}
	if ((changed & 0x10) && output2 < 0) {
		ret = writeScript(s_disable);
		if (ret < 0)
			return ret;
		ret = wm1803WriteRegister(0x19, power0 & ~0xc);
		if (ret < 0)
			return ret;
		ret = wm1803WriteRegister(0x1a, power1 & ~0x180);
		if (ret < 0)
			return ret;
		ret = writeScript(s_disablePower);
		if (ret < 0)
			return ret;
		ret = wm1803WriteRegister(0x19, power0);
		if (ret < 0)
			return ret;
		ret = wm1803WriteRegister(0x1a, power1);
		if (ret < 0)
			return ret;
		sceKernelDelayThread(3000);
	}
	if (changed & 8) {
		ret = wm1803WriteRegister(0x60, output1 < 0 ? 0 : 0xff);
		if (ret < 0)
			return ret;
	}
	if ((changed & 4) && output0 < 0) {
		ret = wm1803WriteRegister(0x45, 0x77);
		if (ret < 0)
			return ret;
		sceKernelDelayThread(500);
		ret = wm1803WriteRegister(0x45, 0);
		if (ret < 0)
			return ret;
	}
	flags = power0 & 2 ? flags | 0x40 : flags & ~0x40;
	if (flags & 0x80)
		power0 |= 2;
	if (s_codec.revision == 1) {
		ret = writeScript(power0 & 2 ? s_biasOn : s_biasOff);
		if (power0 & 2)
			sceKernelDelayThread(500);
		if (ret < 0)
			return ret;
	}
	ret = wm1803WriteRegister(0x19, power0);
	if (ret < 0)
		return ret;
	ret = wm1803WriteRegister(0x1a, power1);
	if (ret < 0)
		return ret;
	if ((changed & 0x10) && output2 > 0) {
		if (!s_codec.route) {
			ret = wm1803WriteRegister(0x19, power0 & ~0xc);
			if (ret < 0)
				return ret;
			ret = wm1803WriteRegister(0x1a, power1 & ~0x180);
			if (ret < 0)
				return ret;
			ret = writeScript(s_enablePower);
			if (ret < 0)
				return ret;
			ret = wm1803WriteRegister(0x19, power0);
			if (ret < 0)
				return ret;
			ret = wm1803WriteRegister(0x1a, power1);
			if (ret < 0)
				return ret;
		}
		if (writeScript(s_enable) < 0)
			return 1;
		ret = 0;
	}
	if ((changed & 4) && output0 > 0) {
		ret = wm1803WriteRegister(0x45, 0x11);
		if (ret < 0)
			return ret;
		sceKernelDelayThread(500);
		ret = wm1803WriteRegister(0x45, 0x33);
		if (ret < 0)
			return ret;
		ret = wm1803WriteRegister(0x45, 0x77);
		if (ret < 0)
			return ret;
		ret = wm1803WriteRegister(0x45, 0xff);
		if (ret < 0)
			return ret;
	}
	s_codec.flags = flags;
	if (flags & 0x1c) {
		if (changed & 0xc) {
			ksceKernelUnlockFastMutex(&s_codec.mutex);
			ret = setVolume(s_codec.volume);
			ksceKernelLockFastMutex(&s_codec.mutex);
			if (ret < 0)
				return ret;
		}
		ret = wm1803WriteRegister(5, 0x10);
	}
	return ret;
}

/* name unknown */
static int setOutputs(unsigned int outputs)
{
	int ret = ksceKernelLockFastMutex(&s_codec.mutex);

	if (ret >= 0) {
		ret = configureRoutes(outputs & 0x100 ? 1 : -1,
				      outputs & 0x400 ? 1 : -1,
				      outputs & 0x1000 ? 1 : -1, 0, 1);
		ksceKernelUnlockFastMutex(&s_codec.mutex);
	}
	return ret;
}

/* name unknown */
static int setMode(int mode)
{
	short script[13];
	int ret, special;

	if (mode >= 3)
		return 0x80261000;
	ret = ksceKernelLockFastMutex(&s_codec.mutex);
	if (ret < 0)
		return ret;
	if (mode < 0) {
		ret = mode == -1 ? writeScript(s_defaultMode) : (int)0x80261000;
	} else {
		ret = configureRoutes(0, 0, 0, mode + 0x80, 1);
		if (ret >= 0) {
			special = mode == 1;
			script[0] = special;
			script[1] = mode == 2 ? 0x119 : s_codec.modeValue;
			script[2] = 0x20 + special;
			script[3] = mode == 2 ? 0 : 0x20;
			script[4] = 0x25 + special;
			script[5] = mode == 2 ? 0x11 : 0x18;
			script[6] = 0x26 - special;
			script[7] = 0;
			script[8] = 0x22;
			script[9] = special ? 1 : 8;
			script[10] = 0x11d;
			script[11] = special + 2;
			script[12] = -1;
			ret = writeScript(script);
			if (ret >= 0) {
				s_codec.mode = mode;
				ret = special;
			}
		}
	}
	ksceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

/* name unknown */
static int selectRoute(unsigned int mode)
{
	int route, ret;

	if (ksceSblAimgrIsGenuineDolce())
		return 0;
	ret = ksceKernelLockFastMutex(&s_codec.mutex);
	if (ret < 0)
		return ret;
	route = (mode & 0xff00) == 0x100 ? 1 :
		((mode & 0xff00) == 0x200 ? 0 : s_codec.route);
	if (s_codec.route != route) {
		if (!(s_codec.flags & 0x10)) {
			s_codec.route = s_codec.flags & 0x10;
		} else {
			ret = configureRoutes(0, 0, -1, 0, 0);
			if (ret >= 0) {
				s_codec.route = route;
				ret = configureRoutes(0, 0, 1, 0, 0);
			}
		}
	}
	ksceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

/* name unknown: preserve the wait result if the following register read fails. */
static int setEnabled(int enable)
{
	int value, ret = ksceKernelLockFastMutex(&s_codec.mutex);

	if (ret < 0)
		return ret;
	if (!(s_codec.flags & 1))
		configureRoutes(0, 0, 0, 0, 1);
	ret = waitReady();
	if (ret >= 0) {
		s_codec.busy = 0;
		value = wm1803ReadRegister(0x19);
		if (value >= 0) {
			if (!enable) {
				if (!(s_codec.flags & 0x40))
					value &= ~2;
				s_codec.flags &= 0x7f;
			} else {
				value |= 2;
				s_codec.flags |= 0x80;
			}
			if (s_codec.revision == 1) {
				ret = writeScript(value & 2 ? s_biasOn : s_biasOff);
				if (value & 2)
					sceKernelDelayThread(500);
				if (ret < 0)
					goto out;
			}
			ret = wm1803WriteRegister(0x19, value);
		}
	}
out:
	ksceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

/* name unknown */
static int getStatus(void)
{
	int ret = ksceKernelLockFastMutex(&s_codec.mutex);

	if (ret >= 0) {
		ret = wm1803ReadRegister(0x30);
		ksceKernelUnlockFastMutex(&s_codec.mutex);
		if (ret >= 0)
			ret &= 0x80;
	}
	return ret;
}

/* name unknown: module_start chooses this dispatch table or NULL on Dolce. */
struct CodecOps {
	int (*start)(void);
	int (*selectRoute)(unsigned int);
	int (*getCached)(int *, int *);
	int (*setMode)(int);
	int (*getStatus)(void);
	int (*setOutputs)(unsigned int);
	int (*read)(unsigned int);
	int (*setVolume)(unsigned int);
	int (*setEnabled)(int);
	int (*write)(int, unsigned int);
};
static const struct CodecOps s_wm1803 = {
	startCodec, selectRoute, getCached, setMode, getStatus, setOutputs,
	wm1803ReadRegister, setVolume, setEnabled, wm1803WriteRegister,
};
static const struct CodecOps *s_ops;

int SceCodecForDriver_CCDE1B04(unsigned int mode)
{
	return s_ops ? s_ops->selectRoute(mode) : 0;
}
int SceCodecForDriver_41CFFE48(int *level0, int *level1)
{
	return s_ops ? s_ops->getCached(level0, level1) : 0;
}
int SceCodecForDriver_C1DB7BAF(int mode)
{
	return s_ops ? s_ops->setMode(mode) : 0;
}
int SceCodecForDriver_CF398D8A(void)
{
	return s_ops ? s_ops->getStatus() : 0;
}
int SceCodecForDriver_762BF716(unsigned int outputs)
{
	return s_ops ? s_ops->setOutputs(outputs) : 0;
}
int SceCodecForDriver_4FA36937(unsigned int reg)
{
	return s_ops ? s_ops->read(reg) : 0;
}
int SceCodecForDriver_8932F886(SceUInt32 volume)
{
	return s_ops ? s_ops->setVolume(volume) : 0;
}
int SceCodecForDriver_E483B663(int enable)
{
	return s_ops ? s_ops->setEnabled(enable) : 0;
}
int SceCodecForDriver_23BF8884(int reg, unsigned int value)
{
	return s_ops ? s_ops->write(reg, value) : 0;
}
int module_start(unsigned int arglen, void *argp)
{
	(void)arglen;
	(void)argp;
	s_ops = ksceSblAimgrIsDolce() ? NULL : &s_wm1803;
	return s_ops ? s_ops->start() : 0;
}
