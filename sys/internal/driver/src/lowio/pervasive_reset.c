/*
 * ScePervasiveReset: devices held in reset while their bit is set.
 * Each helper returns whether the bits were set before the change.
 */
#include "lowio_internal.h"

/* Take a device out of reset; wait for the write to land. */
static __attribute__((noinline)) int ResetDisable(volatile uint32_t *reg,
	uint32_t mask)
{
	uint32_t state, previous;

	state = SceCpuForDriver_D32ACE9E(&s_pervasiveLock);
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

	state = SceCpuForDriver_D32ACE9E(&s_pervasiveLock);
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
