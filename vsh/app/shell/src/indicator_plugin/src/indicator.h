/*
 * indicator.h: the indicator plugin's parts as its entry points (indicator_plugin.cpp) reach
 * them: the interface, the plugin's cache manager and the indicator page. The file's and the
 * names are ours (PLACEHOLDER).
 */
#ifndef INDICATOR_H
#define INDICATOR_H

#include <scetypes.h>
#include <plugin.h>

/* the plugin's interface given to the shell (0x8115F9A8), and its resources taken (0x8115F98C) */
void indicatorInterfaceRegister(paf::Plugin *plugin);
void indicatorInterfaceInit(void);
void indicatorInterfaceExit(void);

/* the plugin's cache manager, "indicator_plugin_CacheManager" (0x8115F8DA, 0x8115F8F0) */
void indicatorCacheCreate(paf::Plugin *plugin);
void indicatorCacheDestroy(void);

/* the indicator page opened and its object made (0x8115E37A), and both destroyed (0x8115E6B2) */
void indicatorCreate(void);
void indicatorDestroy(void);
void indicatorExitReserved(void);

#endif
