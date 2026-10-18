/*
 * ScePower: clock frequencies and rail voltages.
 */

#include "power_private.h"

static inline __attribute__((always_inline)) int raiseClockVoltage(unsigned int rail, uint32_t demand);

static inline __attribute__((always_inline)) void lowerClockVoltage(unsigned int rail, uint32_t demand);

struct PowerClockControl s_powerClockControl;
/*
 * Switch one lane of the bus rail to `selector` at `frequency` MHz.
 * The rail is raised to `demand` first, if needed, and lowered to the
 * smallest demand of the lanes afterwards.
 */
inline int powerSetBusRailClockRaw(unsigned int lane, int frequency,
			    unsigned int selector, uint32_t demand)
{
	uint32_t minimum;
	int ret;

	if (frequency < 0)
		return SCE_POWER_ERROR_INVALID_VALUE;
	ret = powerLockClockControl();
	if (ret < 0)
		return ret;
	ret = raiseClockVoltage(1, demand);
	if (ret < 0)
		return ret;

	switch (lane) {
	case 0:
		ScePervasiveForDriver_C0F7CC39(selector);
		s_powerClockControl.frequencies.busClockFrequency = frequency;
		s_powerClockControl.frequencies.busSelector = selector;
		s_powerClockControl.voltageDemand[0] = demand;
		break;
	case 1:
		if (frequency)
			ScePervasiveForDriver_A6832B33(selector);
		else
			ScePervasiveForDriver_15237AF8(5);
		s_powerClockControl.frequencies.dmac5ClockFrequency = frequency;
		s_powerClockControl.frequencies.dmac5Selector = selector;
		s_powerClockControl.voltageDemand[1] = demand;
		break;
	case 2:
		if (frequency)
			ScePervasiveForDriver_589D0291(selector);
		else
			ScePervasiveForDriver_589D0291(1);
		s_powerClockControl.frequencies.cameraBusClockFrequency = frequency;
		s_powerClockControl.frequencies.cameraBusSelector = selector;
		s_powerClockControl.voltageDemand[2] = demand;
		break;
	case 3:
		ScePervasiveForDriver_98D8914A(selector);
		s_powerClockControl.frequencies.sysClockFrequency = frequency;
		s_powerClockControl.frequencies.sysSelector = selector;
		s_powerClockControl.voltageDemand[3] = demand;
		break;
	}

	minimum = demand;
	if (s_powerClockControl.voltageDemand[4] < minimum)
		minimum = s_powerClockControl.voltageDemand[4];
	if (s_powerClockControl.voltageDemand[0] < minimum)
		minimum = s_powerClockControl.voltageDemand[0];
	if (s_powerClockControl.voltageDemand[1] < minimum)
		minimum = s_powerClockControl.voltageDemand[1];
	if (s_powerClockControl.voltageDemand[2] < minimum)
		minimum = s_powerClockControl.voltageDemand[2];
	if (s_powerClockControl.voltageDemand[3] < minimum)
		minimum = s_powerClockControl.voltageDemand[3];
	lowerClockVoltage(1, minimum);
	powerUnlockClockControl();
	return 0;
}

int powerInitializeClockControl(void)
{
	SceKernelMutexOptParam option = {
		.size = sizeof(option),
		.ceilingPriority = 32,
	};
	SceUID mutex;

	/* Native initializes the original 240-byte prefix, then process flags. */
	memset(&s_powerClockControl, 0, sizeof(s_powerClockControl));
	s_powerClockControl.frequencies.gpuClockFrequency[0] = 111;
	mutex = sceKernelCreateMutex("ScePowerFreq", 6, 0, &option);
	if (mutex < 0)
		return mutex;
	s_powerClockControl.mutex = mutex;
	return 0;
}

