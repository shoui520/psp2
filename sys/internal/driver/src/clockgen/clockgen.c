/*
 * SceClockgen: the clock generator, a derivative of the onsemi P1P40167
 * 4-PLL clock generator on I2C bus 0. The module caches its first three
 * registers and writes back only what changes.
 *
 * Export names (HENkaku wiki): 0x06663ECA/0x82F39378 CameraClkEnable/Disable
 * and 0x7B28A724/0xB6F0A532 WlanBtClkEnable/Disable are Sony's (0.940-0.990);
 * the others are guessed and kept as NIDs.
 */

#include <stddef.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/suspend.h>
#include <lowio/i2c.h>
#include <sysmem/sysevent_ids.h>
#include <clockgen/clockgen.h>

#define CLOCKGEN_I2C_BUS	0
#define CLOCKGEN_I2C_ADDR	0xd2
#define CLOCKGEN_REGISTERS	3	/* cached: 0 revision, 1 enables, 2 spread */
#define CLOCKGEN_READ_SIZE	16	/* a block read: count, then the registers */

/* register numbers on the bus (byte reads and writes) */
#define CLOCKGEN_REG(n)		(0x80 + (n))
#define CLOCKGEN_REG_PLL_CONTROL	0x83
#define CLOCKGEN_REG_PLL(n)	(0x84 + (n))

/* register 0: bits 7:4 the revision, 3:0 the vendor */
#define CLOCKGEN_VENDOR(id)	((id) & 0xf)
#define CLOCKGEN_REV2_VENDOR_F	0x2f
#define CLOCKGEN_REV4_VENDOR_F	0x4f

/* register 1: clock enables */
#define CLOCKGEN_AUDIO_48K	0x01	/* audio clock at 24.576 MHz, else 22.5792 */
#define CLOCKGEN_MOTION_CLK	0x02
#define CLOCKGEN_CAMERA_CLK	0x08
#define CLOCKGEN_AUDIO_CLK	0x10
#define CLOCKGEN_WLANBT_CLK	0x80

/* register 2: the spread of the 37 MHz output */
#define CLOCKGEN_SPREAD_MASK	0x07

#define CLOCKGEN_ERROR_INVALID_REGISTER	0x803f0500
#define CLOCKGEN_ERROR_INVALID_FREQUENCY	0x803f0501
#define CLOCKGEN_ERROR_NOT_SUPPORTED	0x803f0502

/* the state; names are ours */
static struct {
	SceUID mutex;
	signed char singleRead;			/* read the registers one by one */
	signed char extended;			/* vendor F, revision 2 or later */
	signed char motionClk;			/* the same: CLOCKGEN_MOTION_CLK exists */
	unsigned char padding;
	unsigned char registers[CLOCKGEN_REGISTERS];
	unsigned char initial[CLOCKGEN_REGISTERS];	/* as read at init */
} clockgen;

/* name unknown: write one register */
static inline int writeRegister(unsigned char reg, unsigned char value)
{
	unsigned char buffer[2];

	buffer[0] = reg;
	buffer[1] = value;
	return sceI2cMasterTransmit(CLOCKGEN_I2C_BUS, CLOCKGEN_I2C_ADDR, buffer, sizeof(buffer));
}

/* name unknown: the PLL settings revision 4 needs for each spread */
static void writePll(void)
{
	unsigned char a, b, c;

	if (clockgen.registers[0] != CLOCKGEN_REV4_VENDOR_F)
		return;
	switch (clockgen.registers[2] & CLOCKGEN_SPREAD_MASK) {
	case 1:
		a = 0xa5;
		b = 0x5f;
		c = 0x89;
		break;
	case 2:
		a = 0xa4;
		b = 0x4d;
		c = 0x7a;
		break;
	case 4:
		a = 0xa7;
		b = 0x7b;
		c = 0x61;
		break;
	case 6:
		a = 0xa3;
		b = 0x37;
		c = 0x46;
		break;
	default:
		return;
	}
	writeRegister(CLOCKGEN_REG_PLL_CONTROL, 0x6d);
	writeRegister(CLOCKGEN_REG_PLL(0), a);
	writeRegister(CLOCKGEN_REG_PLL(1), b);
	writeRegister(CLOCKGEN_REG_PLL(2), c);
}

/* name unknown: whether the vendor's part has the spread spectrum output */
static int supportsSpread(unsigned char id)
{
	switch (CLOCKGEN_VENDOR(id)) {
	case 3:
	case 4:
	case 7:
	case 8:
	case 9:
	case 12:
	case 15:
		return 1;
	default:
		return id == 10;	/* never true: the vendor of 10 is 10 */
	}
}

