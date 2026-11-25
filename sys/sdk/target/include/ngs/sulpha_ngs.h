/*
 * ngs/sulpha_ngs.h: Sulpha NGS profiling API, under Sony's names and definitions
 * (SDK Release 03.570.011, target/include/ngs/sulpha_ngs.h).
 */
#ifndef _SCE_NGS_SULPHA_NGS_H
#define _SCE_NGS_SULPHA_NGS_H

#include <ngs/ngs_top.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SceSulphaNgsConfig {
	SceUInt32 maxNamedObjects;
	SceUInt32 maxTraceBufferBytes;
} SceSulphaNgsConfig;

int sceSulphaNgsGetDefaultConfig(SceSulphaNgsConfig *config);
int sceSulphaNgsGetNeededMemory(const SceSulphaNgsConfig *config, SceUInt32 *sizeInBytes);
int sceSulphaNgsInit(const SceSulphaNgsConfig *config, void *buffer, SceUInt32 sizeInBytes);
int sceSulphaNgsShutdown(void);

int sceSulphaNgsSetSynthName(SceNgsHSynSystem synthHandle, const char *name);
int sceSulphaNgsSetRackName(SceNgsHRack rackHandle, const char *name);
int sceSulphaNgsSetVoiceName(SceNgsHVoice voiceHandle, const char *name);
int sceSulphaNgsSetSampleName(const void *location, SceUInt32 length, const char *name);

int sceSulphaNgsTrace(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NGS_SULPHA_NGS_H */
