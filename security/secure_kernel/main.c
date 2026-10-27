/*
 * main.c: Secure Kernel main initialization, cold-boot handshake with ARM,
 * idle sleep loop, shutdown, and stack-protector failure handler
 * (0x8003d8..0x8004f3).
 *
 * File name ("main.c") and function name ("sceSblSkMain") are Sony's,
 * recovered from 0.995.070-TOOL debug strings.
 */

#include "secure_kernel.h"

/* 0x8079c0 (.data): stack canary seeded from hardware TRNG in _start */
uint32_t __stack_chk_guard = 0u;

/* 0x8003d8 (Sony name) */
int sceSblSkMain(void)
{
	enableBusErrorInterrupts();

	if (getSkState() == 1u) {
		int status;
		uint32_t armHandshakeVal;
		uint32_t commBufAddr;

		REG32(0xe0000010u) = 0xffffffffu;
		while (REG32(0xe0000000u) != 0u)
			;

		REG32(0xe0000040u) = 0xffffffffu;
		(void)REG32(0xe0000000u);
		__asm__ volatile ("nop\n\tsyncm" ::: "memory");

		status = setupKeyringAccessPermissions();
		skTriggerArmInterrupt(0x0101u);

		do {
			armHandshakeVal = REG32(0xe0000010u);
		} while ((armHandshakeVal & 1u) != 0u);

		setSkState(2u);
		commBufAddr = armHandshakeVal & ~3u;
		if (checkRangeInAnyDram(commBufAddr, 4u) != 0)
			setArmCommBufferAddr(commBufAddr);
		else
			status = (int)0x800f032f;

		REG32(0xe0000010u) = 0xffffffffu;
		if (status != 1) {
			skTriggerArmInterrupt(0x802fu);
			wipeAndLockAllKeyringSlots();
			return 0;
		}
	}

	if (checkHardwareBootStateAndSocVersion() != 1) {
		skTriggerArmInterrupt(0x802fu);
		wipeAndLockAllKeyringSlots();
		return 0;
	}

	skIntcInit();
	skEnableSwiException();
	setSkState(3u);
	skTriggerArmInterrupt(0x0102u);
	skSaveKernelContext();
	__asm__ volatile ("ei");

	while (getSkState() != 9u)
		__asm__ volatile ("sleep");

	__asm__ volatile ("di");
	setSkState(10u);
	skTriggerArmInterrupt(0x0106u);
	wipeAndLockAllKeyringSlots();
	return 0;
}

/* 0x8004c6 */
void __stack_chk_fail(uint32_t callerLp)
{
	setPanicArmInterruptCode(0x00100000u);
	skPanic("%08x %08x\n", 23u, callerLp ^ SK_PC_OBFUSCATION_MASK);
}
