/* SceLcd: SPI panel scripts and I2C backlight control.
 * Internal names are unknown: no debug name records exist in this module.
 */
#include <stdint.h>
#include <sysmem/uid_guid.h>
#include <sysmem/sysclib.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysevent.h>
#include <lowio/spi_regs.h>
#include <lcd/lcd.h>
#include <lowio/gpio.h>
#include <lowio/i2c.h>
#include <lowio/pervasive.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>
#include <modulemgr/modulemgr.h>

#if FW_VERSION >= 0x04000000
#include "data-4.00-DEBUGGER.h"
#elif FW_VERSION >= 0x03650000
#include "data-3.650.011-TOOL.h"
#else
#include "data-3.630.011-DEBUGGER.h"
#endif
#include <sysmem/barrier.h>

#define LCD_INVALID ((int)0x803f0c02)
#define LCD_NO_PANEL ((int)0x803f0c03)
#define LCD_NOT_READY ((int)0x803f0c04)
#define LCD_BUSY ((int)0x803f0c05)

/* name unknown: the SPI link and panel state initLcd clears */
typedef struct LcdLink {
	unsigned spiDepth;			/* 0x00 */
	unsigned ready;				/* 0x04 */
	uint16_t panelId;			/* 0x08 */
	uint16_t panelRevision;			/* 0x0a */
	unsigned txBits;			/* 0x0c */
	unsigned txValue;			/* 0x10 */
	volatile SceSpiRegisters *spi;		/* 0x14 */
	int spiUid;				/* 0x18 */
} LcdLink;

static LcdLink lcd;
static uint16_t scriptRevision;
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
	unsigned start = SceThreadmgrForDriver_47F6DE49();
	while ((unsigned)(SceThreadmgrForDriver_47F6DE49() - start) < usec)
		;
}

static void spiBegin(void) /* name unknown */
{
	if (!lcd.spiDepth) {
		ScePervasiveForDriver_DFD96BFC(2);
		while (lcd.spi->receiveCount)
			(void)lcd.spi->receive;
		lcd.spi->control = 0x30001;
	}
	lcd.spiDepth++;
}

static void spiBits(unsigned word, unsigned bits) /* name unknown */
{
	lcd.txValue |= word << lcd.txBits;
	lcd.txBits += bits;
	while (lcd.txBits >= 16) {
		while (lcd.spi->transmitCount == 0x7f)
			;
		lcd.spi->transmit = lcd.txValue & 0xffff;
		lcd.txValue >>= 16;
		lcd.txBits -= 16;
	}
}

static void spiPad(void) /* name unknown */
{
	while (lcd.txBits)
		spiBits(0, 9);
	lcd.spi->enable = 1;
	while (lcd.spi->transmitCount)
		;
	lcd.spi->enable = 0;
	while (lcd.spi->enable & 1)
		;
}

static void spiEnd(void) /* name unknown */
{
	if (--lcd.spiDepth)
		return;
	while (lcd.txBits)
		spiBits(0, 9);
	lcd.spi->enable = 1;
	while (lcd.spi->transmitCount)
		;
	while (lcd.spi->receiveCount)
		(void)lcd.spi->receive;
	lcd.spi->enable = 0;
	while (lcd.spi->enable & 1)
		;
	lcd.spi->control = 0x30001;
	(void)lcd.spi->control;
	dsb();
	ScePervasiveForDriver_A85BF98A(2);
}

