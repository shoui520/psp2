/*
 * SceLowio's Pervasive driver, first file: the ScePervasive register mappings
 * and the power-event initialization, the register getters, then the reset
 * and gate controls, in the original's order. Its init calls the clock
 * file's LowioPixelClockInit and PervasiveMiscInit (4 bytes) out of line, and
 * that file calls this one's reset and gate wrappers out of line: at Sony's
 * -O3 that means two files, split where the gate controls end. No record
 * names either file; the names are ours.
 */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <sysmem/sysmem.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/dipsw.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysevent.h>
#include <lowio/pervasive.h>

void LowioIdleEntry(void);

/* Hardware-info registers cached by initialization; the final four names unknown. */
uint32_t s_hardwareInfo[5];

static SceKernelFastMutex s_pervasiveMutex;
static SceUID s_pervasiveUids[16];

static const struct {
	const char *name;
	uint32_t paddr;
	uint32_t size;
	void *base;
} s_pervasiveDevices[16] = {
	{ "ScePervasiveMisc", PERVASIVE_MISC_BASE, PERVASIVE_PAGE_SIZE, &s_misc },
	{ "ScePervasiveReset", PERVASIVE_RESET_BASE, PERVASIVE_PAGE_SIZE, &s_reset },
	{ "ScePervasiveGate", PERVASIVE_GATE_BASE, PERVASIVE_PAGE_SIZE, &s_gate },
	{ "ScePervasiveBaseClk", PERVASIVE_BASECLK_BASE, PERVASIVE_PAGE_SIZE, &s_baseClock },
	{ "ScePervasiveVid", PERVASIVE_VID_BASE, PERVASIVE_PAGE_SIZE, &s_video },
	{ "ScePervasiveMailbox", PERVASIVE_MAILBOX_BASE, PERVASIVE_PAGE_SIZE, &s_mailbox },
	{ "ScePervasiveTas0", PERVASIVE_TAS_BASE(0), PERVASIVE_PAGE_SIZE, &s_tas[0] },
	{ "ScePervasiveTas1", PERVASIVE_TAS_BASE(1), PERVASIVE_PAGE_SIZE, &s_tas[1] },
	{ "ScePervasiveTas2", PERVASIVE_TAS_BASE(2), PERVASIVE_PAGE_SIZE, &s_tas[2] },
	{ "ScePervasiveTas3", PERVASIVE_TAS_BASE(3), PERVASIVE_PAGE_SIZE, &s_tas[3] },
	{ "ScePervasiveTas4", PERVASIVE_TAS_BASE(4), PERVASIVE_PAGE_SIZE, &s_tas[4] },
	{ "ScePervasiveTas5", PERVASIVE_TAS_BASE(5), PERVASIVE_PAGE_SIZE, &s_tas[5] },
	{ "ScePervasiveTas6", PERVASIVE_TAS_BASE(6), PERVASIVE_PAGE_SIZE, &s_tas[6] },
	{ "ScePervasiveTas7", PERVASIVE_TAS_BASE(7), PERVASIVE_PAGE_SIZE, &s_tas[7] },
	{ "ScePervasive2", PERVASIVE2_BASE, PERVASIVE_PAGE_SIZE, &s_pervasive2 },
	{ "SceUartclkReg", UART_CLKGEN_BASE, PERVASIVE_PAGE_SIZE, &s_uartClock },
};

static inline __attribute__((always_inline)) void PervasiveClockInit(void)
{
	unsigned mode;

	LowioPixelClockInit();
	PervasiveMiscInit();
	mode = sceKernelCheckDipsw(DIPSW_GPU_OVERCLOCK) == 0 ? 3 : 5;
	ScePervasiveForDriver_6AF0196F(mode, mode);
}

static int PervasiveSysEvent(int resume, int event, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume == 1 && event == 0x10000)
		PervasiveClockInit();
	return 0;
}

