/* SceLowio pervasive controls. Unresolved exports retain their NID names. */
#include "lowio_internal.h"

static unsigned ChangeBits(volatile uint32_t *reg, uint32_t mask,
	unsigned set, unsigned synchronize)
{
	uint32_t state, previous;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	*reg = set ? previous | mask : previous & ~mask;
	LowioDmb();
	if (synchronize) {
		(void)*reg;
		LowioDsb();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return (previous & mask) != 0;
}

uint32_t ScePervasiveForDriver_81A155F1(void)
{
	return (uintptr_t)s_misc;
}

uint32_t ScePervasiveForDriver_47708C03(void)
{
	return (uintptr_t)s_video;
}

uint32_t ScePervasiveForDriver_04D145C7(void)
{
	return (uintptr_t)s_mailbox;
}

uint32_t ScePervasiveForDriver_2A563E35(void)
{
	return (uintptr_t)s_pervasive2;
}

uint32_t ScePervasiveForDriver_84EAB5BC(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc110(0, 1, a2, a3);
}

uint32_t ScePervasiveForDriver_E8656ADF(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc110(0, 0, a2, a3);
}

uint32_t ScePervasiveForDriver_D58E3A30(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc110(1, 1, a2, a3);
}

uint32_t ScePervasiveForDriver_E0711D42(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc110(1, 0, a2, a3);
}

uint32_t ScePervasiveForDriver_B2F232E5(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc110(2, (a0 == 0 ? 1 : 2), a2, a3);
}

uint32_t ScePervasiveForDriver_A7855D3A(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc110(2, 3, a2, a3);
}

uint32_t ScePervasiveForDriver_4712740D(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc110(2, 0, a2, a3);
}

uint32_t ScePervasiveForDriver_3E79D3D3(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x10), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_8A85E36B(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x10), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_377126CD(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x20, 1, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_6E11EB97(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x20, 0, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_7B0F388B(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x24, 1, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_4CCD40E6(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x24, 0, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_7C285361(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x28, 1, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_E40BED0F(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x28, 0, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_28731EC5(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x30), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_A7E64C6F(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x30), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_31C0A98B(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x34), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_E2D8F6C3(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x34), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_F345FB53(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x40 + a0 * 4, 1, a2, (uintptr_t)s_reset + 0x40);
}

uint32_t ScePervasiveForDriver_EDCA3961(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x40 + a0 * 4, 0, a2, (uintptr_t)s_reset + 0x40);
}

uint32_t ScePervasiveForDriver_FF4B3CA6(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x50 + a0 * 4), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_AC475961(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x50 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_66BF2885(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x70 + a0 * 4), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_D3B59C33(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x70 + a0 * 4), 1,
		0, 1);
}

uint32_t kscePervasiveDsiResetEnable(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x80 + a0 * 4), a1,
		1, 0);
}

uint32_t kscePervasiveDsiResetDisable(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x80 + a0 * 4), a1,
		0, 1);
}

uint32_t ScePervasiveForDriver_B68254AD(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x88), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_E92E28FF(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x88), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_7AE2F8E8(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x8c), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_17109C28(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x8c), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_4AF7A01E(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x90 + a0 * 4), a1,
		1, 0);
}

uint32_t ScePervasiveForDriver_13CC07C9(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x90 + a0 * 4), a1,
		0, 1);
}

uint32_t ScePervasiveForDriver_3D8A8712(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xa0 + a0 * 4), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_D326E15F(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xa0 + a0 * 4), 1,
		0, 1);
}

uint32_t kscePervasiveMsifResetEnable(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xb0), 1,
		1, 0);
}

uint32_t kscePervasiveMsifResetDisable(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xb0), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_146108D4(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xc0 + a0 * 4), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_EEE340ED(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xc0 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_821AE6C7(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xe0 + a0 * 4), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_5BAFA0DC(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xe0 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_B8AF1249(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xf0), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_6A2FB697(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0xf0), 1,
		0, 1);
}

uint32_t kscePervasiveGpioResetEnable(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x100), 1,
		1, 0);
}

uint32_t kscePervasiveGpioResetDisable(void)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x100), 1,
		0, 1);
}

uint32_t kscePervasiveSpiResetEnable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x104 + a0 * 4), 1,
		1, 0);
}

uint32_t kscePervasiveSpiResetDisable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x104 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_41F9E1C3(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x110 + a0 * 4), 1,
		1, 0);
}

uint32_t ScePervasiveForDriver_86BEA9A4(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x110 + a0 * 4), 1,
		0, 1);
}

uint32_t kscePervasiveUartResetEnable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x120 + a0 * 4), 1,
		1, 0);
}

uint32_t kscePervasiveUartResetDisable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_reset + 0x120 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_EBE9C84E(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x180, 1, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_8CF567AD(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc111((uintptr_t)s_reset + 0x180, 0, a2, (uintptr_t)&s_reset);
}

uint32_t ScePervasiveForDriver_39E51AE2(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x10), a0 & 0xf000f,
		1, 1);
}

