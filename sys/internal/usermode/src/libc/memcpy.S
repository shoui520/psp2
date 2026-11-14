/*
 * memcpy and memmove: the kernel's hand-scheduled copies (sysmem's
 * copy_forward.S and move.S), which the C library links as one object, so
 * that memmove's forward case is a short branch to memcpy.
 */
#define CopyMemoryForward	memcpy
#define MoveMemoryCore		memmove
#define SYSCLIB_NO_STANDARD_ALIASES
#define MEMMOVE_AFTER_MEMCPY
#include "../../../kernel/src/sysmem/copy_forward.S"
#include "../../../kernel/src/sysmem/move.S"
	.balign	16
