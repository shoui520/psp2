#ifndef MISC_H
#define MISC_H

extern s32 gSysmemRandomValue;
extern u32 gSysmemRestrictionFlags;
extern u32 gSysmemModel;

void InitSystemIdString(void);

void InitGameInfo(void);
void CopyGameInfo(SceGameInfo *info);

#endif