uint32_t ScePervasiveForDriver_CA0ACFC5(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x10), a0 & 0xf000f,
		0, 1);
}

uint32_t ScePervasiveForDriver_8EE3AEDF(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x20, 1, a2, (uintptr_t)&s_gate);
}

uint32_t ScePervasiveForDriver_3BF2A9B5(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x20, 0, a2, (uintptr_t)&s_gate);
}

uint32_t ScePervasiveForDriver_7F4AB4AA(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x24, 1, a2, (uintptr_t)&s_gate);
}

uint32_t ScePervasiveForDriver_0EBBE8DE(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x24, 0, a2, (uintptr_t)&s_gate);
}

uint32_t ScePervasiveForDriver_B2EE45C9(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x28, 1, a2, (uintptr_t)&s_gate);
}

uint32_t ScePervasiveForDriver_39979C55(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x28, 0, a2, (uintptr_t)&s_gate);
}

uint32_t ScePervasiveForDriver_FB01A2DD(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x30), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_2EEBE9AE(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x30), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_B9904983(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x30), 2,
		1, 1);
}

uint32_t ScePervasiveForDriver_FBE5AA2F(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x30), 2,
		0, 1);
}

uint32_t ScePervasiveForDriver_B1CFA18F(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x34), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_03E1FAA6(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x34), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_D34E74C5(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x40 + a0 * 4, 1, a2, (uintptr_t)s_gate + 0x40);
}

uint32_t ScePervasiveForDriver_701D5355(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x40 + a0 * 4, 0, a2, (uintptr_t)s_gate + 0x40);
}

uint32_t ScePervasiveForDriver_A10583FF(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x50 + a0 * 4, 1, a2, (uintptr_t)s_gate + 0x50);
}

uint32_t ScePervasiveForDriver_15237AF8(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc113((uintptr_t)s_gate + 0x50 + a0 * 4, 0, a2, (uintptr_t)s_gate + 0x50);
}

uint32_t ScePervasiveForDriver_1068C067(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x70 + a0 * 4), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_4B223AF5(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x70 + a0 * 4), 1,
		0, 1);
}

uint32_t kscePervasiveDsiClockEnable(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x80 + a0 * 4), a1,
		1, 1);
}

uint32_t kscePervasiveDsiClockDisable(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x80 + a0 * 4), a1,
		0, 1);
}

uint32_t ScePervasiveForDriver_07F2A738(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x88), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_5AFE0AF0(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x88), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_C0C842FE(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x8c), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_9BB7B932(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x8c), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_A2EFD7AF(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x90 + a0 * 4), a1,
		1, 1);
}

uint32_t ScePervasiveForDriver_AD1E81EB(unsigned a0, unsigned a1)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x90 + a0 * 4), a1,
		0, 1);
}

uint32_t ScePervasiveForDriver_3CF7A012(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xa0 + a0 * 4), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_4E43299D(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xa0 + a0 * 4), 1,
		0, 1);
}

uint32_t kscePervasiveMsifClockEnable(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xb0), 1,
		1, 1);
}

uint32_t kscePervasiveMsifClockDisable(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xb0), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_4235EB7D(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xc0 + a0 * 4), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_A3CF9844(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xc0 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_47F2C35C(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xe0 + a0 * 4), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_F861FEE4(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xe0 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_6B5DEDA2(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xf0), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_51981FEE(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0xf0), 1,
		0, 1);
}

uint32_t kscePervasiveGpioClockEnable(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x100), 1,
		1, 1);
}

uint32_t kscePervasiveGpioClockDisable(void)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x100), 1,
		0, 1);
}

uint32_t kscePervasiveSpiClockEnable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x104 + a0 * 4), 1,
		1, 1);
}

uint32_t kscePervasiveSpiClockDisable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x104 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_C37CFDFC(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x110 + a0 * 4), 1,
		1, 1);
}

uint32_t ScePervasiveForDriver_3F3E1AEA(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x110 + a0 * 4), 1,
		0, 1);
}

uint32_t kscePervasiveUartClockEnable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x120 + a0 * 4), 1,
		1, 1);
}

uint32_t kscePervasiveUartClockDisable(unsigned a0)
{
	return ChangeBits((volatile uint32_t *)(s_gate + 0x120 + a0 * 4), 1,
		0, 1);
}

uint32_t ScePervasiveForDriver_C0F7CC39(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc114(0, a0, a2, a3);
}

uint32_t ScePervasiveForDriver_09C35980(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc114(1, 0, a2, a3);
}

uint32_t ScePervasiveForDriver_7D02215D(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc114(2, a0, a2, a3);
}

uint32_t ScePervasiveForDriver_ECA675E0(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc114(3, 0, a2, a3);
}

