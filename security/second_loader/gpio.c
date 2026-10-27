/*
 * gpio.c — Pervasive & Secure GPIO Controller Driver
 *
 * Address range: 0x809422..0x8097ff
 *
 * Manages GPIO port 0 (pervasive GPIO at 0xe20a0000) and port 1 (secure GPIO
 * at 0xe0100000), including pin direction, output set/clear, interrupt sense
 * mode configuration, interrupt mask groups, and latched interrupt status
 * polling (used for the Ernie/Syscon interrupt line on GPIO 0 pin 4).
 */

#include "second_loader.h"

#define SCE_GPIO_ERROR_INVALID_PORT	0x803f0100U
#define SCE_GPIO_ERROR_INVALID_PIN	0x803f0101U
#define SCE_GPIO_ERROR_INVALID_MODE	0x803f0102U

typedef struct SceGpioPortContext { /* name ours, per-port GPIO shadow state */
	volatile u32	*regs;
	u32		dir_shadow;
	u32		int_mode_lo;
	u32		int_mode_hi;
	u32		int_mask[5];
	u32		reserved_mask;
	u32		edge_mask;
	u32		pad_2c;
} SceGpioPortContext;

/* .bss: 0x80d7a8..0x80d807 */
static SceGpioPortContext s_gpioPorts[2];

/* 0x809422: gpioInitPorts (name ours) */
int gpioInitPorts(void)
{
	u32 i;

	memset(s_gpioPorts, 0, sizeof(s_gpioPorts));
	s_gpioPorts[0].regs = (volatile u32 *)0xe20a0000U;
	s_gpioPorts[1].regs = (volatile u32 *)0xe0100000U;

	MMIO_W32(0xe3102100U, 1);
	while (MMIO_R32(0xe3102100U) != 1)
		;

	MMIO_W32(0xe3101100U, 0);
	while (MMIO_R32(0xe3101100U) != 0)
		;

	for (i = 0; i < 2; i++) {
		SceGpioPortContext *port = &s_gpioPorts[i];
		volatile u32 *regs = port->regs;

		port->reserved_mask = 0;
		port->pad_2c = 0;
		port->dir_shadow = regs[0];
		regs[14] = regs[14];
		regs[15] = regs[15];
		regs[16] = regs[16];
		regs[17] = regs[17];
		regs[18] = regs[18];
		port->int_mode_lo = regs[5];
		port->int_mode_hi = regs[6];
		port->int_mask[0] = ~regs[7];
		port->int_mask[1] = ~regs[8];
		port->int_mask[2] = ~regs[9];
		port->int_mask[3] = ~regs[10];
		port->int_mask[4] = ~regs[11];
	}

	return 0;
}

/* 0x80950a: gpioPortSetPin (name ours) */
int gpioPortSetPin(u32 port, u32 pin)
{
	volatile u32 *regs;

	if (port >= 2)
		return (int)SCE_GPIO_ERROR_INVALID_PORT;
	if (pin >= 32)
		return (int)SCE_GPIO_ERROR_INVALID_PIN;

	regs = s_gpioPorts[port].regs;
	regs[2] = (1U << pin);
	(void)regs[13];
	return 0;
}

/* 0x809546: gpioPortClearPin (name ours) */
int gpioPortClearPin(u32 port, u32 pin)
{
	volatile u32 *regs;

	if (port >= 2)
		return (int)SCE_GPIO_ERROR_INVALID_PORT;
	if (pin >= 32)
		return (int)SCE_GPIO_ERROR_INVALID_PIN;

	regs = s_gpioPorts[port].regs;
	regs[3] = (1U << pin);
	(void)regs[13];
	return 0;
}

/* 0x809582: gpioSetPortDirection (name ours) */
u32 gpioSetPortDirection(u32 port, u32 pin, u32 isInput)
{
	u32 bit;
	u32 oldDir;
	u32 newDir;

	if (port >= 2)
		return SCE_GPIO_ERROR_INVALID_PORT;
	if (pin >= 32)
		return SCE_GPIO_ERROR_INVALID_PIN;
	if (isInput >= 2)
		return SCE_GPIO_ERROR_INVALID_MODE;

	bit = 1U << pin;
	oldDir = s_gpioPorts[port].dir_shadow;
	newDir = (oldDir & ~bit) | ((isInput ^ 1U) << pin);
	s_gpioPorts[port].dir_shadow = newDir;
	s_gpioPorts[port].regs[0] = newDir;
	return (oldDir & bit) == 0;
}