int kscePowerSetBusClockFrequency(int frequency)
{
	uint32_t demand = s_powerClockControl.busVoltageTable[2];
	unsigned int selector;

	if (frequency <= 0)
		return SCE_POWER_ERROR_INVALID_VALUE;
	if (frequency <= 55) {
		frequency = 55;
		selector = 2;
	} else if (frequency <= 83) {
		frequency = 83;
		selector = 3;
	} else if (frequency <= 111) {
		frequency = 111;
		selector = 4;
	} else if (frequency <= 166) {
		frequency = 166;
		selector = 5;
		demand = s_powerClockControl.busVoltageTable[1];
	} else if (frequency <= 222) {
		frequency = 222;
		selector = 6;
		demand = s_powerClockControl.busVoltageTable[0];
	} else {
		return SCE_POWER_ERROR_INVALID_VALUE;
	}
	return powerSetBusRailClockRaw(0, frequency, selector, demand);
}

uint32_t kscePowerGetBusClockFrequency(void)
{
	return s_powerClockControl.frequencies.busClockFrequency;
}

int kscePowerSetDmac5ClockFrequency(int frequency)
{
	uint32_t demand = s_powerClockControl.busVoltageTable[2];
	unsigned int selector;

	if (frequency < 0)
		return SCE_POWER_ERROR_INVALID_VALUE;
	if (frequency == 0) {
		selector = 0;
	} else if (frequency <= 41) {
		frequency = 41;
		selector = 1;
	} else if (frequency <= 83) {
		frequency = 83;
		selector = 2;
	} else if (frequency <= 133) {
		frequency = 133;
		selector = 3;
	} else if (frequency <= 166) {
		frequency = 166;
		selector = 4;
		demand = s_powerClockControl.busVoltageTable[0];
	} else {
		return SCE_POWER_ERROR_INVALID_VALUE;
	}
	return powerSetBusRailClockRaw(1, frequency, selector, demand);
}

uint32_t kscePowerGetDmac5ClockFrequency(void)
{
	return s_powerClockControl.frequencies.dmac5ClockFrequency;
}

int kscePowerSetCameraBusClockFrequency(int frequency)
{
	uint32_t demand = s_powerClockControl.busVoltageTable[2];
	unsigned int selector;

	if (frequency < 0)
		return SCE_POWER_ERROR_INVALID_VALUE;
	if (frequency == 0) {
		selector = 0;
	} else if (frequency <= 41) {
		frequency = 41;
		selector = 1;
	} else if (frequency <= 67) {
		frequency = 67;
		selector = 2;
	} else if (frequency <= 83) {
		frequency = 83;
		selector = 3;
	} else if (frequency <= 133) {
		frequency = 133;
		selector = 4;
	} else if (frequency <= 166) {
		frequency = 166;
		selector = 5;
		demand = s_powerClockControl.busVoltageTable[0];
	} else {
		return SCE_POWER_ERROR_INVALID_VALUE;
	}
	return powerSetBusRailClockRaw(2, frequency, selector, demand);
}

uint32_t kscePowerGetCameraBusClockFrequency(void)
{
	return s_powerClockControl.frequencies.cameraBusClockFrequency;
}

/* The system clock keeps the requested frequency; only two selectors exist. */
int kscePowerSetSysClockFrequency(int frequency)
{
	uint32_t demand;
	unsigned int selector;

	if (frequency <= 0)
		return SCE_POWER_ERROR_INVALID_VALUE;
	if (frequency <= 190) {
		selector = 1;
		demand = s_powerClockControl.busVoltageTable[2];
	} else if (frequency <= 222) {
		selector = 0;
		demand = s_powerClockControl.busVoltageTable[0];
	} else {
		return SCE_POWER_ERROR_INVALID_VALUE;
	}
	return powerSetBusRailClockRaw(3, frequency, selector, demand);
}

uint32_t kscePowerGetSysClockFrequency(void)
{
	return s_powerClockControl.frequencies.sysClockFrequency;
}

/* inlined in this unit; the out-of-line copy serves the others */
inline __attribute__((always_inline)) int powerLockClockControl(void)
{
	int ret;

	if (!s_powerClockControl.lockBypass) {
		ret = sceKernelLockMutex(s_powerClockControl.mutex, 1, NULL);
		if (ret < 0)
			return ret;
	}
	return 0;
}

