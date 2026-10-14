/* SceLcd: SPI panel scripts and I2C backlight control.
 * Internal names are unknown: no debug name records exist in this module.
 */
#include <stdint.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/cpu.h>
#include "sce/sysmem.h"
#include "sce/sysevent.h"
#include "sce/lcd.h"

#if FW_VERSION >= 0x04000000
#include "data-4.00-DEBUGGER.h"
#elif FW_VERSION >= 0x03650000
#include "data-3.650.011-TOOL.h"
#else
#include "data-3.630.011-DEBUGGER.h"
#endif
#include <sce/barrier.h>

extern int sceKernelDelayThread(unsigned usec);
extern int ksceKernelSearchModuleByName(const char *name);
extern unsigned ksceSysrootGetHardwareInfo(void);

extern int kscePervasiveSpiResetDisable(int bus);
extern int kscePervasiveSpiResetEnable(int bus);
extern int kscePervasiveSpiClockEnable(int bus);
extern int kscePervasiveSpiClockDisable(int bus);
extern int ksceGpioSetPortMode(int bus, int port, int mode);
extern int ksceGpioPortRead(int bus, int port);
extern int ksceGpioPortSet(int bus, int port);
extern int ksceGpioPortClear(int bus, int port);
extern int ksceI2cTransferWrite(int bus, int address, const void *data, unsigned size);

#define LCD_INVALID ((int)0x803f0c02)
#define LCD_NO_PANEL ((int)0x803f0c03)
#define LCD_NOT_READY ((int)0x803f0c04)
#define LCD_BUSY ((int)0x803f0c05)

static unsigned spiDepth, ready, txBits, txValue;
static uint16_t panelId, panelRevision, scriptRevision;
static volatile uint32_t *spi;
static int spiUid;
static unsigned char script[0x80];
static unsigned synchronous;
static const unsigned char *sequence;
static int delayFrames;
static SceKernelSpinlock backlightLock;
static unsigned backlightReady, displayEnabled, brightness;
static int backlightCurrent, backlightTarget;
static unsigned panelMode, colorMode, hardwareInfo, hardwareSpecial;

static unsigned reverseByte(unsigned value) /* name unknown */
{
	value = ((value & 0x55) << 1) | ((value >> 1) & 0x55);
	value = ((value & 0x33) << 2) | ((value >> 2) & 0x33);
	return ((value & 0x0f) << 4) | ((value >> 4) & 0x0f);
}

static void waitTime(unsigned usec) /* name unknown */
{
	unsigned start = ksceKernelGetSystemTimeLow();
	while ((unsigned)(ksceKernelGetSystemTimeLow() - start) < usec)
		;
}

static void spiBegin(void) /* name unknown */
{
	if (!spiDepth) {
		kscePervasiveSpiClockEnable(2);
		while (spi[10])
			(void)spi[0];
		spi[2] = 0x30001;
	}
	spiDepth++;
}

static void spiBits(unsigned word, unsigned bits) /* name unknown */
{
	txValue |= word << txBits;
	txBits += bits;
	while (txBits >= 16) {
		while (spi[11] == 0x7f)
			;
		spi[1] = txValue & 0xffff;
		txValue >>= 16;
		txBits -= 16;
	}
}

static void spiPad(void) /* name unknown */
{
	while (txBits)
		spiBits(0, 9);
	spi[4] = 1;
	while (spi[11])
		;
	spi[4] = 0;
	while (spi[4] & 1)
		;
}

static void spiEnd(void) /* name unknown */
{
	if (--spiDepth)
		return;
	while (txBits)
		spiBits(0, 9);
	spi[4] = 1;
	while (spi[11])
		;
	while (spi[10])
		(void)spi[0];
	spi[4] = 0;
	while (spi[4] & 1)
		;
	spi[2] = 0x30001;
	(void)spi[2];
	dsb();
	kscePervasiveSpiClockDisable(2);
}

static void spiRead(unsigned char *data, unsigned length, unsigned skip) /* name unknown */
{
	/* The original stores every whole byte, even past the requested length. */
	unsigned value = 0, bits = 0, count = 0;
	if (!length)
		return;
	do {
		while (!spi[10])
			;
		value |= spi[0] << bits;
		bits += 16;
		if (skip) {
			if (bits <= skip) {
				skip -= bits;
				bits = 0;
				/* Original retains the accumulator on this path. */
				continue;
			}
			value >>= skip;
			bits -= skip;
			skip = 0;
		}
		while (bits >= 8) {
			data[count++] = reverseByte(value & 0xff);
			value >>= 8;
			bits -= 8;
		}
	} while (count < length);
}

