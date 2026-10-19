/* GRAB secure-monitor services. Their private argument names remain unknown. */
#include "lowio_internal.h"
#include <lowio/grab.h>

uint32_t Smc11B(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc11C(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc11D(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc11EOperation0(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc11EOperation1(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc11EOperation2(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc11F(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t Smc121(uint32_t, uint32_t, uint32_t, uint32_t);

int LowioGrabInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

uint32_t SceGrabForDriver_B22EE8BC(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc11B(a0, a1, a2, a3);
}

uint32_t SceGrabForDriver_188BBCC8(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc11C(a0, a1, a2, a3);
}

uint32_t SceGrabForDriver_E9C25A28(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc11D(a0, a1, a2, a3);
}

uint32_t SceGrabForDriver_072B8D93(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc11EOperation1(a0, a1, a2, a3);
}

uint32_t SceGrabForDriver_379130B2(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc11EOperation2(a0, a1, a2, a3);
}

uint32_t SceGrabForDriver_F13C63DD(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc11EOperation0(a0, a1, a2, a3);
}

uint32_t SceGrabForDriver_5F36ABC4(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc11F(a0, a1, a2, a3);
}

uint32_t SceGrabForDriver_1F292554(uint32_t a0, uint32_t a1,
	uint32_t a2, uint32_t a3)
{
	return Smc121(a0, a1, a2, a3);
}