inline __attribute__((always_inline)) void powerUnlockClockControl(void)
{
	if (!s_powerClockControl.lockBypass)
		sceKernelUnlockMutex(s_powerClockControl.mutex, 1);
}

static inline __attribute__((always_inline)) int raiseClockVoltage(unsigned int rail, uint32_t demand)
{
	int ret;

	if (demand < s_powerClockControl.railVoltage[rail - 1]) {
		ret = SceSysconForDriver_7F198FA2(rail, 138 - demand);
		if (ret < 0) {
			powerUnlockClockControl();
			return ret;
		}
		s_powerClockControl.railVoltage[rail - 1] = demand;
	}
	return 0;
}

static inline __attribute__((always_inline)) void lowerClockVoltage(unsigned int rail, uint32_t demand)
{
	if (s_powerClockControl.railVoltage[rail - 1] < demand &&
	    SceSysconForDriver_7F198FA2(rail, 138 - demand) >= 0)
		s_powerClockControl.railVoltage[rail - 1] = demand;
}

int kscePowerSetArmClockFrequency(int frequency)
{
	const unsigned char voltageIndex[16] = {
		0, 5, 0, 5, 5, 4, 3, 2, 1, 0, 2, 0, 4, 2, 1, 0
	};
	unsigned int selector;
	uint32_t demand;
	int ret;

	if (frequency < 1 || frequency > 444)
		return SCE_POWER_ERROR_INVALID_VALUE;
	selector = frequency <= 41 ? 1 : frequency <= 83 ? 3 :
		   frequency <= 111 ? 4 : frequency <= 166 ? 5 :
		   frequency <= 222 ? 6 : frequency <= 333 ? 7 : 14;
	demand = s_powerClockControl.armVoltageTable[voltageIndex[selector]];
	ret = powerLockClockControl();
	if (ret < 0)
		return ret;
	ret = raiseClockVoltage(2, demand);
	if (ret < 0)
		return ret;
	ScePervasiveForDriver_E9D95643(selector, 16);
	if (s_powerClockControl.frequencies.armClockFrequency == 444) {
		if (frequency != 444)
			sceKernelSysrootEnableAutoClockDown();
	} else if (frequency == 444) {
		sceKernelSysrootDisableAutoClockDown();
	}
	s_powerClockControl.frequencies.armClockFrequency = frequency;
	s_powerClockControl.frequencies.armSelector = selector;
	lowerClockVoltage(2, demand);
	powerUnlockClockControl();
	return 0;
}

uint32_t kscePowerGetArmClockFrequency(void)
{
	return s_powerClockControl.frequencies.armClockFrequency;
}

static inline __attribute__((always_inline)) int setMediaClock(unsigned int lane, int frequency)
{
	static const unsigned int rates[] = { 41, 55, 83, 111, 166, 222, 333 };
	uint32_t demand, minimum;
	unsigned int selector, index;
	int ret;

	if (frequency < 0 || frequency > (lane ? 222 : 333))
		return SCE_POWER_ERROR_INVALID_VALUE;
	selector = 0;
	demand = 255;
	if (frequency) {
		for (index = 0; (unsigned int)frequency > rates[index]; index++)
			;
		selector = index + 1;
		index = index < 4 ? (lane ? 2 : 3) : 6 - index - lane;
		demand = s_powerClockControl.mediaVoltageTable[index];
	}
	ret = powerLockClockControl();
	if (ret < 0)
		return ret;
	if (!lane && demand < s_powerClockControl.railVoltage[2] && demand == 255)
		demand = s_powerClockControl.mediaVoltageTable[3];
	ret = raiseClockVoltage(3, demand);
	if (ret < 0)
		return ret;
	if (!lane) {
		if (!selector)
			ScePervasiveForDriver_2EEBE9AE();
		else
			ScePervasiveForDriver_725B4F59(selector);
		s_powerClockControl.frequencies.veneziaClockFrequency = frequency;
		s_powerClockControl.frequencies.veneziaSelector = selector;
	} else {
		if (!selector)
			ScePervasiveForDriver_03E1FAA6();
		else
			ScePervasiveForDriver_12D17D47(selector);
		s_powerClockControl.frequencies.vipClockFrequency = frequency;
		s_powerClockControl.frequencies.vipSelector = selector;
	}
	minimum = s_powerClockControl.voltageDemand[6 - lane];
	if (demand < minimum)
		minimum = demand;
	s_powerClockControl.voltageDemand[5 + lane] = demand;
	if (lane && s_powerClockControl.railVoltage[2] < minimum && minimum == 255) {
		minimum = s_powerClockControl.mediaVoltageTable[3];
		if (SceSysconForDriver_7F198FA2(3, 138 - minimum) >= 0)
			s_powerClockControl.railVoltage[2] = minimum;
	} else {
		lowerClockVoltage(3, minimum);
	}
	powerUnlockClockControl();
	return 0;
}

