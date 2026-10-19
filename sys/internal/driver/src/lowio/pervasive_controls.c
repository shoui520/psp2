/* SceLowio pervasive controls. Unresolved exports retain their NID names. */
#include "lowio_internal.h"

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

uint32_t ScePervasiveForDriver_2A563E35(void)
{
	return (uintptr_t)s_pervasive2;
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

uint32_t ScePervasiveForDriver_C0F7CC39(uint32_t level)
{
	return Smc114(0, level);
}

uint32_t ScePervasiveForDriver_09C35980(void)
{
	return Smc114(1, 0);
}

uint32_t ScePervasiveForDriver_7D02215D(uint32_t level)
{
	return Smc114(2, level);
}

uint32_t ScePervasiveForDriver_ECA675E0(void)
{
	return Smc114(3, 0);
}

