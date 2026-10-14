/*
 * Mask ROM patch unit (0x00026328 .. 0x0002638a).
 *
 * The SoC has four comparators that replace a 32-bit ROM word on fetch. A
 * slot holds the ROM address (bits 19:0, enabled with bits 31:30) and the
 * replacement word. Patched words that are SWI opcodes trap to vectorSwi
 * (boot/reset.S), which calls romPatchSlots[hit].target and adds
 * returnAdjust to the return address; hit is read from ROM_PATCH_HIT.
 */
#include <stdint.h>
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/sys.h"

#define ROM_PATCH_COMPARATORS	4
#define ROM_PATCH_SLOTS		24
#define ROM_PATCH_ADDR_MASK	0x000fffffu
#define ROM_PATCH_ADDR_ENABLE	0xc0000000u

/* Opcodes substituted when a patch traps: Thumb "svc 0", ARM "swi 0xdead00". */
#define ROM_PATCH_THUMB_SVC	0xdf00u
#define ROM_PATCH_ARM_SWI	0xefdead00u

struct RomPatchSlot {
	void (*target)(void);
	uint32_t returnAdjust;
};

struct RomPatchSlot romPatchSlots[ROM_PATCH_SLOTS];

/* Init table entry (0x00026328). */
int romPatchInit(void)
{
	ROM_PATCH_ENABLE = 0;
	ROM_PATCH_THUMB_INSN = ROM_PATCH_THUMB_SVC;
	ROM_PATCH_ARM_INSN = ROM_PATCH_ARM_SWI;
	__aeabi_memclr4(romPatchSlots, sizeof(romPatchSlots));
	ROM_PATCH_ENABLE = 1;
	return 0;
}

/*
 * Replaces the ROM word at romAddress with replacement in the first free
 * comparator (0x0002634e). Halts when all four are taken.
 */
void romPatchInstall(uint32_t romAddress, uint32_t replacement)
{
	int i;

	for (i = 0; i < ROM_PATCH_COMPARATORS; i++) {
		if (romPatchSlots[i].target != 0)
			continue;
		romPatchSlots[i].target = (void (*)(void))romAddress;
		ROM_PATCH_DATA(i) = replacement;
		ROM_PATCH_ADDR(i) = (romAddress & ROM_PATCH_ADDR_MASK) + ROM_PATCH_ADDR_ENABLE;
		return;
	}
	for (;;)
		;
}