int kscePowerSetVeneziaClockFrequency(int frequency)
{
	return setMediaClock(0, frequency);
}

uint32_t kscePowerGetVeneziaClockFrequency(void)
{
	return s_powerClockControl.frequencies.veneziaClockFrequency;
}

int kscePowerSetVipClockFrequency(int frequency)
{
	return setMediaClock(1, frequency);
}

uint32_t kscePowerGetVipClockFrequency(void)
{
	return s_powerClockControl.frequencies.vipClockFrequency;
}

static inline __attribute__((always_inline)) int setXbarClock(int frequency, int compat)
{
	uint32_t demand, minimum;
	unsigned int selector, i;
	int ret;

	if (compat) {
		if (frequency > 333)
			return SCE_POWER_ERROR_INVALID_VALUE;
		if (!frequency) {
			demand = 255;
			selector = s_powerClockControl.frequencies.gpuXbarSelector;
		} else {
			selector = frequency == 222 ? 2 : 0;
			demand = s_powerClockControl.compatVoltageTable[frequency == 222];
		}
	} else {
		if (frequency < 1 || frequency > 166)
			return SCE_POWER_ERROR_INVALID_VALUE;
		selector = frequency <= 83 ? 1 : frequency <= 111 ? 2 : 0;
		demand = s_powerClockControl.gpuVoltageTable[selector ? 0 : 1];
	}
	ret = powerLockClockControl();
	if (ret < 0)
		return ret;
	ret = raiseClockVoltage(4, demand);
	if (ret < 0)
		return ret;
	if (compat || !s_powerClockControl.frequencies.compatClockFrequency) {
		ksceDisplayWaitVblankStart();
		ScePervasiveForDriver_7D02215D(selector);
	}
	minimum = demand;
	for (i = 7; i < 10; i++)
		if (i != (unsigned int)(7 + compat) &&
		    s_powerClockControl.voltageDemand[i] < minimum)
			minimum = s_powerClockControl.voltageDemand[i];
	s_powerClockControl.voltageDemand[7 + compat] = demand;
	if (compat) {
		s_powerClockControl.frequencies.compatClockFrequency = frequency;
		s_powerClockControl.frequencies.compatSelector = selector;
	} else {
		s_powerClockControl.frequencies.gpuXbarClockFrequency = frequency;
		s_powerClockControl.frequencies.gpuXbarSelector = selector;
	}
	lowerClockVoltage(4, minimum);
	powerUnlockClockControl();
	return 0;
}

int kscePowerSetGpuXbarClockFrequency(int frequency)
{
	return setXbarClock(frequency, 0);
}

uint32_t kscePowerGetGpuXbarClockFrequency(void)
{
	return s_powerClockControl.frequencies.gpuXbarClockFrequency;
}

int kscePowerSetCompatClockFrequency(int frequency)
{
	return setXbarClock(frequency, 1);
}

