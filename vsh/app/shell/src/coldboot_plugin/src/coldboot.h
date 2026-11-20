/*
 * coldboot.h: the cold boot plugin's parts, built into the shell: its interface, and its page
 * (coldboot.cpp). Every name here is ours (PLACEHOLDER: no record names them).
 */
#ifndef _SHELL_COLDBOOT_H
#define _SHELL_COLDBOOT_H

#include <plugin.h>
#include "../../../plugins.h"

/* (0x81555F28, 0x81555F2C) */
extern ColdbootPluginInterface *g_coldbootInterface;
extern paf::Plugin *g_coldbootPlugin;

void coldbootInterfaceRegister(paf::Plugin *plugin);	/* (0x810E4BF0) */
void coldbootInterfaceUnregister(void);			/* (0x814098DC, folded) */

void coldbootCreate(paf::Plugin *plugin);		/* (0x810E4C1A) */
void coldbootDestroy(void);				/* (0x810E4C4E) */
void coldbootFinish(void);				/* (0x810E4C6E) */
void coldbootOpen(void);				/* (0x810E4E42) */
void coldbootClose(void);				/* (0x810E5602) */

#endif /* _SHELL_COLDBOOT_H */
