/* SceLowio pervasive controls. Unresolved exports retain their NID names. */
#include "lowio_internal.h"

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

