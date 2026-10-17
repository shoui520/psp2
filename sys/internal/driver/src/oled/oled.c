/* SceOled: packed nine-bit SPI panel commands and deferred display sequences. */
#include <stdint.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <sysmem/sysroot.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysevent.h>
#include <lowio/spi_regs.h>
#include <oled/oled.h>
#include "panel_data.h"
#include <sysmem/barrier.h>
#include <psp2kern/lowio/gpio.h>
#include <psp2kern/lowio/pervasive.h>
#include <threadmgr/threadmgr.h>

/* Internal names are unknown; no debug function records exist in this module. */

#define OLED_NOT_READY ((int)0x803f0a04)
#define OLED_NO_PANEL ((int)0x803f0a03)
#define OLED_BUSY ((int)0x803f0a05)
#define OLED_INVALID ((int)0x803f0a02)

/* name unknown: the SPI link and panel state initOled clears */
typedef struct OledLink {
	unsigned spiDepth;			/* 0x00 */
	unsigned ready;				/* 0x04 */
	uint16_t panelId;			/* 0x08 */
	uint16_t panelRevision;			/* 0x0a */
	unsigned txBits;			/* 0x0c */
	unsigned txValue;			/* 0x10 */
	volatile SceSpiRegisters *spi;		/* 0x14 */
	int spiUid;				/* 0x18 */
	int eventUid;				/* 0x1c */
} OledLink;

static OledLink oled;
static unsigned synchronous;
static const unsigned char *sequence;
static int delayFrames;
static const unsigned char *gamma, *brightnessControl;
static unsigned char brightnessSequence[0x101];
static unsigned char colorSupported, panelType;
static unsigned brightness, colorMode;

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
	if (!oled.spiDepth) {
		kscePervasiveSpiClockEnable(2);
		while (oled.spi->receiveCount)
			(void)oled.spi->receive;
		oled.spi->control = 0x30001;
		oled.spi->enable = 1;
	}
	oled.spiDepth++;
}

static void spiWord(unsigned word) /* name unknown */
{
	oled.txValue |= word << oled.txBits;
	oled.txBits += 9;
	if (oled.txBits >= 16) {
		while (oled.spi->transmitCount == 0x7f)
			;
		oled.spi->transmit = oled.txValue & 0xffff;
		oled.txValue >>= 16;
		oled.txBits -= 16;
	}
}

static void spiPad(void) /* name unknown */
{
	while (oled.txBits)
		spiWord(0);
	while (oled.spi->transmitCount)
		;
	oled.spi->enable = 0;
	while (oled.spi->enable & 1)
		;
}

static void spiEnd(void) /* name unknown */
{
	if (--oled.spiDepth)
		return;
	while (oled.txBits)
		spiWord(0);
	while (oled.spi->transmitCount)
		;
	while (oled.spi->receiveCount)
		(void)oled.spi->receive;
	oled.spi->enable = 0;
	while (oled.spi->enable & 1)
		;
	oled.spi->control = 0x30001;
	(void)oled.spi->control;
	dsb();
	kscePervasiveSpiClockDisable(2);
}

static void spiRead(unsigned char *data, unsigned length, unsigned skip) /* name unknown */
{
	unsigned bits = 0, value = 0, count = 0;
	if (!length)
		return;
	do {
		while (!oled.spi->receiveCount)
			;
		value |= oled.spi->receive << bits;
		bits += 16;
		while (bits > 8) {
			if (skip)
				skip--;
			else
				data[count++] = reverseByte((value >> 1) & 0xff);
			value >>= 9;
			bits -= 9;
		}
	} while (count < length);
}

int SceOledForDriver_2F0C4B67(unsigned char command, const unsigned char *data,
			  unsigned length)
{
	unsigned i;
	if (!oled.ready)
		return OLED_NOT_READY;
	if (oled.ready == 2)
		return OLED_NO_PANEL;
	spiBegin();
	spiWord(reverseByte(command) << 1);
	for (i = 0; i != length; i++)
		spiWord((reverseByte(data[i]) << 1) | 1);
	spiEnd();
	return 0;
}

int SceOledForDriver_3E6AE8A2(unsigned command, unsigned char *data, unsigned length)
{
	unsigned i;
	if (!oled.ready)
		return OLED_NOT_READY;
	if (oled.ready == 2)
		return OLED_NO_PANEL;
	spiBegin();
	if (command < 0xb0) {
		spiWord(reverseByte(command & 0xff) << 1);
	} else {
		spiWord(0x1be);
		spiWord((reverseByte(command & 0xff) << 1) | 1);
		spiWord(0x7e);
	}
	if (length != 0xffffffff)
		for (i = 0; i != length + 1; i++)
			spiWord(1);
	spiPad();
	spiRead(data, length, command < 0xb0 ? 2 : 4);
	spiEnd();
	return 0;
}

