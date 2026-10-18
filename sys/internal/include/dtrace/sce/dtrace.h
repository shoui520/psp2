/*
 * A static DTrace probe a module carries (SceDisplay's __interrupt_vblank,
 * SceHdmi's __hdmi__hdmi_cec_tx/rx). DTrace finds the module's probes, sets
 * the hook and calls the provider callbacks. No Sony name is known for the
 * type or its fields.
 */
#pragma once

#include <scetypes.h>

typedef struct SceDtraceProbe {
	int *enabled;			/* 0x00 */
	const char *provider;		/* 0x04 */
	const char *name;		/* 0x08 */
	const void *address;		/* 0x0c: where the probe fires */
	void *hook;			/* 0x10: the handler pointer DTrace sets */
	SceUInt32 reserved;		/* 0x14 */
	void (*create)(void);		/* 0x18 */
	void (*enable)(void);		/* 0x1c */
	void (*disable)(void);		/* 0x20 */
	void (*destroy)(void);		/* 0x24 */
} SceDtraceProbe;