uint32_t kscePowerGetCompatClockFrequency(void)
{
	return s_powerClockControl.frequencies.compatClockFrequency;
}

int ScePowerForDriver_711C68DF(void (*before)(uint32_t, uint32_t, int, void *),
			     void *beforeArgument,
			     void (*after)(uint32_t, uint32_t, int, void *),
			     void *afterArgument)
{
	s_powerClockControl.beforeGpuClockChange = before;
	s_powerClockControl.beforeGpuClockArgument = beforeArgument;
	s_powerClockControl.afterGpuClockChange = after;
	s_powerClockControl.afterGpuClockArgument = afterArgument;
	return 0;
}

int kscePowerSetGpuClockFrequencyInternal(int first, int second)
{
	const unsigned int rates[] = { 41, 55, 83, 111, 166, 222 };
	static const unsigned int selectors[] = { 0, 1, 2, 3, 5, 7 };
	uint32_t firstRate, secondRate, firstSelector, secondSelector;
	uint32_t demand, minimum, irqState, start;
	unsigned int index;
	int ret;

	if (first < 0 || second < 0 || first > 222 || second > 222)
		return SCE_POWER_ERROR_INVALID_VALUE;
	firstRate = secondRate = firstSelector = secondSelector = 0;
	demand = 255;
	if (first || second) {
		for (index = 0; (unsigned int)first > rates[index]; index++)
			;
		firstRate = rates[index];
		firstSelector = selectors[index];
		for (index = 0; (unsigned int)second > rates[index]; index++)
			;
		secondRate = rates[index];
		secondSelector = selectors[index];
		if (firstRate > secondRate)
			return SCE_POWER_ERROR_INVALID_VALUE;
		demand = secondRate <= 111 ? s_powerClockControl.gpuVoltageTable[0] :
			 s_powerClockControl.gpuVoltageTable[1];
		if (firstRate == 222)
			demand = s_powerClockControl.compatVoltageTable[0];
	}
	ret = powerLockClockControl();
	if (ret < 0)
		return ret;
	ret = raiseClockVoltage(4, demand);
	if (ret < 0)
		return ret;
	if (!s_powerClockControl.lockBypass && s_powerClockControl.beforeGpuClockChange)
		s_powerClockControl.beforeGpuClockChange(firstRate, secondRate, 0,
						       s_powerClockControl.beforeGpuClockArgument);
	if (!firstRate && !secondRate) {
		ScePervasiveForDriver_CA0ACFC5(0xf000f);
	} else {
		irqState = SceCpuForDriver_821FC0EE();
		SceGrabForDriver_B22EE8BC(0xf00, 0);
		start = SceThreadmgrForDriver_47F6DE49();
		while ((uint32_t)(SceThreadmgrForDriver_47F6DE49() - start) < 3)
			;
		ScePervasiveForDriver_6AF0196F(firstSelector, secondSelector);
		SceGrabForDriver_B22EE8BC(0xf00, 1);
		sceKernelCpuResumeIntr(irqState);
	}
	s_powerClockControl.voltageDemand[9] = demand;
	s_powerClockControl.frequencies.gpuClockFrequency[0] = firstRate;
	s_powerClockControl.frequencies.gpuClockFrequency[1] = secondRate;
	s_powerClockControl.gpuSelector[0] = firstSelector;
	s_powerClockControl.gpuSelector[1] = secondSelector;
	if (!s_powerClockControl.lockBypass && s_powerClockControl.afterGpuClockChange)
		s_powerClockControl.afterGpuClockChange(firstRate, secondRate, 0,
						      s_powerClockControl.afterGpuClockArgument);
	minimum = s_powerClockControl.voltageDemand[8];
	if (s_powerClockControl.voltageDemand[7] < minimum)
		minimum = s_powerClockControl.voltageDemand[7];
	if (s_powerClockControl.voltageDemand[9] < minimum)
		minimum = s_powerClockControl.voltageDemand[9];
	lowerClockVoltage(4, minimum);
	powerUnlockClockControl();
	return 0;
}