static __attribute__((noinline)) int PervasiveInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned i;
	int ret;

	ret = sceKernelInitializeFastMutex(&s_pervasiveMutex, "ScePervasive",
		2, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < 16; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_pervasiveDevices[i].paddr;
		ret = sceKernelAllocMemBlockForKernel(s_pervasiveDevices[i].name,
			i == 5 ? 0x20100206 : 0x20100806,
			s_pervasiveDevices[i].size, &opt);
		if (ret < 0) {
			unsigned j;

			/* Sony frees the whole UID array, including untouched entries. */
			for (j = 0; j < 16; j++)
				sceKernelFreeMemBlock(s_pervasiveUids[j]);
			sceKernelFinalizeFastMutex(&s_pervasiveMutex);
			return ret;
		}
		s_pervasiveUids[i] = ret;
		sceKernelGetMemBlockBase(ret, (void **)s_pervasiveDevices[i].base);
	}
	s_hardwareInfo[0] = s_misc->revision;
	s_hardwareInfo[1] = s_misc->cacheStatus;
	for (i = 0; i < 3; i++)
		s_hardwareInfo[i + 2] = s_misc->hardwareInfo[i];
	PervasiveClockInit();
	sceKernelRegisterSysEventHandler("ScePervasive", PervasiveSysEvent, NULL);
	SceThreadmgrForDriver_B4A05763(LowioIdleEntry);
	return 0;
}

int LowioPervasiveInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (PervasiveInit() < 0)
		return 2;
	return 0;
}

ScePervasiveMiscRegisters *ScePervasiveForDriver_81A155F1(void)
{
	return s_misc;
}

ScePervasiveVidRegisters *ScePervasiveForDriver_47708C03(void)
{
	return s_video;
}

uint32_t ScePervasiveForDriver_04D145C7(void)
{
	return (uintptr_t)s_mailbox;
}

uint32_t ScePervasiveForDriver_270BE98A(unsigned index)
{
	return index < 8 ? (uintptr_t)s_tas[index] : 0;
}

uint32_t ScePervasiveForDriver_2A563E35(void)
{
	return (uintptr_t)s_pervasive2;
}

uint32_t ScePervasiveForDriver_714EEFB7(void)
{
	return s_hardwareInfo[0];
}

uint32_t ScePervasiveForDriver_41C3C299(void)
{
	return s_hardwareInfo[1];
}

uint32_t ScePervasiveForDriver_FB0EF8C7(void)
{
	return s_hardwareInfo[2];
}

uint32_t ScePervasiveForDriver_443E2A3F(void)
{
	return s_hardwareInfo[3];
}

uint32_t ScePervasiveForDriver_F58EF3A5(void)
{
	return s_hardwareInfo[4];
}

uint32_t ScePervasiveForDriver_84EAB5BC(void)
{
	return Smc110(0, 1);
}

uint32_t ScePervasiveForDriver_E8656ADF(void)
{
	return Smc110(0, 0);
}

uint32_t ScePervasiveForDriver_D58E3A30(void)
{
	return Smc110(1, 1);
}

uint32_t ScePervasiveForDriver_E0711D42(void)
{
	return Smc110(1, 0);
}

uint32_t ScePervasiveForDriver_B2F232E5(uint32_t mode)
{
	return Smc110(2, (mode == 0 ? 1 : 2));
}

uint32_t ScePervasiveForDriver_A7855D3A(void)
{
	return Smc110(2, 3);
}

uint32_t ScePervasiveForDriver_4712740D(void)
{
	return Smc110(2, 0);
}

uint32_t ScePervasiveForDriver_A1D2E440(uint32_t value)
{
	s_misc->audioMode = value;
	return 0;
}

/*
 * ScePervasiveReset: devices held in reset while their bit is set.
 * Each helper returns whether the bits were set before the change.
 */

/* Take a device out of reset; wait for the write to land. */
static __attribute__((noinline)) int ResetDisable(volatile uint32_t *reg,
	uint32_t mask)
{
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	*reg = previous & ~mask;
	LowioDmb();
	(void)*reg;
	LowioDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return (previous & mask) != 0;
}

static __attribute__((noinline)) int ResetDisableDevice(volatile uint32_t *reg)
{
	return ResetDisable(reg, 1);
}

static __attribute__((noinline)) int ResetDisableMask(volatile uint32_t *reg,
	uint32_t mask)
{
	return ResetDisable(reg, mask);
}