/* guessed: sceClockgenSetProtocolForDriver */
int SceClockgenForDriver_EFE8A68C(int singleRead)
{
	clockgen.singleRead = singleRead;
	return 0;
}

/* guessed: sceClockgenInitForDriver; reads the registers into the cache */
int SceClockgenForDriver_0BA41732(void)
{
	unsigned char command, response[CLOCKGEN_READ_SIZE];
	int ret, i;

	if (clockgen.singleRead) {
		for (i = 0; i < CLOCKGEN_REGISTERS; i++) {
			command = CLOCKGEN_REG(i);
			ret = sceI2cMasterTransmitReceive(CLOCKGEN_I2C_BUS, CLOCKGEN_I2C_ADDR, &command, 1,
						       CLOCKGEN_I2C_ADDR, response, 1);
			if (ret < 0)
				return ret;
			clockgen.registers[i] = response[0];
			clockgen.initial[i] = response[0];
		}
	} else {
		command = 0;
		ret = sceI2cMasterTransmitReceive(CLOCKGEN_I2C_BUS, CLOCKGEN_I2C_ADDR, &command, 1,
					       CLOCKGEN_I2C_ADDR, response, sizeof(response));
		if (ret < 0)
			return ret;
		for (i = 0; i < CLOCKGEN_REGISTERS && i < response[0]; i++) {
			clockgen.registers[i] = response[i + 1];
			clockgen.initial[i] = response[i + 1];
		}
	}
	clockgen.extended = 0;
	clockgen.motionClk = 0;
	if (CLOCKGEN_VENDOR(clockgen.registers[0]) == 0xf &&
	    clockgen.registers[0] >= CLOCKGEN_REV2_VENDOR_F) {
		clockgen.extended = 1;
		clockgen.motionClk = 1;
	}
	return 0;
}

/* name unknown: restore the registers on resume */
static int systemEvent(int resume, int event, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume != 1)
		return 0;
	if (event == SYSEVENT_RESUME_DEVICE(0)) {
		sceKernelTryLockMutex(clockgen.mutex, 1);
	} else if (event == SYSEVENT_RESUME_START) {
		if (clockgen.registers[0] != CLOCKGEN_REV2_VENDOR_F)
			clockgen.registers[1] &= ~CLOCKGEN_CAMERA_CLK;
		writeRegister(CLOCKGEN_REG(1), clockgen.registers[1]);
		writeRegister(CLOCKGEN_REG(2), clockgen.registers[2]);
		writePll();
		sceKernelUnlockMutex(clockgen.mutex, 1);
	}
	return 0;
}

int module_start(SceSize argc, const void *argv)
{
	int ret;

	(void)argc;
	(void)argv;
	ret = sceKernelCreateMutex("SceClockgen", 0, 0, NULL);
	if (ret < 0)
		return 2;
	clockgen.mutex = ret;
	sceKernelRegisterSysEventHandler("SceClockgen", systemEvent, NULL);
	clockgen.singleRead = 1;
	SceClockgenForDriver_0BA41732();
	if (!supportsSpread(clockgen.registers[0]))
		return 0;
	ret = sceKernelLockMutex(clockgen.mutex, 1, NULL);
	if (ret < 0)
		return 0;
	clockgen.registers[2] = (clockgen.registers[2] & ~CLOCKGEN_SPREAD_MASK) | 1;
	ret = writeRegister(CLOCKGEN_REG(2), clockgen.registers[2]);
	sceKernelUnlockMutex(clockgen.mutex, 1);
	if (ret >= 0)
		writePll();
	return 0;
}

/* guessed: sceClockgenGetRevisionForDriver */
int SceClockgenForDriver_CF63B11D(void)
{
	return clockgen.registers[0];
}

/* guessed: sceClockgenGetRegValueForDriver */
int SceClockgenForDriver_09072366(unsigned int reg)
{
	if (reg >= CLOCKGEN_REGISTERS)
		return CLOCKGEN_ERROR_INVALID_REGISTER;
	return clockgen.registers[reg];
}

