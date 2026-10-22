/* SceAVConfig's driver interface. */
#pragma once

#include <scetypes.h>

/*
 * Display requests: a sound-exclusive application asks for the display at a
 * priority (index); its callers pass a fifth argument, which is not read.
 */
int SceAVConfigForDriver_2A2E9042(unsigned int index, unsigned int enabled,
		unsigned int mode, int blanked, int reserved);
int SceAVConfigForDriver_3817CDA1(unsigned int index, unsigned int enabled,
		unsigned int mode);
int SceAVConfigForDriver_91792D6F(unsigned int index);
