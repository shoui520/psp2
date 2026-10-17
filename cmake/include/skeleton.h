/*
 * Placeholder bodies for T2 module skeletons.
 *
 * A skeleton links to the module's real export/import tables; each export
 * returns SKELETON_ERROR until the module is reimplemented.
 */

#pragma once

#define SKELETON_ERROR	0x80020001	/* SCE_KERNEL_ERROR_ERROR */

#define STUB(name) \
	int name(void) { return SKELETON_ERROR; }

#define STUB_MAIN(name) \
	int name(unsigned int args, void *argp) { return 0; }

#define STUB_VAR(name) \
	int name;