/* Put a device in reset. */
static __attribute__((noinline)) int ResetEnable(volatile uint32_t *reg,
	uint32_t mask)
{
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	*reg = previous | mask;
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return (previous & mask) != 0;
}

static __attribute__((noinline)) int ResetEnableDevice(volatile uint32_t *reg)
{
	return ResetEnable(reg, 1);
}

static __attribute__((noinline)) int ResetEnableMask(volatile uint32_t *reg,
	uint32_t mask)
{
	return ResetEnable(reg, mask);
}

int ScePervasiveForDriver_3E79D3D3(void)
{
	return ResetEnableDevice(&s_reset->gpu);
}

int ScePervasiveForDriver_8A85E36B(void)
{
	return ResetDisableDevice(&s_reset->gpu);
}

int ScePervasiveForDriver_377126CD(void)
{
	return Smc111(&s_reset->allegrex, 1);
}

int ScePervasiveForDriver_6E11EB97(void)
{
	return Smc111(&s_reset->allegrex, 0);
}

int ScePervasiveForDriver_7B0F388B(void)
{
	return Smc111(&s_reset->sonyRegbus, 1);
}

int ScePervasiveForDriver_4CCD40E6(void)
{
	return Smc111(&s_reset->sonyRegbus, 0);
}

int ScePervasiveForDriver_7C285361(void)
{
	return Smc111(&s_reset->compatRam, 1);
}

int ScePervasiveForDriver_E40BED0F(void)
{
	return Smc111(&s_reset->compatRam, 0);
}

int ScePervasiveForDriver_28731EC5(void)
{
	return ResetEnableDevice(&s_reset->venezia);
}

int ScePervasiveForDriver_A7E64C6F(void)
{
	return ResetDisableDevice(&s_reset->venezia);
}

int ScePervasiveForDriver_31C0A98B(void)
{
	return ResetEnableDevice(&s_reset->vip);
}

int ScePervasiveForDriver_E2D8F6C3(void)
{
	return ResetDisableDevice(&s_reset->vip);
}

int ScePervasiveForDriver_F345FB53(int port)
{
	volatile uint32_t *sdio = s_reset->sdio;

	return Smc111(&sdio[port], 1);
}

int ScePervasiveForDriver_EDCA3961(int port)
{
	volatile uint32_t *sdio = s_reset->sdio;

	return Smc111(&sdio[port], 0);
}

int ScePervasiveForDriver_FF4B3CA6(int port)
{
	volatile uint32_t *dmac = s_reset->dmac;

	return ResetEnableDevice(&dmac[port]);
}

int ScePervasiveForDriver_AC475961(int port)
{
	volatile uint32_t *dmac = s_reset->dmac;

	return ResetDisableDevice(&dmac[port]);
}

int ScePervasiveForDriver_66BF2885(int port)
{
	volatile uint32_t *csi = s_reset->csi;

	return ResetEnableDevice(&csi[port]);
}

int ScePervasiveForDriver_D3B59C33(int port)
{
	volatile uint32_t *csi = s_reset->csi;

	return ResetDisableDevice(&csi[port]);
}

int ScePervasiveForDriver_E3FC1C8D(int port, unsigned int mask)
{
	volatile uint32_t *dsi = s_reset->dsi;

	return ResetEnableMask(&dsi[port], mask);
}

int ScePervasiveForDriver_FFB43AC2(int port, unsigned int mask)
{
	volatile uint32_t *dsi = s_reset->dsi;

	return ResetDisableMask(&dsi[port], mask);
}

int ScePervasiveForDriver_B68254AD(void)
{
	return ResetEnableDevice(&s_reset->iftu);
}

int ScePervasiveForDriver_E92E28FF(void)
{
	return ResetDisableDevice(&s_reset->iftu);
}

int ScePervasiveForDriver_7AE2F8E8(void)
{
	return ResetEnableDevice(&s_reset->unidentifiedDevice);
}

int ScePervasiveForDriver_17109C28(void)
{
	return ResetDisableDevice(&s_reset->unidentifiedDevice);
}