int powerInitializeClockRuntime(void)
{
	const ScePervasiveVidRegisters *vid;
	uint32_t revision;
	uint32_t mediaVoltage;
	uint32_t gpuFrequency;

	s_powerClockControl.hardwareInfo = ksceSysconGetHardwareInfo();

	vid = ScePervasiveForDriver_47708C03();
	s_powerClockControl.busVoltageTable[0] = vid->busVoltage[0];
	s_powerClockControl.busVoltageTable[1] = vid->busVoltage[1];
	s_powerClockControl.busVoltageTable[2] = vid->busVoltage[2];
	s_powerClockControl.armVoltageTable[1] = vid->armVoltage[0];
	s_powerClockControl.armVoltageTable[2] = vid->armVoltage[1];
	s_powerClockControl.armVoltageTable[3] = vid->armVoltage[2];
	s_powerClockControl.armVoltageTable[4] = vid->armVoltage[3];
	s_powerClockControl.armVoltageTable[5] = vid->armVoltage[4];
	s_powerClockControl.mediaVoltageTable[0] = vid->mediaVoltage[0];
	s_powerClockControl.mediaVoltageTable[1] = vid->mediaVoltage[1];
	s_powerClockControl.mediaVoltageTable[2] = vid->mediaVoltage[2];
	s_powerClockControl.mediaVoltageTable[3] = vid->mediaVoltage[3];
	s_powerClockControl.compatVoltageTable[0] = vid->compatVoltage[0];
	s_powerClockControl.compatVoltageTable[1] = vid->compatVoltage[1];
	s_powerClockControl.gpuVoltageTable[0] = vid->gpuVoltage[0];

	/* Later SoC revisions carry their own second GPU voltage. */
	revision = kscePervasiveGetSoCRevision();
	if ((revision & 0x1ff00) != 0 || (revision & 0xff) > 0x41) {
		s_powerClockControl.gpuVoltageTable[1] = vid->gpuVoltage[1];
		if (s_powerClockControl.gpuVoltageTable[1] == 0)
			s_powerClockControl.gpuVoltageTable[1] =
				s_powerClockControl.compatVoltageTable[1];
	} else {
		s_powerClockControl.gpuVoltageTable[1] =
			s_powerClockControl.compatVoltageTable[1];
	}

	if (!s_powerClockControl.lockBypass)
		sceKernelLockMutex(s_powerClockControl.mutex, 1, NULL);

	SceSysconForDriver_7F198FA2(1, 0x8a - s_powerClockControl.busVoltageTable[0]);
	s_powerClockControl.railVoltage[0] = s_powerClockControl.busVoltageTable[0];
	if (SceDipswForDriver_A98FC2FD(0x9f))
		s_powerClockControl.voltageDemand[4] =
			s_powerClockControl.busVoltageTable[0];
	else
		s_powerClockControl.voltageDemand[4] =
			s_powerClockControl.busVoltageTable[2];

	SceSysconForDriver_7F198FA2(2, 0x8a - s_powerClockControl.armVoltageTable[2]);
	s_powerClockControl.voltageDemand[6] = 0xff;
	s_powerClockControl.railVoltage[1] = s_powerClockControl.armVoltageTable[2];

	mediaVoltage = s_powerClockControl.mediaVoltageTable[2];
	if (mediaVoltage > 0xff)
		mediaVoltage = 0xff;
	s_powerClockControl.voltageDemand[5] =
		s_powerClockControl.mediaVoltageTable[2];
	SceSysconForDriver_7F198FA2(3, 0x8a - mediaVoltage);
	s_powerClockControl.railVoltage[2] = mediaVoltage;

	s_powerClockControl.voltageDemand[7] = s_powerClockControl.gpuVoltageTable[0];
	s_powerClockControl.voltageDemand[8] = s_powerClockControl.gpuVoltageTable[0];
	s_powerClockControl.voltageDemand[9] = s_powerClockControl.gpuVoltageTable[0];
	SceSysconForDriver_7F198FA2(4, 0x8a - s_powerClockControl.gpuVoltageTable[0]);
	s_powerClockControl.railVoltage[3] = s_powerClockControl.gpuVoltageTable[0];

	powerSetBusRailClockRaw(0, 222, 6, s_powerClockControl.busVoltageTable[0]);
	powerSetBusRailClockRaw(1, 166, 4, s_powerClockControl.busVoltageTable[0]);
	powerSetBusRailClockRaw(2, 0, 0, s_powerClockControl.busVoltageTable[2]);
	powerSetBusRailClockRaw(3, 190, 1, s_powerClockControl.busVoltageTable[2]);
	kscePowerSetArmClockFrequency(333);

	if (SceDipswForDriver_A98FC2FD(0x9f) && SceDipswForDriver_A98FC2FD(0xec))
		gpuFrequency = 166;
	else
		gpuFrequency = 111;
	kscePowerSetGpuXbarClockFrequency(gpuFrequency);
	kscePowerSetGpuClockFrequencyInternal(gpuFrequency, gpuFrequency);
	kscePowerSetCompatClockFrequency(0);

	if (!s_powerClockControl.lockBypass)
		sceKernelUnlockMutex(s_powerClockControl.mutex, 1);
	return 0;
}

