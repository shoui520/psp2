/*
 * Host commands the RAM code answers itself (0x262c0, 0x26c58, 0x26fdc,
 * 0x27de0): the antenna selection, the SoC register window, the version
 * and the stubs of the commands this firmware does not implement. The
 * filename is inferred.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/event.h"
#include "robin/eventpower.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

#define HOSTCMD_VERSION		0xdf0e4109	/* what the host reads as the firmware version */

/* A register of the SoC and the bits of it the host may change. */
struct SocRegisterMask {
	volatile uint32_t *reg;
	uint32_t mask;
};

/* The registers hostCmd008f reads and writes. */
static struct SocRegisterMask socHostRegisters[9] __attribute__((section(".sqram1.rw"))) = {
	{ (volatile uint32_t *)0x80002064, 0x18000000 },
	{ (volatile uint32_t *)0x80002214, 0xffffffff },
	{ (volatile uint32_t *)0x80002218, 0x00000003 },
	{ (volatile uint32_t *)0x80002248, 0x000000ff },
	{ (volatile uint32_t *)0x8000224c, 0xff000000 },
	{ (volatile uint32_t *)0x80002208, 0x7fffff80 },
	{ (volatile uint32_t *)0x8000220c, 0x36dbfffe },
	{ (volatile uint32_t *)0x80002210, 0x001ffedb },
	{ (volatile uint32_t *)0x80002228, 0x00007fff },
};

/* The host command 0x20 selecting the antenna of the second radio (not used by this chip's first). */
unsigned int hostCmd0020Set(unsigned int secondary, unsigned int antenna)
{
	int level = macGetEventLevel();
	unsigned int posture = sysIrqDisable();
	int value;

	if (level == 1) {
		SOC_REG(0x200) |= 0x28;
		if (antenna == 1) {
			rfCalibManageTable(3, 0);
			rfCalibAcquireTable(3, 1);
			value = 1;
		} else {
			rfCalibManageTable(3, 1);
			rfCalibAcquireTable(3, 1);
			value = 0;
		}
		rfCalibManageTable(5, value);
		rfCalibAcquireTable(5, 1);
	}
	return sysIrqRestore(posture);
}

void hostCmd0096Enable(void)
{
}

void hostCmd0096Disable(void)
{
}

unsigned int hostCmd0096Get(void)
{
	return 0;
}

/* The host command that reads and, with an action of 1, writes the registers of socHostRegisters. */
unsigned int hostCmd008fResolve(uint8_t *body)
{
	uint8_t *data;
	unsigned int i;
	uint32_t value;

	if ((body[0] | (body[1] << 8)) == 1) {
		if ((body[4] | (body[5] << 8)) != 36)
			return 0;
		data = body + 6;
		for (i = 0; i < 9; i++) {
			value = data[0] | (data[1] << 8) | (data[2] << 16) | ((uint32_t)data[3] << 24);
			*socHostRegisters[i].reg = (value & socHostRegisters[i].mask) |
						   (*socHostRegisters[i].reg & ~socHostRegisters[i].mask);
			data += 4;
		}
		SOC_REG(0x20c) = (SOC_REG(0x20c) >> 1) << 1;
		SOC_REG(0x20c) |= 1;
		sysDelayUs(1);
		SOC_REG(0x20c) = (SOC_REG(0x20c) >> 1) << 1;
	}
	data = body + 6;
	for (i = 0; i < 9; i++) {
		value = *socHostRegisters[i].reg & socHostRegisters[i].mask;
		__aeabi_uwrite4(value, data);
		data += 4;
	}
	*(uint16_t *)(body + 4) = 36;
	return 36;
}


unsigned int hostCmd0003Version(void)
{
	return HOSTCMD_VERSION;
}
