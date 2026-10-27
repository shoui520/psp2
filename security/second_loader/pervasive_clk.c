/*
 * pervasive_clk.c — Pervasive Clock Generator & Voltage Scaling Driver
 *
 * Address range: 0x80878a..0x808a43
 *
 * Configures ARM Cortex-A9, MeP (cMeP), bus, and system clock dividers via
 * 0xe3100000..0xe3110c00 and adjusts the ARM and SoC power rail voltages via
 * Ernie (Syscon) according to the eFuse voltage calibration registers at
 * 0xe3104000..0xe31040ac.
 */

#include "second_loader.h"

/* .rodata: 0x80c3c4..0x80c407 */
static const volatile u32 *const s_armClockStepEfuseRegs[13] = {
	(const volatile u32 *)0xe3104014U,
	(const volatile u32 *)0xe3104000U,
	(const volatile u32 *)0xe3104014U,
	(const volatile u32 *)0xe3104014U,
	(const volatile u32 *)0xe3104010U,
	(const volatile u32 *)0xe310400cU,
	(const volatile u32 *)0xe3104008U,
	(const volatile u32 *)0xe3104004U,
	(const volatile u32 *)0xe3104000U,
	(const volatile u32 *)0xe3104008U,
	(const volatile u32 *)0xe3104000U,
	(const volatile u32 *)0xe3104010U,
	(const volatile u32 *)0xe3104008U
};

static const volatile u32 *const s_socClockStepEfuseRegs[4] = {
	(const volatile u32 *)0xe31040acU,
	(const volatile u32 *)0xe31040acU,
	(const volatile u32 *)0xe31040acU,
	(const volatile u32 *)0xe31040a8U
};

/* .sbss: 0x80db44..0x80db4b */
static u32 s_lastArmCalibVal;
static u32 s_lastSocCalibVal;

/* 0x80878a: pervasiveConfigureArmAndCmepClocks (name ours) */
int pervasiveConfigureArmAndCmepClocks(u32 armClkRatio, int enableDspClk, u32 cmepClkRatio)
{
	u32 val;
	u32 pllGate;

	MMIO_W32(0xe3102000U, 0);
	while (MMIO_R32(0xe3102000U) != 0)
		;

	if (enableDspClk == 1) {
		MMIO_W32(0xe3102004U, 0);
		while (MMIO_R32(0xe3102004U) != 0)
			;
	}

	MMIO_W32(0xe3101000U, 0x1000fU);
	while (MMIO_R32(0xe3101000U) != 0x1000fU)
		;

	if (enableDspClk == 1) {
		MMIO_W32(0xe3101004U, 1);
		while (MMIO_R32(0xe3101004U) != 1)
			;
	}

	val = MMIO_R32(0xe3102180U);
	MMIO_W32(0xe3102180U, val & ~0x80U);
	while (MMIO_R32(0xe3102180U) != (val & ~0x80U))
		;

	MMIO_W32(0xe3101184U, 0);
	while (MMIO_R32(0xe3101184U) != 0)
		;

	MMIO_W32(0xe3102180U, val | 0x80U);
	while (MMIO_R32(0xe3102180U) != (val | 0x80U))
		;

	MMIO_W32(0xe3103000U, 1);
	while (MMIO_R32(0xe3103000U) != 1)
		;

	MMIO_W32(0xe3100030U, 0);
	while (MMIO_R32(0xe3100030U) != 0)
		;

	MMIO_W32(0xe3100050U, 1);
	while (MMIO_R32(0xe3100050U) != 1)
		;

	while (MMIO_R32(0xe31000c0U) != 1)
		;
	MMIO_W32(0xe31000c0U, 1);
	while (MMIO_R32(0xe31000c0U) == 1)
		;

	MMIO_W32(0xe3102000U, 0x00c00000U);
	while (MMIO_R32(0xe3102000U) != 0x00c00000U)
		;

	MMIO_W32(0xe3102000U, 0);
	while (MMIO_R32(0xe3102000U) != 0)
		;

	MMIO_W32(0xe3103000U, armClkRatio & 0x0fU);
	while (MMIO_R32(0xe3103000U) != (armClkRatio & 0x0fU))
		;

	MMIO_W32(0xe3110c00U, cmepClkRatio & 1U);
	while (MMIO_R32(0xe3110c00U) != (cmepClkRatio & 1U))
		;

	pllGate = (enableDspClk == 1) ? 0x00c10000U : 0x00010000U;
	MMIO_W32(0xe3102000U, pllGate);
	while (MMIO_R32(0xe3102000U) == 0)
		;

	if (enableDspClk == 1) {
		MMIO_W32(0xe3102004U, 1);
		while (MMIO_R32(0xe3102004U) != 1)
			;
	}

	return 0;
}

/* 0x808960: pervasiveSetBusClock (name ours) */
void pervasiveSetBusClock(u32 busClkRatio)
{
	u32 val = (busClkRatio & 0x0fU) | 0x10000U;

	MMIO_W32(0xe3103040U, val);
	while (MMIO_R32(0xe3103040U) != val)
		;
}

/* 0x808982: pervasiveEnableSysClock (name ours) */
void pervasiveEnableSysClock(void)
{
	MMIO_W32(0xe3103050U, 1);
	while (MMIO_R32(0xe3103050U) != 1)
		;
}

/* 0x80899c: setArmCoreVoltageForClockStep (name ours) */
int setArmCoreVoltageForClockStep(u32 clockStep)
{
	const volatile u32 *efuseReg = (const volatile u32 *)0xe3104000U;
	u32 calibVal;
	int res;

	if (clockStep - 1U < 13U)
		efuseReg = s_armClockStepEfuseRegs[clockStep - 1U];

	calibVal = *efuseReg;
	if (calibVal == 0)
		return SCE_SBL_ERROR_SL_EINVAL;
	if (calibVal == s_lastArmCalibVal)
		return 0;

	res = sysconSetPowerRailVoltage(2, 0x8aU - calibVal);
	if (res == 0)
		s_lastArmCalibVal = calibVal;
	return res;
}

/* 0x8089f0: setSocVoltageForClockStep (name ours) */
int setSocVoltageForClockStep(u32 clockStep)
{
	const volatile u32 *efuseReg = (const volatile u32 *)0xe31040a4U;
	u32 calibVal;
	int res;

	if (clockStep - 2U < 4U)
		efuseReg = s_socClockStepEfuseRegs[clockStep - 2U];

	calibVal = *efuseReg;
	if (calibVal == 0)
		return SCE_SBL_ERROR_SL_EINVAL;
	if (calibVal == s_lastSocCalibVal)
		return 0;

	res = sysconSetPowerRailVoltage(1, 0x8aU - calibVal);
	if (res == 0)
		s_lastSocCalibVal = calibVal;
	return res;
}
