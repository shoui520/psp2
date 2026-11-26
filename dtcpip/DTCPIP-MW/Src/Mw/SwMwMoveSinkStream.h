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
#include "SwMwMoveSink.h"

extern "C" {
struct SwMwSinkStream *SwMwMoveSinkStreamConstruct(struct SwMwSinkStream *stream);
struct SwMwSinkStream *SwMwMoveSinkStreamDestruct(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamInit(struct SwMwSinkStream *stream, void *sink,
			   const struct SwMwStreamWindow *window);
int SwMwMoveSinkStreamCreate(struct SwMwSinkStream **stream, void *sink,
			     const struct SwMwStreamWindow *window);
int SwMwMoveSinkStreamDestroy(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamCompleteLabel(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamFinalize(struct SwMwSinkStream *stream, int value);
int SwMwMoveSinkStreamComplete(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamWaitClosed(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamEventSet(struct SwMwSinkStream *stream, void *handler, void *argument);
int SwMwMoveSinkStreamOnEvent(struct SwMwSinkStream *stream, int *message);
int SwMwMoveSinkStreamKxExpiring(void);
int SwMwMoveSinkStreamConnect(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamAbort(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamDestroy(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamCompleteLabel(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamFinalize(struct SwMwSinkStream *stream, int value);
int SwMwMoveSinkStreamComplete(struct SwMwSinkStream *stream);
int SwMwMoveSinkStreamWaitClosed(struct SwMwSinkStream *stream);
struct SwMwSinkStream *SwMwMoveSinkStreamConstruct(struct SwMwSinkStream *stream);
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