int ScePervasiveForDriver_4AF7A01E(int port, unsigned int mask)
{
	volatile uint32_t *usb = s_reset->usb;

	return ResetEnableMask(&usb[port], mask);
}

int ScePervasiveForDriver_13CC07C9(int port, unsigned int mask)
{
	volatile uint32_t *usb = s_reset->usb;

	return ResetDisableMask(&usb[port], mask);
}

int ScePervasiveForDriver_3D8A8712(int port)
{
	volatile uint32_t *sdif = s_reset->sdif;

	return ResetEnableDevice(&sdif[port]);
}

int ScePervasiveForDriver_D326E15F(int port)
{
	volatile uint32_t *sdif = s_reset->sdif;

	return ResetDisableDevice(&sdif[port]);
}

int ScePervasiveForDriver_A3569FF1(void)
{
	return ResetEnableDevice(&s_reset->msif);
}

int ScePervasiveForDriver_CB0F15CD(void)
{
	return ResetDisableDevice(&s_reset->msif);
}

int ScePervasiveForDriver_146108D4(int port)
{
	volatile uint32_t *i2s = s_reset->i2s;

	return ResetEnableDevice(&i2s[port]);
}

int ScePervasiveForDriver_EEE340ED(int port)
{
	volatile uint32_t *i2s = s_reset->i2s;

	return ResetDisableDevice(&i2s[port]);
}

int ScePervasiveForDriver_821AE6C7(int port)
{
	volatile uint32_t *srcMix = s_reset->srcMix;

	return ResetEnableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_5BAFA0DC(int port)
{
	volatile uint32_t *srcMix = s_reset->srcMix;

	return ResetDisableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_B8AF1249(void)
{
	return ResetEnableDevice(&s_reset->spdif);
}

int ScePervasiveForDriver_6A2FB697(void)
{
	return ResetDisableDevice(&s_reset->spdif);
}

int ScePervasiveForDriver_731A097D(void)
{
	return ResetEnableDevice(&s_reset->gpio);
}

int ScePervasiveForDriver_78C34032(void)
{
	return ResetDisableDevice(&s_reset->gpio);
}

int ScePervasiveForDriver_7B16F900(int port)
{
	volatile uint32_t *spi = s_reset->spi;

	return ResetEnableDevice(&spi[port]);
}

int ScePervasiveForDriver_E4B145AE(int port)
{
	volatile uint32_t *spi = s_reset->spi;

	return ResetDisableDevice(&spi[port]);
}

int ScePervasiveForDriver_41F9E1C3(int port)
{
	volatile uint32_t *i2c = s_reset->i2c;

	return ResetEnableDevice(&i2c[port]);
}

int ScePervasiveForDriver_86BEA9A4(int port)
{
	volatile uint32_t *i2c = s_reset->i2c;

	return ResetDisableDevice(&i2c[port]);
}

int ScePervasiveForDriver_788B6C61(int port)
{
	volatile uint32_t *uart = s_reset->uart;

	return ResetEnableDevice(&uart[port]);
}

int ScePervasiveForDriver_A7CE7DCC(int port)
{
	volatile uint32_t *uart = s_reset->uart;

	return ResetDisableDevice(&uart[port]);
}

int ScePervasiveForDriver_EBE9C84E(void)
{
	return Smc111(&s_reset->vipRegisterAccess, 1);
}

int ScePervasiveForDriver_8CF567AD(void)
{
	return Smc111(&s_reset->vipRegisterAccess, 0);
}

static inline __attribute__((always_inline)) uint32_t MiscReplaceLocked(
	volatile uint32_t *reg, uint32_t value)
{
	uint32_t intr, previous;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = MiscReplace(reg, value);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, intr);
	return previous;
}

uint32_t ScePervasiveForDriver_F1908D3C(unsigned head)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	if (head != 0)
		return MiscReplaceLocked(&misc->dsi1Control, 1);
	return MiscReplaceLocked(&misc->dsi0Control, 1);
}

uint32_t ScePervasiveForDriver_350BF570(unsigned head)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	if (head != 0)
		return MiscReplaceLocked(&misc->dsi1Control, 0);
	return MiscReplaceLocked(&misc->dsi0Control, 0);
}