static void spiRead(unsigned char *data, unsigned length, unsigned skip) /* name unknown */
{
	/* The original stores every whole byte, even past the requested length. */
	unsigned value = 0, bits = 0, count = 0;
	if (!length)
		return;
	do {
		while (!lcd.spi->receiveCount)
			;
		value |= lcd.spi->receive << bits;
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
	if (!lcd.ready)
		return LCD_NOT_READY;
	if (lcd.ready == 2)
		return LCD_NO_PANEL;
	intr = SceCpuForDriver_821FC0EE();
	spiBegin();
	spiBits(reverseByte(command) << 1, 9);
	for (i = 0; i != length; i++)
		spiBits((reverseByte(data[i]) << 1) | 1, 9);
	spiEnd();
	sceKernelCpuResumeIntr(intr);
	return 0;
}

int SceLcdForDriver_223C8375(unsigned command, unsigned char *data, unsigned length)
{
	unsigned intr, i;
	if (!lcd.ready)
		return LCD_NOT_READY;
	if (lcd.ready == 2)
		return LCD_NO_PANEL;
	intr = SceCpuForDriver_821FC0EE();
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
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static int initWork(void *arg) /* name unknown */
{
	unsigned char id[12];
	unsigned intr, i;
	int port = SceGpioForDriver_129DF5AC(0, 0);
	if (!port) {
		SceGpioForDriver_D454A584(0, 0);
		SceGpioForDriver_D454A584(1, 7);
		sceKernelDelayThread(20000);
	}
	intr = SceCpuForDriver_821FC0EE();
	spiBegin();
	spiBits(0x10a, 9);
	for (i = 0; i < 6; i++)
		spiBits(0, 8);
	spiPad();
	spiRead(id, 5, 9);
	spiEnd();
	sceKernelCpuResumeIntr(intr);
	if (id[4] == 0xff) {
		lcd.panelId = id[0] | (id[1] << 8);
		lcd.panelRevision = id[2] | (id[3] << 8);
	}
	if (!port) {
		SceGpioForDriver_F6310435(0, 0);
		SceGpioForDriver_F6310435(1, 7);
	}
	if (id[4] == 0xff && !lcd.panelId) {
		lcd.ready = 1;
	} else {
		ScePervasiveForDriver_A85BF98A(2);
		ScePervasiveForDriver_7B16F900(2);
		lcd.ready = 2;
	}
	return 1;
}

int SceLcdForDriver_949D45E4(void)
{
	SceGpioForDriver_F6310435(0, 0);
	SceGpioForDriver_F6310435(1, 7);
	return 0;
}
int SceLcdForDriver_C0A2A6EA(void)
{
	SceGpioForDriver_D454A584(0, 0);
	SceGpioForDriver_D454A584(1, 7);
	return 0;
}
int SceLcdForDriver_6A08623C(void)
{
	return SceGpioForDriver_129DF5AC(0, 0) == 0;
}
int SceLcdForDriver_0C7E03D8(void)
{
	while (!lcd.ready)
		waitTime(1000);
	return 0;
}
int SceLcdForDriver_E03E120B(uint16_t *id, uint16_t *revision)
{
	if (!lcd.ready)
		return LCD_NOT_READY;
	if (lcd.ready == 2)
		return LCD_NO_PANEL;
	if (id)
		*id = lcd.panelId;
	if (revision)
		*revision = lcd.panelRevision;
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

static __attribute__((noinline)) int readHardwareInfo(void) /* name unknown */
{
	unsigned char flags[16];

	SceSysrootForDriver_46E72428(flags);
	if ((flags[0] & 3) == 1)
		hardwareSpecial = 1;
	hardwareInfo = SceSysrootForDriver_930B1342();
	sequence = NULL;
	delayFrames = 0;
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
		if (SceCpuForDriver_5AC9D394(&backlightLock) < 0)
			break;
		current = backlightCurrent;
		target = backlightTarget;
		if (!target) {
			data[0] = 5;
			data[1] = 0;
			SceI2cForDriver_CA94A759(1, 200, data, 2);
			if (current > 0) {
				data[0] = 14;
				SceI2cForDriver_CA94A759(1, 200, data, 2);
			}
		} else {
			if (!current) {
				data[0] = 14;
				data[1] = 1;
				SceI2cForDriver_CA94A759(1, 200, data, 2);
			}
			while (target - current > 64) {
				current += 64;
				data[0] = 5;
				data[1] = current;
				SceI2cForDriver_CA94A759(1, 200, data, 2);
				waitTime(16000);
			}
			data[0] = 5;
			data[1] = target;
			SceI2cForDriver_CA94A759(1, 200, data, 2);
		}
		backlightCurrent = target;
		sceKernelSpinlockLowUnlock(&backlightLock);
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
		if (SceI2cForDriver_CA94A759(1, 200, data, 2) < 0)
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
	ret = sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceLcdBlSetBrightness",
				      backlightWork, 0);
	return ret < 0 ? ret : 0;
}
int SceLcdForDriver_5F4124AB(void)
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
int SceLcdForDriver_1A0A7519(void)
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
int SceLcdForDriver_581D3A87(unsigned value)
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
			ret = SceLcdForDriver_5F4124AB();
			if (ret)
				return ret;
		}
	}
	brightness = value;
	backlightTarget = target;
	return enqueueBrightness();
}
unsigned SceLcdForDriver_3A6D6AC3(void)
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
int SceLcdForDriver_D40968FB(unsigned mode)
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
unsigned SceLcdForDriver_17F66722(void)
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
	SceLcdForDriver_E03E120B(0, &revision);
	if (!revision)
		revision = 50;
	scriptRevision = revision;
	return sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceLcdBlResetSeq", resetBacklightWork, 0);
}
static inline void setupSpi(volatile SceSpiRegisters *regs) /* name unknown */
{
	ScePervasiveForDriver_E4B145AE(2);
	ScePervasiveForDriver_DFD96BFC(2);
	regs->control = 0x30001;
	regs->dmaControl = 15;
	regs->intControl = 4;
	regs->mode = 0;
	(void)regs->mode;
	dsb();
	ScePervasiveForDriver_A85BF98A(2);
	SceGpioForDriver_372022A4(0, 0, 0);
	SceGpioForDriver_372022A4(1, 7, 0);
}
static int sysEvent(int resume, int event, void *arg, void *opt) /* name unknown */
{
	if (!resume) {
		if (event == SYSEVENT_SUSPEND_IDLE_QUERY)
			return SceLcdForDriver_4735059C(1) ? -1 : 0;
		if (event == SYSEVENT_SUSPEND_DEVICE(3)) {
			ScePervasiveForDriver_A85BF98A(2);
			ScePervasiveForDriver_7B16F900(2);
			/* SceLcdForDriver_949D45E4, inlined in Sony's build */
			SceGpioForDriver_F6310435(0, 0);
			SceGpioForDriver_F6310435(1, 7);
		}
	} else if (resume == 1 && event == SYSEVENT_RESUME_DEVICE(3)) {
		setupSpi(lcd.spi);
	}
	return 0;
}
static __attribute__((noinline)) int initLcd(void) /* name unknown */
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	int ret;

	lcd = (LcdLink){ 0 };
	readHardwareInfo();
	sceKernelRegisterSysEventHandler("SceLcd", sysEvent, 0);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = SPI_REG_BASE(2);
	ret = SceSysmemForDriver_C94850C9("SceSpi2Reg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (ret < 0)
		return ret;
	lcd.spiUid = ret;
	SceSysmemForDriver_A841EDDA(ret, &base);
	lcd.spi = base;
	setupSpi(base);
	sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceLcdInit", initWork, 0);
	return 0;
}

int module_start(unsigned arglen, void *argp)
{
	if (SceModulemgrForDriver_BBE1771C("SceOled") >= 0)
		return 1;
	return initLcd() < 0;
}