/* Resume: put the rails and every clock back where they were at suspend. */
void powerRestoreClockRuntime(void)
{
	uint32_t gpuVoltage;

	s_powerClockControl.lockBypass = 1;
	s_powerClockControl.railVoltage[0] = s_powerClockControl.busVoltageTable[0];
	s_powerClockControl.railVoltage[1] = s_powerClockControl.armVoltageTable[2];
	SceSysconForDriver_7F198FA2(3, 0x8a - s_powerClockControl.railVoltage[2]);

	gpuVoltage = s_powerClockControl.voltageDemand[7];
	if (s_powerClockControl.voltageDemand[8] <= gpuVoltage)
		gpuVoltage = s_powerClockControl.voltageDemand[8];
	if (s_powerClockControl.voltageDemand[9] <= gpuVoltage)
		gpuVoltage = s_powerClockControl.voltageDemand[9];
	SceSysconForDriver_7F198FA2(4, 0x8a - gpuVoltage);
	s_powerClockControl.railVoltage[3] = gpuVoltage;

	kscePowerSetBusClockFrequency(s_powerClockControl.frequencies.busClockFrequency);
	kscePowerSetDmac5ClockFrequency(s_powerClockControl.frequencies.dmac5ClockFrequency);
	kscePowerSetCameraBusClockFrequency(s_powerClockControl.frequencies.cameraBusClockFrequency);
	kscePowerSetSysClockFrequency(s_powerClockControl.frequencies.sysClockFrequency);
	kscePowerSetArmClockFrequency(s_powerClockControl.frequencies.armClockFrequency);
	kscePowerSetVeneziaClockFrequency(s_powerClockControl.frequencies.veneziaClockFrequency);
	kscePowerSetVipClockFrequency(s_powerClockControl.frequencies.vipClockFrequency);
	kscePowerSetGpuXbarClockFrequency(s_powerClockControl.frequencies.gpuXbarClockFrequency);
	kscePowerSetGpuClockFrequencyInternal(s_powerClockControl.frequencies.gpuClockFrequency[0],
					      s_powerClockControl.frequencies.gpuClockFrequency[1]);
	kscePowerSetCompatClockFrequency(0);
	s_powerClockControl.lockBypass = 0;
}

int kscePowerGetGpuClockFrequencyInternal(uint32_t *first, uint32_t *second)
{
	if (first)
		*first = s_powerClockControl.frequencies.gpuClockFrequency[0];
	if (second)
		*second = s_powerClockControl.frequencies.gpuClockFrequency[1];
	return 0;
}

/* name unknown: the clock unit's part of system event 0x4009 (nothing to do) */
int powerClockSysEventHook(void)
{
	return 0;
}
