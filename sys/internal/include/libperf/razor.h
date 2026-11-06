/*
 * ScePerf's exports for the Razor CPU capture that other libraries trace into (libfiber and
 * libult): the capture flag and the fiber packets and user markers.
 */
#ifndef LIBPERF_RAZOR_H
#define LIBPERF_RAZOR_H

#include <scetypes.h>

/* nonzero (*_pLibPerfCaptureFlagPtr) while a capture runs */
extern SceUInt32 *_pLibPerfCaptureFlagPtr;

int _sceRazorCpuWriteFiberUltPkt(SceUInt32 kind, SceUInt32 event, SceUInt32 arg,
				 const void *object, const void *data);
int _sceCpuRazorPushFiberUserMarker(const char *label, SceUInt32 *markerStack,
				    const void *fiber, SceUInt32 color, SceUInt32 flags);
int _sceCpuRazorPopFiberUserMarker(SceUInt32 *markerStack, const void *fiber);

#endif
