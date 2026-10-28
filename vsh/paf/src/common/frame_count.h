#ifndef PAF_COMMON_FRAME_COUNT_H
#define PAF_COMMON_FRAME_COUNT_H

#include <stdint.h>

#define IncrFrameCount ScePafCommon_2700B0E2
#define ResetFrameCount ScePafCommon_23B36031
#define FrameCount ScePafCommon_D0D82903

void IncrFrameCount(void);
void ResetFrameCount(void);
uint32_t FrameCount(void);

#endif