/* name unknown: set or clear a clock enable; returns the old state */
static inline int changeBit(unsigned int bit, int enable)
{
	unsigned char value;
	int ret, old;

	ret = sceKernelLockMutex(clockgen.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	old = !!(clockgen.registers[1] & bit);
	value = enable ? clockgen.registers[1] | bit : clockgen.registers[1] & ~bit;
	if (clockgen.registers[1] != value) {
		clockgen.registers[1] = value;
		writeRegister(CLOCKGEN_REG(1), value);
	}
	sceKernelUnlockMutex(clockgen.mutex, 1);
	return old;
}

/* guessed: sceClockgenAudioClkSetSamplingFreqForDriver */
int SceClockgenForDriver_820D3516(int frequency)
{
	if (frequency != 44100 && frequency != 48000)
		return CLOCKGEN_ERROR_INVALID_FREQUENCY;
	return changeBit(CLOCKGEN_AUDIO_48K, frequency == 48000);
}

/* guessed: sceClockgenAudioClkEnableForDriver */
int SceClockgenForDriver_5130682D(void)
{
	return changeBit(CLOCKGEN_AUDIO_CLK, 1);
}

/* guessed: sceClockgenAudioClkDisableForDriver */
int SceClockgenForDriver_F0A0242D(void)
{
	return changeBit(CLOCKGEN_AUDIO_CLK, 0);
}

/* sceClockgenWlanBtClkEnableForDriver */
int sceClockgenWlanBtClkEnable(void)
{
	if (clockgen.registers[0] != CLOCKGEN_REV2_VENDOR_F)
		return 0;
	return changeBit(CLOCKGEN_WLANBT_CLK, 1);
}

int sceClockgenWlanBtClkDisable(void)
{
	if (clockgen.registers[0] != CLOCKGEN_REV2_VENDOR_F)
		return 0;
	return changeBit(CLOCKGEN_WLANBT_CLK, 0);
}

/* sceClockgenCameraClkEnableForDriver */
int SceClockgenForDriver_06663ECA(void)
{
	if (clockgen.registers[0] == CLOCKGEN_REV2_VENDOR_F)
		return 0;
	return changeBit(CLOCKGEN_CAMERA_CLK, 1);
}

/* sceClockgenCameraClkDisableForDriver */
int SceClockgenForDriver_82F39378(void)
{
	if (clockgen.registers[0] == CLOCKGEN_REV2_VENDOR_F)
		return 0;
	return changeBit(CLOCKGEN_CAMERA_CLK, 0);
}

/* guessed: sceClockgenMotionClkEnableForDriver */
int SceClockgenForDriver_62478916(void)
{
	return clockgen.motionClk ? changeBit(CLOCKGEN_MOTION_CLK, 1) : 0;
}

/* guessed: sceClockgenMotionClkDisableForDriver */
int SceClockgenForDriver_66AC5580(void)
{
	return clockgen.motionClk ? changeBit(CLOCKGEN_MOTION_CLK, 0) : 0;
}

/*
 * guessed: sceClockgenSetSpectrumSpreadingForDriver. spread is in tenths of a
 * percent down-spread; negative restores the setting read at init. Returns
 * the spread set.
 */
int SceClockgenForDriver_A2CA8B65(int spread)
{
	int code, ret, result = spread;

	if (spread < 0) {
		code = clockgen.initial[2] & CLOCKGEN_SPREAD_MASK;
		if (code == (clockgen.registers[2] & CLOCKGEN_SPREAD_MASK))
			return spread;
	} else {
		if (!supportsSpread(clockgen.registers[0]))
			return CLOCKGEN_ERROR_NOT_SUPPORTED;
		if (spread <= 1) {
			code = 0;
			result = 0;
		} else if (spread <= 6) {
			code = 1;
			result = 5;
		} else if (CLOCKGEN_VENDOR(clockgen.registers[0]) == 4) {
			if (spread <= 11) {
				code = 2;
				result = 10;
			} else if (spread <= 16) {
				code = 3;
				result = 15;
			} else if (spread <= 21) {
				code = 4;
				result = 20;
			} else if (spread <= 26) {
				code = 5;
				result = 25;
			} else {
				code = 6;
				result = 30;
			}
		} else if (spread <= 14) {
			code = 2;
			result = 10;
		} else if (spread <= 24) {
			code = 4;
			result = 20;
		} else {
			code = 6;
			result = 30;
		}
	}
	ret = sceKernelLockMutex(clockgen.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	clockgen.registers[2] = (clockgen.registers[2] & ~CLOCKGEN_SPREAD_MASK) | code;
	ret = writeRegister(CLOCKGEN_REG(2), clockgen.registers[2]);
	sceKernelUnlockMutex(clockgen.mutex, 1);
	if (ret < 0)
		return ret;
	writePll();
	return result;
}