uint32_t ScePervasiveForDriver_0C1A7FFA(void)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	return MiscReplaceLocked(&misc->csiControl, 1);
}

uint32_t ScePervasiveForDriver_5B4EF352(void)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	return MiscReplaceLocked(&misc->csiControl, 0);
}

/*
 * ScePervasiveGate: a device's clock runs while its bit is set.
 * Each helper returns whether the bits were set before the change.
 */

/* Start a device's clock; wait for the write to land. */
static __attribute__((noinline)) int GateEnable(volatile uint32_t *reg,
	uint32_t mask)
{
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	*reg = previous | mask;
	LowioDmb();
	(void)*reg;
	LowioDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return (previous & mask) != 0;
}

static __attribute__((noinline)) int GateEnableDevice(volatile uint32_t *reg)
{
	return GateEnable(reg, 1);
}

/* Stop a device's clock; wait for the write to land. */
static __attribute__((noinline)) int GateDisable(volatile uint32_t *reg,
	uint32_t mask)
{
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	*reg = previous & ~mask;
	LowioDmb();
	(void)*reg;
	LowioDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return (previous & mask) != 0;
}

static __attribute__((noinline)) int GateDisableDevice(volatile uint32_t *reg)
{
	return GateDisable(reg, 1);
}

int ScePervasiveForDriver_39E51AE2(unsigned int mask)
{
	return GateEnable(&s_gate->gpu, mask & 0xf000f);
}

int ScePervasiveForDriver_CA0ACFC5(unsigned int mask)
{
	return GateDisable(&s_gate->gpu, mask & 0xf000f);
}

int ScePervasiveForDriver_8EE3AEDF(void)
{
	return Smc113(&s_gate->allegrex, 1);
}

int ScePervasiveForDriver_3BF2A9B5(void)
{
	return Smc113(&s_gate->allegrex, 0);
}

int ScePervasiveForDriver_7F4AB4AA(void)
{
	return Smc113(&s_gate->sonyRegbus, 1);
}

int ScePervasiveForDriver_0EBBE8DE(void)
{
	return Smc113(&s_gate->sonyRegbus, 0);
}

int ScePervasiveForDriver_B2EE45C9(void)
{
	return Smc113(&s_gate->compatRam, 1);
}

int ScePervasiveForDriver_39979C55(void)
{
	return Smc113(&s_gate->compatRam, 0);
}

int ScePervasiveForDriver_FB01A2DD(void)
{
	return GateEnable(&s_gate->venezia, 1);
}

int ScePervasiveForDriver_2EEBE9AE(void)
{
	return GateDisable(&s_gate->venezia, 1);
}

int ScePervasiveForDriver_B9904983(void)
{
	return GateEnable(&s_gate->venezia, 2);
}

int ScePervasiveForDriver_FBE5AA2F(void)
{
	return GateDisable(&s_gate->venezia, 2);
}

int ScePervasiveForDriver_B1CFA18F(void)
{
	return GateEnableDevice(&s_gate->vip);
}

int ScePervasiveForDriver_03E1FAA6(void)
{
	return GateDisableDevice(&s_gate->vip);
}

int ScePervasiveForDriver_D34E74C5(int port)
{
	volatile uint32_t *sdio = s_gate->sdio;

	return Smc113(&sdio[port], 1);
}

int ScePervasiveForDriver_701D5355(int port)
{
	volatile uint32_t *sdio = s_gate->sdio;

	return Smc113(&sdio[port], 0);
}

int ScePervasiveForDriver_A10583FF(int port)
{
	volatile uint32_t *dmac = s_gate->dmac;

	return Smc113(&dmac[port], 1);
}

int ScePervasiveForDriver_15237AF8(int port)
{
	volatile uint32_t *dmac = s_gate->dmac;

	return Smc113(&dmac[port], 0);
}

int ScePervasiveForDriver_1068C067(int port)
{
	volatile uint32_t *csi = s_gate->csi;

	return GateEnableDevice(&csi[port]);
}

int ScePervasiveForDriver_4B223AF5(int port)
{
	volatile uint32_t *csi = s_gate->csi;

	return GateDisableDevice(&csi[port]);
}