static int readPanelId(void) /* name unknown */
{
	unsigned char id[8];
	unsigned i;
	int port = ksceGpioPortRead(0, 0);
	if (!port) {
		ksceGpioPortSet(0, 0);
		sceKernelDelayThread(20000);
	}
	spiBegin();
	spiWord(0x10a);
	for (i = 0; i < 6; i++)
		spiWord(1);
	spiPad();
	spiRead(id, 5, 2);
	spiEnd();
	if (id[4] == 0xff) {
		oled.panelId = id[0] | (id[1] << 8);
		oled.panelRevision = id[2] | (id[3] << 8);
	}
	if (!port)
		ksceGpioPortClear(0, 0);
	if (id[4] == 0xff && !oled.panelId) {
		oled.ready = 1;
		return 0;
	}
	kscePervasiveSpiClockDisable(2);
	kscePervasiveSpiResetEnable(2);
	oled.ready = 2;
	return OLED_NO_PANEL;
}

static int initWork(void *arg) /* name unknown */
{
	readPanelId();
	return 1;
}

int SceOledForDriver_E30604CC(void)
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
			delayFrames = p[1] - 1;
			sequence = p + 2;
			return 0;
		}
		SceOledForDriver_2F0C4B67(p[0], p + 2, p[1]);
		p += 2 + p[1];
	}
	sequence = 0;
	return 0;
}

static void flushSequence(void) /* name unknown */
{
	while (sequence) {
		if (synchronous) {
			SceOledForDriver_E30604CC();
			if (sequence)
				waitTime(16000);
		} else {
			sceKernelDelayThread(16000);
		}
	}
}

static int queueSequence(const unsigned char *p) /* name unknown */
{
	if (sequence)
		return OLED_BUSY;
	sequence = p;
	delayFrames = 0;
	if (synchronous)
		flushSequence();
	return 0;
}

int SceOledForDriver_26F9EEA8(void)
{
	ksceGpioPortClear(0, 0);
	return 0;
}
int SceOledForDriver_6B1E0B52(void)
{
	ksceGpioPortSet(0, 0);
	return 0;
}
int SceOledForDriver_9F4ABDDC(void)
{
	return ksceGpioPortRead(0, 0) == 0;
}
int ksceOledWaitReady(void)
{
	while (!oled.ready)
		waitTime(1000);
	return 0;
}
int ksceOledGetDDB(uint16_t *id, uint16_t *revision)
{
	if (!oled.ready)
		return OLED_NOT_READY;
	if (oled.ready == 2)
		return OLED_NO_PANEL;
	if (id)
		*id = oled.panelId;
	if (revision)
		*revision = oled.panelRevision;
	return 0;
}
int SceOledForDriver_3C990F53(void)
{
	unsigned char value[4];
	int ret = SceOledForDriver_3E6AE8A2(10, value, 1);
	return ret < 0 ? ret : ((value[0] >> 2) & 1);
}
int ksceOledDisplayOn(void)
{
	return queueSequence(PANEL_DATA(0x81001a60));
}
int ksceOledDisplayOff(void)
{
	int ret = queueSequence(PANEL_DATA(0x81001a70));
	if (!ret)
		brightness = 0;
	return ret;
}
int SceOledForDriver_4891257F(void)
{
	return queueSequence(PANEL_DATA(0x81001ab0));
}
int SceOledForDriver_DDB1412B(void)
{
	uint16_t revision;
	const unsigned char *init;
	int ret;
	SceOledForDriver_6B1E0B52();
	if (synchronous)
		waitTime(120000);
	else
		sceKernelDelayThread(120000);
	brightness = 0;
	if (sequence)
		return OLED_BUSY;
	sequence = PANEL_DATA(0x81001dd0);
	delayFrames = 0;
	flushSequence();
	ksceOledGetDDB(0, &revision);
	panelType = revision;
	colorSupported = 1;
	if (panelType == 4) {
		gamma = PANEL_DATA(0x81001ab8);
		init = PANEL_DATA(0x81001d88);
	} else if (panelType == 5) {
		gamma = PANEL_DATA(0x81001c20);
		brightnessControl = PANEL_DATA(0x81001db0);
		init = PANEL_DATA(0x81001a88);
	} else {
		gamma = PANEL_DATA(0x81001e00);
		brightnessControl = PANEL_DATA(0x81001f88);
		init = PANEL_DATA(0x81001a88);
	}
	ret = queueSequence(init);
	if (!ret)
		flushSequence();
	return ret;
}

