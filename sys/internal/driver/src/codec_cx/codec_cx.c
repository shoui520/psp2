/* SceCodec: I2C register scripts and cached audio routing state. */
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/suspend.h>
#include <lowio/i2c.h>
#include <codec/codec.h>
#include <sysmem/sysevent_ids.h>
#include "registers.h"
#include <threadmgr/threadmgr.h>

static struct {
	SceKernelFastMutex mutex;
	unsigned char flags;
	signed char volume;
	signed char initialized;
	signed char mode;
	short reserved;
	short level;
} s_codec;

/* name unknown: coalesce consecutive register writes, at most 16 bytes. */
static int writeScript(const short *script)
{
	unsigned char buffer[18];
	int count, ret;

	do {
		buffer[0] = (unsigned short)script[0] >> 8;
		buffer[1] = script[0];
		count = 0;
		do {
			buffer[2 + count++] = script[1];
			script += 2;
			if (script[0] != script[-2] + 1)
				break;
		} while (count != 16);
		ret = SceI2cForDriver_CA94A759(0, 0x66, buffer, count + 2);
	} while (ret >= 0 && script[0] != -1);
	return ret;
}

int SceCodecForDriver_23BF8884(unsigned int reg, unsigned int value)
{
	unsigned char buffer[3];

	buffer[0] = reg >> 8;
	buffer[1] = reg;
	buffer[2] = value;
	return SceI2cForDriver_CA94A759(0, 0x66, buffer, 3);
}

int SceCodecForDriver_4FA36937(unsigned int reg)
{
	unsigned char command[2], value;
	int ret;

	command[0] = reg >> 8;
	command[1] = reg;
	ret = SceI2cForDriver_0A40B7BF(0, 0x66, command, 2, 0x66, &value, 1);
	return ret < 0 ? ret : value;
}

/* name unknown: apply the volume table for the currently cached route. */
static int setVolume(int volume)
{
	int value = 0x8b - s_volume[volume + ((s_codec.flags & 4) ? 0 : 31)];
	int ret;

	if (value < 0)
		value = 0;
	ret = SceCodecForDriver_23BF8884(0x12, value);
	if (ret >= 0)
		s_codec.level = (value - 0x7f) >> 1;
	return ret;
}

/* name unknown: initialize once, then wait for the codec-ready bit. */
static int initializeCodec(int first)
{
	int ret;

	s_codec.flags = 0;
	if (s_codec.initialized <= 0) {
		ret = writeScript(s_init);
		if (ret < 0)
			return ret;
		s_codec.initialized = 1;
		if (first)
			return 0;
	}
	do {
		sceKernelDelayThread(5000);
		ret = SceCodecForDriver_4FA36937(0x154);
		if (ret < 0)
			return ret;
	} while (!(ret & 0x40));
	return writeScript(s_ready);
}

/* name unknown: tri-state route arguments, with negative power forcing off. */
static int setRouting(int route, int output, int active, int power)
{
	unsigned int old = s_codec.flags, flags = old;
	int alternate = 0, ret;

	if (power < 0) {
		flags = 0;
		route = output = active = -1;
	} else if (power && !(old & 1)) {
		/* The original deliberately ignores initialization failure. */
		initializeCodec(0);
		flags = old | 1;
	}
	if (active > 0 || (active == 0 && (flags & 2)))
		flags |= 2;
	else
		flags &= ~2;
	if (route > 0 || (route == 0 && (flags & 4)))
		flags |= 4;
	else
		flags &= ~4;
	if (output > 0 || (output == 0 && (flags & 0x10)))
		flags |= 0x10;
	else
		flags &= ~0x10;
	if ((old ^ flags) & 0x14)
		setVolume(0);
	ret = writeScript((flags & 4) ? s_routingOn : s_routingOff);
	if (ret < 0)
		return ret;
	if (!s_codec.mode && (flags & 0x14) == 0x10) {
		ret = writeScript(s_alternate);
		if (ret < 0)
			return ret;
		alternate = 1;
	}
	ret = SceCodecForDriver_23BF8884(0x36, (flags & 0x10) ? 0x13 : 6);
	if (ret < 0)
		return ret;
	if (!alternate) {
		ret = writeScript(s_normal);
		if (ret < 0)
			return ret;
	}
	s_codec.flags = flags;
	if (flags & 0x14)
		setVolume(s_codec.volume);
	if (active < 0)
		return writeScript(s_shutdown);
	return ret;
}

/* name unknown: system suspend/resume notification. */
static int codecEvent(int resume, int event, void *args, void *opt)
{
	if (!resume && event == SYSEVENT_SUSPEND_PREPARE(0))
		setRouting(0, 0, 0, -1);
	else if (resume == 1 && event == SYSEVENT_RESUME_DEVICE(0)) {
		s_codec.initialized = 0;
		s_codec.flags = 0;
	}
	return 0;
}