/* 0x8095e0: gpioSetInterruptMode (name ours) */
int gpioSetInterruptMode(u32 port, u32 pin, u32 mode)
{
	volatile u32 *regs;
	u32 shift;
	u32 val;

	if (port >= 2)
		return (int)SCE_GPIO_ERROR_INVALID_PORT;
	if (pin >= 32)
		return (int)SCE_GPIO_ERROR_INVALID_PIN;
	if (mode >= 4)
		return (int)SCE_GPIO_ERROR_INVALID_MODE;

	regs = s_gpioPorts[port].regs;
	shift = pin * 2U;
	if (pin < 16) {
		val = (regs[5] & ~(3U << shift)) | (mode << shift);
		regs[5] = val;
		s_gpioPorts[port].int_mode_lo = val;
	} else {
		shift -= 32U;
		val = (regs[6] & ~(3U << shift)) | (mode << shift);
		regs[6] = val;
		s_gpioPorts[port].int_mode_hi = val;
	}

	if (mode - 2U < 2U)
		s_gpioPorts[port].edge_mask &= ~(1U << pin);
	else
		s_gpioPorts[port].edge_mask |= (1U << pin);

	return 0;
}

/* 0x8096a8: gpioReadInterruptStatus (name ours) */
u32 gpioReadInterruptStatus(u32 port, u32 pin)
{
	SceGpioPortContext *ctx;
	volatile u32 *regs;
	u32 pending;

	if (port >= 2)
		return SCE_GPIO_ERROR_INVALID_PORT;
	if (pin >= 32)
		return SCE_GPIO_ERROR_INVALID_PIN;

	ctx = &s_gpioPorts[port];
	regs = ctx->regs;
	pending = (regs[15] & ctx->int_mask[1]) |
		  (regs[14] & ctx->int_mask[0]) |
		  (regs[16] & ctx->int_mask[2]) |
		  (regs[17] & ctx->int_mask[3]) |
		  (regs[18] & ctx->int_mask[4]);
	return ((pending & (1U << pin)) != 0);
}

/* 0x80971e: gpioAckInterruptStatus (name ours) */
u32 gpioAckInterruptStatus(u32 port, u32 pin)
{
	SceGpioPortContext *ctx;
	volatile u32 *regs;
	u32 bit;
	u32 s0, s1, s2, s3, s4;
	u32 m0, m1, m2, m3, m4;

	if (port >= 2)
		return SCE_GPIO_ERROR_INVALID_PORT;
	if (pin >= 32)
		return SCE_GPIO_ERROR_INVALID_PIN;

	bit = 1U << pin;
	ctx = &s_gpioPorts[port];
	regs = ctx->regs;
	m1 = ctx->int_mask[1];
	s0 = regs[14];
	s1 = regs[15];
	s2 = regs[16];
	s3 = regs[17];
	m0 = ctx->int_mask[0];
	s4 = regs[18];
	m2 = ctx->int_mask[2];
	m3 = ctx->int_mask[3];
	m4 = ctx->int_mask[4];

	regs[14] = bit;
	regs[15] = bit;
	regs[16] = bit;
	regs[17] = bit;
	regs[18] = bit;
	(void)regs[18];

	return (((s1 & m1) | (s0 & m0) | (s2 & m2) | (s3 & m3) | (s4 & m4)) & bit) != 0;
}

/* 0x8097ac: gpioEnablePinInterrupt (name ours) */
int gpioEnablePinInterrupt(u32 port, u32 pin)
{
	SceGpioPortContext *ctx;
	u32 mask0;

	if (port >= 2)
		return (int)SCE_GPIO_ERROR_INVALID_PORT;
	if (pin >= 32)
		return (int)SCE_GPIO_ERROR_INVALID_PIN;

	ctx = &s_gpioPorts[port];
	mask0 = ctx->int_mask[0] | (1U << pin);
	ctx->int_mask[0] = mask0;
	ctx->regs[7] = ~(mask0 | ctx->reserved_mask) |
		       (ctx->regs[7] & ctx->reserved_mask);
	return 0;
}
