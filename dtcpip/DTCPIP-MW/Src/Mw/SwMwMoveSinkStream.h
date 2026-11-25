/*
 * The move sink's stream (SwMwMoveSinkStream.cpp): the transfer object the system
 * engine's 0x2806 openStream creates behind a move sink. Its memory layout is the sink
 * stream's (SwMwSinkStream.h): the sink at +4, the transfer object at +0xC, the window
 * at +0x14, the transfer bookkeeping at +0x4D8 and the gate at +0x660.
 */

#pragma once

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwSinkStream.h"
#include "SwMwSync.h"

extern "C" {
int SwMwMoveSinkStreamGetWindow(struct SwMwSinkStream *stream, int *window);
int SwMwMoveSinkStreamMutexLock(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkStreamMutexUnlock(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkStreamSetFinished(struct SwMwSinkStream *stream, int status);
int SwMwMoveSinkStreamIsOpen(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamSetName(struct SwMwSinkStream *stream, const char *name);
int SwMwMoveSinkStreamNoop(void);
int SwMwMoveSinkStreamPoll(struct SwMwSinkStream *stream);
struct SwMwGate *SwMwMoveSinkStreamGate(struct SwMwSinkStream *stream);
}