int ScePervasiveForDriver_BC42C72F(int port, unsigned int mask)
{
	volatile uint32_t *dsi = s_gate->dsi;

	return GateEnable(&dsi[port], mask);
}

int ScePervasiveForDriver_25AE181E(int port, unsigned int mask)
{
	volatile uint32_t *dsi = s_gate->dsi;

	return GateDisable(&dsi[port], mask);
}

int ScePervasiveForDriver_07F2A738(void)
{
	return GateEnableDevice(&s_gate->iftu);
}

int ScePervasiveForDriver_5AFE0AF0(void)
{
	return GateDisableDevice(&s_gate->iftu);
}

int ScePervasiveForDriver_C0C842FE(void)
{
	return GateEnableDevice(&s_gate->unidentifiedDevice);
}

int ScePervasiveForDriver_9BB7B932(void)
{
	return GateDisableDevice(&s_gate->unidentifiedDevice);
}

int ScePervasiveForDriver_A2EFD7AF(int port, unsigned int mask)
{
	volatile uint32_t *usb = s_gate->usb;

	return GateEnable(&usb[port], mask);
}

int ScePervasiveForDriver_AD1E81EB(int port, unsigned int mask)
{
	volatile uint32_t *usb = s_gate->usb;

	return GateDisable(&usb[port], mask);
}

int ScePervasiveForDriver_3CF7A012(int port)
{
	volatile uint32_t *sdif = s_gate->sdif;

	return GateEnableDevice(&sdif[port]);
}

int ScePervasiveForDriver_4E43299D(int port)
{
	volatile uint32_t *sdif = s_gate->sdif;

	return GateDisableDevice(&sdif[port]);
}

int ScePervasiveForDriver_7704C013(void)
{
	return GateEnableDevice(&s_gate->msif);
}

int ScePervasiveForDriver_2A9778CD(void)
{
	return GateDisableDevice(&s_gate->msif);
}

int ScePervasiveForDriver_4235EB7D(int port)
{
	volatile uint32_t *i2s = s_gate->i2s;

	return GateEnableDevice(&i2s[port]);
}

int ScePervasiveForDriver_A3CF9844(int port)
{
	volatile uint32_t *i2s = s_gate->i2s;

	return GateDisableDevice(&i2s[port]);
}

int ScePervasiveForDriver_47F2C35C(int port)
{
	volatile uint32_t *srcMix = s_gate->srcMix;

	return GateEnableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_F861FEE4(int port)
{
	volatile uint32_t *srcMix = s_gate->srcMix;

	return GateDisableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_6B5DEDA2(void)
{
	return GateEnableDevice(&s_gate->spdif);
}

int ScePervasiveForDriver_51981FEE(void)
{
	return GateDisableDevice(&s_gate->spdif);
}

int ScePervasiveForDriver_2F195C97(void)
{
	return GateEnableDevice(&s_gate->gpio);
}

int ScePervasiveForDriver_EB176898(void)
{
	return GateDisableDevice(&s_gate->gpio);
}

int ScePervasiveForDriver_DFD96BFC(int port)
{
	volatile uint32_t *spi = s_gate->spi;

	return GateEnableDevice(&spi[port]);
}

int ScePervasiveForDriver_A85BF98A(int port)
{
	volatile uint32_t *spi = s_gate->spi;

	return GateDisableDevice(&spi[port]);
}

int ScePervasiveForDriver_C37CFDFC(int port)
{
	volatile uint32_t *i2c = s_gate->i2c;

	return GateEnableDevice(&i2c[port]);
}

int ScePervasiveForDriver_3F3E1AEA(int port)
{
	volatile uint32_t *i2c = s_gate->i2c;

	return GateDisableDevice(&i2c[port]);
}

int ScePervasiveForDriver_EFD084D8(int port)
{
	volatile uint32_t *uart = s_gate->uart;

	return GateEnableDevice(&uart[port]);
}

int ScePervasiveForDriver_18DD8043(int port)
{
	volatile uint32_t *uart = s_gate->uart;

	return GateDisableDevice(&uart[port]);
}
