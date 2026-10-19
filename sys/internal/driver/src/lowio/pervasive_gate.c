/*
 * ScePervasiveGate: a device's clock runs while its bit is set.
 * Each helper returns whether the bits were set before the change.
 */
#include "lowio_internal.h"

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