int ksceOledSetBrightness(unsigned value)
{
	unsigned char *p = brightnessSequence;
	unsigned index, i;
	if (!gamma)
		return OLED_BUSY;
	if (value > 0x10000)
		return OLED_INVALID;
	if (!value) {
		if (sequence)
			return OLED_BUSY;
		sequence = PANEL_DATA(0x81001a70);
		delayFrames = 0;
	} else {
		if (colorSupported && !brightness) {
			for (i = 0; i < 26; i++)
				p[i] = PANEL_DATA(0x81001f68)[i];
			p[26] = 0xb3;
			p[27] = 1;
			p[28] = !!colorMode;
			p += 29;
		}
		p[0] = 0xf9;
		p[1] = 22;
		p[2] = 1;
		index = value == 1 ? 16 : (value == 0x10000 ? 0 : 15 - (value >> 12));
		for (i = 0; i < 21; i++)
			p[3 + i] = gamma[index * 21 + i];
		p[24] = 0xf9;
		p[25] = 1;
		p[26] = 0;
		p[27] = 0x26;
		p[28] = 1;
		p[29] = 0;
		p += 30;
		if (brightnessControl) {
			p[0] = 0xb2;
			p[1] = 1;
			p[2] = panelType == 4 ? 0 : brightnessControl[index];
			p += 3;
		}
		if (!brightness) {
			for (i = 0; i < 11; i++)
				p[i] = PANEL_DATA(0x81001a60)[i];
		} else {
			p[0] = 0xff;
		}
		if (sequence)
			return OLED_BUSY;
		sequence = brightnessSequence;
		delayFrames = 0;
	}
	brightness = value;
	if (synchronous)
		flushSequence();
	return 0;
}
unsigned ksceOledGetBrightness(void)
{
	return brightness;
}
int ksceOledSetDisplayColorSpaceMode(unsigned mode)
{
	int ret;
	if (mode > 1)
		return OLED_INVALID;
	if (!colorSupported)
		return 0;
	if (!brightness) {
		colorMode = mode;
		return 0;
	}
	if (sequence)
		return OLED_BUSY;
	sequence = PANEL_DATA(mode ? 0x81001a80 : 0x81001dc8);
	delayFrames = 0;
	colorMode = mode;
	ret = 0;
	if (synchronous)
		flushSequence();
	return ret;
}
unsigned ksceOledGetDisplayColorSpaceMode(void)
{
	return colorMode;
}
int SceOledForDriver_ED2D6F19(unsigned mode)
{
	synchronous = mode;
	return 0;
}

static __attribute__((noinline)) int resetSequence(void) /* name unknown */
{
	sequence = 0;
	delayFrames = 0;
	return 0;
}
int SceOledForDriver_5AF31932(const unsigned char *p)
{
	if (sequence)
		return OLED_BUSY;
	sequence = p;
	delayFrames = 0;
	return 0;
}
int SceOledForDriver_AD6C7FB8(const unsigned char *p)
{
	int ret = SceOledForDriver_5AF31932(p);
	if (!ret)
		flushSequence();
	return ret;
}
int SceOledForDriver_BF5C5624(unsigned poll)
{
	if (poll)
		return sequence != 0;
	flushSequence();
	return 0;
}

static inline void setupSpi(volatile SceSpiRegisters *regs) /* name unknown */
{
	kscePervasiveSpiResetDisable(2);
	kscePervasiveSpiClockEnable(2);
	regs->control = 0x30001;
	regs->dmaControl = 15;
	regs->intControl = 3;
	regs->mode = 0;
	(void)regs->mode;
	dsb();
	kscePervasiveSpiClockDisable(2);
	ksceGpioSetPortMode(0, 0, 0);
}

static int sysEvent(int resume, int event, void *arg, void *opt) /* name unknown */
{
	if (!resume) {
		if (event == SYSEVENT_SUSPEND_IDLE_QUERY) {
			if (SceOledForDriver_BF5C5624(1))
				return -1;
		} else if (event == SYSEVENT_SUSPEND_DEVICE(3)) {
			kscePervasiveSpiClockDisable(2);
			kscePervasiveSpiResetEnable(2);
			ksceGpioPortClear(0, 0);
		}
	} else if (resume == 1 && event == SYSEVENT_RESUME_DEVICE(3)) {
		setupSpi(oled.spi);
	}
	return 0;
}

static __attribute__((noinline)) int initOled(void) /* name unknown */
{
	SceKernelAllocMemBlockKernelOpt opt;
	volatile SceSpiRegisters *base;
	int ret;

	oled = (OledLink){ 0 };
	resetSequence();
	oled.eventUid = sceKernelRegisterSysEventHandler("SceOled", sysEvent, 0);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = SPI_REG_BASE(2);
	ret = SceSysmemForDriver_C94850C9("SceSpi2Reg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &opt);
	if (ret < 0)
		return ret;
	oled.spiUid = ret;
	SceSysmemForDriver_A841EDDA(ret, (void **)&base);
	oled.spi = base;
	setupSpi(base);
	if ((SceSysrootForDriver_930B1342() & 0xff0000) == 0x800000) {
		ret = readPanelId();
		if (ret < 0) {
			sceKernelUnregisterSysEventHandler(oled.eventUid);
			SceSysmemForDriver_009E1C61(oled.spiUid);
			return ret;
		}
	} else {
		ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceOledInit", initWork, 0);
	}
	return 0;
}

int module_start(unsigned arglen, void *argp)
{
	return initOled() < 0;
}