int SceLcdForDriver_5127FB5E(unsigned char command, const unsigned char *data,
			  unsigned length)
{
	unsigned intr, i;
	if (!ready)
		return LCD_NOT_READY;
	if (ready == 2)
		return LCD_NO_PANEL;
	intr = ksceKernelCpuSuspendIntr();
	spiBegin();
	spiBits(reverseByte(command) << 1, 9);
	for (i = 0; i != length; i++)
		spiBits((reverseByte(data[i]) << 1) | 1, 9);
	spiEnd();
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

int SceLcdForDriver_223C8375(unsigned command, unsigned char *data, unsigned length)
{
	unsigned intr, i;
	if (!ready)
		return LCD_NOT_READY;
	if (ready == 2)
		return LCD_NO_PANEL;
	intr = ksceKernelCpuSuspendIntr();
	spiBegin();
	if (command >= 0xb0)
		spiBits(0x15e, 9);
	spiBits(reverseByte(command & 0xff) << 1, 9);
	if (length != 0xffffffff)
		for (i = 0; i != length + 1; i++)
			spiBits(0, 8);
	spiPad();
	spiRead(data, length, command < 0xb0 ? 9 : 18);
	if (command >= 0xb0) {
		spiEnd();
		spiBegin();
		spiBits(0xde, 9);
	}
	spiEnd();
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

static int initWork(void *arg) /* name unknown */
{
	unsigned char id[12];
	unsigned intr, i;
	int port = ksceGpioPortRead(0, 0);
	if (!port) {
		ksceGpioPortSet(0, 0);
		ksceGpioPortSet(1, 7);
		sceKernelDelayThread(20000);
	}
	intr = ksceKernelCpuSuspendIntr();
	spiBegin();
	spiBits(0x10a, 9);
	for (i = 0; i < 6; i++)
		spiBits(0, 8);
	spiPad();
	spiRead(id, 5, 9);
	spiEnd();
	ksceKernelCpuResumeIntr(intr);
	if (id[4] == 0xff) {
		panelId = id[0] | (id[1] << 8);
		panelRevision = id[2] | (id[3] << 8);
	}
	if (!port) {
		ksceGpioPortClear(0, 0);
		ksceGpioPortClear(1, 7);
	}
	if (id[4] == 0xff && !panelId) {
		ready = 1;
	} else {
		kscePervasiveSpiClockDisable(2);
		kscePervasiveSpiResetEnable(2);
		ready = 2;
	}
	return 1;
}

int SceLcdForDriver_949D45E4(void)
{
	ksceGpioPortClear(0, 0);
	ksceGpioPortClear(1, 7);
	return 0;
}
int SceLcdForDriver_C0A2A6EA(void)
{
	ksceGpioPortSet(0, 0);
	ksceGpioPortSet(1, 7);
	return 0;
}
int SceLcdForDriver_6A08623C(void)
{
	return ksceGpioPortRead(0, 0) == 0;
}
int ksceLcdWaitReady(void)
{
	while (!ready)
		waitTime(1000);
	return 0;
}
int ksceLcdGetDDB(uint16_t *id, uint16_t *revision)
{
	if (!ready)
		return LCD_NOT_READY;
	if (ready == 2)
		return LCD_NO_PANEL;
	if (id)
		*id = panelId;
	if (revision)
		*revision = panelRevision;
	return 0;
}
int SceLcdForDriver_8C965153(void)
{
	unsigned char value[4];
	int ret = SceLcdForDriver_223C8375(10, value, 1);
	return ret < 0 ? ret : ((value[0] >> 2) & 1);
}
int SceLcdForDriver_59CCDC97(void)
{
	const unsigned char *p = sequence;
	if (!p)
		return 0;
	if (delayFrames > 0) {
		delayFrames--;
		return 0;
	}
	while (p) {
		if (*p == 0xff) {
			sequence = 0;
			return 0;
		}
		if (*p == 13) {
			delayFrames = p[1];
			sequence = p + 2;
			return 0;
		}
		SceLcdForDriver_5127FB5E(p[0], p + 2, p[1]);
		p += 2 + p[1];
	}
	sequence = 0;
	return 0;
}
int SceLcdForDriver_4735059C(unsigned poll)
{
	if (poll)
		return sequence != 0 || backlightCurrent != backlightTarget;
	while (sequence || backlightCurrent != backlightTarget) {
		if (!synchronous) {
			sceKernelDelayThread(16000);
		} else {
			SceLcdForDriver_59CCDC97();
			if (sequence || backlightCurrent != backlightTarget)
				waitTime(16000);
		}
	}
	return 0;
}
int SceLcdForDriver_4B6B53C0(const unsigned char *p)
{
	if (sequence)
		return LCD_BUSY;
	sequence = p;
	delayFrames = 0;
	return 0;
}
int SceLcdForDriver_04E9269A(const unsigned char *p)
{
	int ret = SceLcdForDriver_4B6B53C0(p);
	if (!ret)
		SceLcdForDriver_4735059C(0);
	return ret;
}
int SceLcdForDriver_4CC1E8C1(unsigned mode)
{
	synchronous = mode;
	return 0;
}

static unsigned char *copyScript(unsigned char *out, const unsigned char **in,
				 unsigned stop) /* name unknown */
{
	const unsigned char *p = *in;
	unsigned command, length;
	while ((command = p[0]) != stop) {
		length = p[1];
		p += 2;
		*out++ = command;
		*out++ = length;
		if (command != 13 && length) {
			memcpy(out, p, length);
			out += length;
			p += length;
		}
	}
	*in = p;
	return out;
}
static void buildScript(void) /* name unknown */
{
	const unsigned char *init, *p;
	unsigned char *out;
	init = scriptRevision < 0x25 ? initOld :
		(scriptRevision < 0x32 ? initMid : initNew);
	out = copyScript(script, &init, 0x29);
	p = colorMode ? colorOn : colorOff;
	out = copyScript(out, &p, 0xff);
	p = panelMode ? modeOn : modeOff;
	out = copyScript(out, &p, 0xff);
	out = copyScript(out, &init, 0xff);
	*out = 0xff;
}
static int queueSequence(const unsigned char *p) /* name unknown */
{
	int ret = SceLcdForDriver_4B6B53C0(p);
	if (!ret && synchronous)
		SceLcdForDriver_4735059C(0);
	return ret;
}
static int backlightWork(void *arg) /* name unknown */
{
	unsigned char data[2];
	int current, target;
	do {
		if (ksceKernelSpinlockLowTrylock(&backlightLock) < 0)
			break;
		current = backlightCurrent;
		target = backlightTarget;
		if (!target) {
			data[0] = 5;
			data[1] = 0;
			ksceI2cTransferWrite(1, 200, data, 2);
			if (current > 0) {
				data[0] = 14;
				ksceI2cTransferWrite(1, 200, data, 2);
			}
		} else {
			if (!current) {
				data[0] = 14;
				data[1] = 1;
				ksceI2cTransferWrite(1, 200, data, 2);
			}
			while (target - current > 64) {
				current += 64;
				data[0] = 5;
				data[1] = current;
				ksceI2cTransferWrite(1, 200, data, 2);
				waitTime(16000);
			}
			data[0] = 5;
			data[1] = target;
			ksceI2cTransferWrite(1, 200, data, 2);
		}
		backlightCurrent = target;
		ksceKernelSpinlockLowUnlock(&backlightLock);
	} while (backlightCurrent != backlightTarget);
	return 1;
}
static int resetBacklightWork(void *arg) /* name unknown */
{
	unsigned info = hardwareInfo & 0xffff00;
	const unsigned char *p = backlightDefault;
	unsigned char data[2];
	if (info > 0x8043ff || info == 0x804000)
		p = info < 0x901000 ? backlightMid : backlightNew;
	if (hardwareSpecial)
		p = backlightSpecial;
	while (p[0] != 0xff) {
		data[0] = p[0];
		data[1] = p[1];
		if (ksceI2cTransferWrite(1, 200, data, 2) < 0)
			break;
		p += 2;
	}
	if (backlightCurrent != backlightTarget)
		backlightWork(0);
	backlightReady = 1;
	return 1;
}
static int enqueueBrightness(void) /* name unknown */
{
	int ret;
	if (!backlightReady)
		return 0;
	ret = ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceLcdBlSetBrightness",
				      backlightWork, 0);
	return ret < 0 ? ret : 0;
}
int ksceLcdDisplayOn(void)
{
	int ret;
	if (displayEnabled)
		return 0;
	buildScript();
	ret = queueSequence(script);
	if (!ret)
		displayEnabled = 1;
	return ret;
}
int ksceLcdDisplayOff(void)
{
	int ret;
	if (!displayEnabled)
		return 0;
	ret = queueSequence(displayOff);
	if (ret)
		return ret;
	displayEnabled = brightness = backlightTarget = 0;
	return enqueueBrightness();
}
int SceLcdForDriver_9F1DB5CD(void)
{
	return queueSequence(standby);
}
int ksceLcdSetBrightness(unsigned value)
{
	unsigned target;
	int ret;
	if (value > 0x10000)
		return LCD_INVALID;
	if (!value) {
		if (displayEnabled) {
			ret = queueSequence(displayOff);
			if (ret)
				return ret;
			displayEnabled = 0;
		}
		target = 0;
	} else {
		target = value == 1 ? 25 : brightnessTable[((value - 2) * 16) / 65534];
		if (!displayEnabled) {
			ret = ksceLcdDisplayOn();
			if (ret)
				return ret;
		}
	}
	brightness = value;
	backlightTarget = target;
	return enqueueBrightness();
}
unsigned ksceLcdGetBrightness(void)
{
	return brightness;
}
int SceLcdForDriver_1D73D7F3(unsigned mode)
{
	int ret;
	if (mode > 1)
		return LCD_INVALID;
	if (!displayEnabled) {
		panelMode = mode;
		return 0;
	}
	ret = SceLcdForDriver_4B6B53C0(mode ? modeOn : modeOff);
	if (!ret) {
		panelMode = mode;
		if (synchronous)
			SceLcdForDriver_4735059C(0);
	}
	return ret;
}
unsigned SceLcdForDriver_4C277FEC(void)
{
	return panelMode;
}
int ksceLcdSetDisplayColorSpaceMode(unsigned mode)
{
	int ret;
	if (mode > 1)
		return LCD_INVALID;
	if (!displayEnabled) {
		colorMode = mode;
		return 0;
	}
	ret = SceLcdForDriver_4B6B53C0(mode ? colorOn : colorOff);
	if (!ret) {
		colorMode = mode;
		if (synchronous)
			SceLcdForDriver_4735059C(0);
	}
	return ret;
}
unsigned ksceLcdGetDisplayColorSpaceMode(void)
{
	return colorMode;
}
int SceLcdForDriver_3E7C2CCA(void)
{
	/* Sony leaves this uninitialized when GetDDB fails. */
	uint16_t revision;
	int ret;
	SceLcdForDriver_C0A2A6EA();
	if (synchronous)
		waitTime(120000);
	else
		sceKernelDelayThread(120000);
	backlightCurrent = backlightReady = displayEnabled = brightness = backlightTarget = 0;
	ret = SceLcdForDriver_4B6B53C0(resetPanel);
	if (ret)
		return ret;
	SceLcdForDriver_4735059C(0);
	ksceLcdGetDDB(0, &revision);
	if (!revision)
		revision = 50;
	scriptRevision = revision;
	return ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceLcdBlResetSeq", resetBacklightWork, 0);
}
static void setupSpi(void) /* name unknown */
{
	kscePervasiveSpiResetDisable(2);
	kscePervasiveSpiClockEnable(2);
	spi[2] = 0x30001;
	spi[5] = 15;
	spi[3] = 4;
	spi[8] = 0;
	(void)spi[8];
	dsb();
	kscePervasiveSpiClockDisable(2);
	ksceGpioSetPortMode(0, 0, 0);
	ksceGpioSetPortMode(1, 7, 0);
}
static int sysEvent(int resume, int event, void *arg, void *opt) /* name unknown */
{
	if (resume == 1 && event == SYSEVENT_RESUME_DEVICE(3))
		setupSpi();
	if (!resume) {
		if (event == SYSEVENT_SUSPEND_IDLE_QUERY)
			return SceLcdForDriver_4735059C(1) ? -1 : 0;
		if (event == SYSEVENT_SUSPEND_DEVICE(3)) {
			kscePervasiveSpiClockDisable(2);
			kscePervasiveSpiResetEnable(2);
			SceLcdForDriver_949D45E4();
		}
	}
	return 0;
}
int module_start(unsigned arglen, void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned char flags[16];
	void *base;
	int ret;
	if (ksceKernelSearchModuleByName("SceOled") >= 0)
		return 1;
	spiDepth = ready = panelId = panelRevision = txBits = txValue = 0;
	spi = 0;
	spiUid = 0;
	ksceSysrootGetHardwareFlags(flags);
	if ((flags[0] & 3) == 1)
		hardwareSpecial = 1;
	hardwareInfo = ksceSysrootGetHardwareInfo();
	sequence = 0;
	delayFrames = 0;
	ksceKernelRegisterSysEventHandler("SceLcd", sysEvent, 0);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = 0xe0a20000;
	ret = ksceKernelAllocMemBlock("SceSpi2Reg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (ret < 0)
		return 1;
	spiUid = ret;
	ksceKernelGetMemBlockBase(ret, &base);
	spi = base;
	setupSpi();
	ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceLcdInit", initWork, 0);
	return 0;
}
