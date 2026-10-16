/*
 * power_tzs.c: monitor call 0x11a, sceSysconSetPowerMode (henkaku wiki;
 * SceSyscon's sceSysconSetPowerModeForDriver issues it). The last syscon
 * command, power off, suspend, reset or hibernate, must reach the syscon
 * after the CDRAM is in self refresh, so this copies the head of the module
 * (power_entry.S) to the 32 KiB power scratchpad at 0x1f000000, builds the
 * command packet behind it and jumps there; nothing returns.
 *
 * Internal names are unknown.
 */
#include "driver_tzs.h"
#include <sce/cp15.h>
#include <sce/ssx_firewall_regs.h>
#include <sce/spad_regs.h>

#define SYSCON_ERROR_INVALID_MODE	((int)0x80250000)

/* the modes (henkaku wiki, SceSyscon) */
#define POWER_MODE_POWER_OFF		0
#define POWER_MODE_SUSPEND		1
#define POWER_MODE_COLD_RESET		2
#define POWER_MODE_EXTERNAL_BOOT	3
#define POWER_MODE_UPDATE		4
#define POWER_MODE_HIBERNATE		5
#define POWER_MODE_RESUME_FIRST		16	/* 16-19 resume */
#define POWER_MODE_ARGUMENT_RESERVED	0x9ffe0421	/* bits the argument may not set */

/* the scratchpad: the copied entry, then the syscon packet */
#define SPAD32K_ENTRY_SIZE		0x200
#define SPAD32K_SECTION			0x1f010402	/* section, AP[0], shareable */
#define SPAD32K_PACKET			((uint8_t *)(SPAD32K_BASE + 0x400))

#define SPAD32K_FIREWALL	((SceSsxFirewallRegisters *)SSX_FIREWALL_SPAD32K_BASE)

/* name unknown: the secure world's level 1 translation table */
uint32_t *SceSysmemForTZS_038EAEEE(void);

/* power_entry.S, run from the scratchpad with the MMU off */
void PowerModeEntry(uint8_t *packet, unsigned int size);


int sceSysconSetPowerMode(unsigned int mode, unsigned int argument)
{
	uint8_t *tx = SPAD32K_PACKET;
	uint32_t *table;
	unsigned int sum;
	int length, i;

	if ((mode - POWER_MODE_RESUME_FIRST > 3 && mode > POWER_MODE_HIBERNATE)
	    || (argument & POWER_MODE_ARGUMENT_RESERVED) != 0)
		return SYSCON_ERROR_INVALID_MODE;
	sceKernelDcacheCleanInvalidateAll();
	SPAD32K_FIREWALL->region[0].reqInfoPermission = 1;
	PervasiveDsb();
	table = SceSysmemForTZS_038EAEEE();
	table[SPAD32K_BASE >> 20] = SPAD32K_SECTION;
	PervasiveDsb();
	writeDccimvac(&table[SPAD32K_BASE >> 20]);
	writeTlbiall(0);
	memcpy((void *)SPAD32K_BASE, (void *)((uintptr_t)PowerModeEntry & ~1),
		SPAD32K_ENTRY_SIZE);

	if (mode <= POWER_MODE_SUSPEND || mode - POWER_MODE_RESUME_FIRST <= 3) {
		tx[0] = 0xc0;
		tx[1] = 0;
		tx[2] = 5;
		tx[3] = mode;
		tx[4] = ~argument;
		tx[5] = ~argument >> 8;
		tx[6] = ~argument >> 16;
		tx[7] = ~argument >> 24;
		tx[8] = 0;
		tx[9] = 0;
		tx[10] = 0;
		if ((argument >> 24) != 0)
			tx[2] = 10;
	} else {
		switch (mode) {
		case POWER_MODE_COLD_RESET:
			tx[0] = 0x01;
			tx[1] = 0x08;
			tx[2] = 1;
			tx[3] = 0;
			break;
		case POWER_MODE_EXTERNAL_BOOT:
			tx[0] = 0xc1;
			tx[1] = 0;
			tx[2] = 2;
			tx[3] = 0;
			break;
		case POWER_MODE_UPDATE:
			tx[0] = 0xc1;
			tx[1] = 0;
			tx[2] = 2;
			tx[3] = 1;
			break;
		case POWER_MODE_HIBERNATE:
			tx[0] = 0xc2;
			tx[1] = 0;
			tx[2] = 2;
			tx[3] = 0x5a;
			break;
		}
	}

	/* the checksum: the complement of the command, length and data bytes */
	length = tx[2];
	sum = 0;
	for (i = 0; i <= length + 1; i++)
		sum += tx[i];
	tx[length + 2] = ~sum;
	PervasiveDsb();
	__asm__ volatile("isb sy" : : : "memory");
	((void (*)(uint8_t *, unsigned int))(SPAD32K_BASE | 1))(tx, 8);
	for (;;)
		;
}

int PowerModeModuleStart(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	sceKernelRegisterMonitorCallHandler(SYSCON_MONCALL_SET_POWER_MODE,
		(SceKernelMonitorCallHandler)sceSysconSetPowerMode);
	return 0;
}
