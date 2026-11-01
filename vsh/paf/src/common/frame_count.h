#ifndef PAF_COMMON_FRAME_COUNT_H
#define PAF_COMMON_FRAME_COUNT_H

#include <stdint.h>

#ifdef __cplusplus
namespace paf {
namespace common {
namespace internal {
void IncrFrameCount();
void ResetFrameCount();
}

uint32_t FrameCount();
}
}
#else
#define IncrFrameCount _ZN3paf6common8internal14IncrFrameCountEv
#define ResetFrameCount _ZN3paf6common8internal15ResetFrameCountEv
#define FrameCount _ZN3paf6common10FrameCountEv

void IncrFrameCount(void);
void ResetFrameCount(void);
uint32_t FrameCount(void);
#endif

#endif
