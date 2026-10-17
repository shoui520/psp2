/* MagicGate key-manager dependencies recovered from its call sites. */
#pragma once

#include <psp2kern/types.h>
#include <sysmem/suspend.h>

int ksceSblDmac5AesEcbEncWithKeyslot(const void *input, void *output,
	SceSize size, unsigned slot, unsigned keyBits, unsigned flags);
int ksceSblDmac5AesEcbDecWithKeyslot(const void *input, void *output,
	SceSize size, unsigned slot, unsigned keyBits, unsigned flags);
int ksceSblDmac5DesEcbEncWithKeyslot(const void *input, void *output,
	SceSize size, unsigned slot, unsigned keyBits, unsigned flags);
int ksceSblDmac5DesEcbDecWithKeyslot(const void *input, void *output,
	SceSize size, unsigned slot, unsigned keyBits, unsigned flags);
