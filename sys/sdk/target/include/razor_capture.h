/*
 * PlayStation(R)Vita SDK header: razor_capture.h
 * Razor GPU capture control API.
 */

#ifndef _SCE_RAZOR_RAZOR_CAPTURE_H
#define _SCE_RAZOR_RAZOR_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sceRazorGpuCaptureSetTrigger(uint32_t frameIndex, const char *captureFilename);
void sceRazorGpuCaptureSetTriggerNextFrame(const char *captureFilename);
bool sceRazorGpuCaptureIsInProgress(void);
void sceRazorGpuCaptureEnableSalvage(const char *salvageFilename);

void sceRazorCaptureSetTrigger(uint32_t frameIndex, const char *captureFilename);
void sceRazorCaptureSetTriggerNextFrame(const char *captureFilename);
bool sceRazorCaptureIsInProgress(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_RAZOR_RAZOR_CAPTURE_H */