int module_start(unsigned int args, void *argp)
{
	SceClockgenForDriver_5130682D();
	ScePervasiveForDriver_925D9D24(0);
	if (sceKernelInitializeFastMutex(&s_codec.mutex, "SceCodec", 0x2000, 0) < 0)
		return 2;
	initializeCodec(1);
	sceKernelRegisterSysEventHandler("SceCodec", codecEvent, 0);
	return 0;
}

int SceCodecForDriver_41CFFE48(int *output, int *level)
{
	int ret = lockFastMutex(&s_codec.mutex);

	if (ret < 0)
		return ret;
	if (output)
		*output = (s_codec.flags & 0x10) ? s_codec.level : -0x79;
	if (level)
		*level = s_codec.level;
	ret = ((s_codec.flags & 0x10) ? 0x1000 : 0) |
	      ((s_codec.flags & 4) ? 0x100 : 0);
	sceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

int SceCodecForDriver_8932F886(SceUInt32 volume)
{
	int ret;

	if (volume >= 31)
		return 0x80261000;
	ret = lockFastMutex(&s_codec.mutex);
	if (ret < 0)
		return ret;
	ret = setVolume(volume);
	if (ret >= 0)
		s_codec.volume = volume;
	sceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

int SceCodecForDriver_762BF716(unsigned int flags)
{
	int ret = lockFastMutex(&s_codec.mutex);

	if (ret < 0)
		return ret;
	ret = setRouting((flags & 0x100) ? 1 : -1, (flags & 0x1000) ? 1 : -1, 0, 1);
	sceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

int SceCodecForDriver_C1DB7BAF(unsigned int mode)
{
	short script[29], *p = script;
	int ret = lockFastMutex(&s_codec.mutex);
	int other = mode != 1;

	if (ret < 0)
		return ret;
	if (mode >= 3) {
		ret = 0x80261000;
		goto out;
	}
	ret = setRouting(0, 0, -1, 1);
	if (ret < 0)
		goto out;
	if (other) {
		*p++ = 0xb0;
		*p++ = 0x23;
		*p++ = 0x5d;
		*p++ = 0;
	}
	*p++ = 0x20;
	*p++ = 0x11;
	*p++ = 0x29;
	*p++ = other ? 7 : 6;
	*p++ = 0x2a;
	*p++ = 0x21;
	*p++ = 0x2d;
	*p++ = 0x52;
	*p++ = 0x50;
	*p++ = 0xa0;
	*p++ = 0x52;
	*p++ = other ? 0x21 : 0x26;
	*p++ = 0x53;
	*p++ = 0x20;
	*p++ = 0x54;
	*p++ = 1;
	*p++ = 0x5b;
	*p++ = other ? 0x28 : 0x18;
	*p++ = 0x6b;
	*p++ = other;
	if (!other) {
		*p++ = 0xb0;
		*p++ = 0x23;
		*p++ = 0x5d;
		*p++ = 1;
	}
	*p++ = 0x26;
	*p++ = 0xe0;
	*p++ = 0x140;
	*p++ = 0x12;
	*p = -1;
	ret = writeScript(script);
	if (ret >= 0)
		ret = 0;
out:
	sceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

int SceCodecForDriver_CCDE1B04(unsigned int mode)
{
	int ret = lockFastMutex(&s_codec.mutex);

	if (ret < 0)
		return ret;
	if ((mode & 0xff00) == 0x100 || (mode & 0xff00) == 0x200) {
		s_codec.mode = (mode & 0xff00) == 0x100;
		if ((s_codec.flags & 0x14) == 0x10)
			ret = writeScript(s_codec.mode ? s_normal : s_alternate);
	}
	sceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}

int SceCodecForDriver_CF398D8A(void)
{
	int count, ret;

	for (count = 100; count; count--) {
		ret = lockFastMutex(&s_codec.mutex);
		if (ret < 0)
			return ret;
		ret = SceCodecForDriver_4FA36937(0xb6);
		sceKernelUnlockFastMutex(&s_codec.mutex);
		if (ret < 0)
			return ret;
		if (ret & 0x40)
			return (ret >> 1) & 1;
		sceKernelDelayThread(10000);
	}
	return 0x80261001;
}

int SceCodecForDriver_E483B663(int enable)
{
	int ret = lockFastMutex(&s_codec.mutex);

	if (ret < 0)
		return ret;
	ret = writeScript(enable ? s_on : s_off);
	sceKernelUnlockFastMutex(&s_codec.mutex);
	return ret;
}
