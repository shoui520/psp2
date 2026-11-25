/*
 * The move sink's stream (see SwMwMoveSinkStream.h): the window copy, the bookkeeping
 * lock and the "finished" flag its commands share, the open state, the name, the poll
 * and the gate its commands serialize on.
 */

#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwMoveSinkStream.h"

#define SWMW_MOVESTREAM_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwMoveSinkStream.cpp"

/* the stream's window, as the transfer commands report it (0x8100a190) */
extern "C" int SwMwMoveSinkStreamGetWindow(struct SwMwSinkStream *stream, int *window)
{
	window[0] = stream->window.value[0];
	window[1] = stream->window.value[1];
	window[2] = stream->window.value[2];
	window[3] = stream->window.value[3];
	window[4] = stream->window.value[4];
	window[5] = stream->window.value[5];
	return stream->window.value[5];
}

/* lock and unlock the transfer bookkeeping (0x8100a348/0x8100a36a) */
extern "C" int SwMwMoveSinkStreamMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwMoveSinkStreamMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* mark the stream finished with a status, once (0x8100a388) */
extern "C" int SwMwMoveSinkStreamSetFinished(struct SwMwSinkStream *stream, int status)
{
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	if (stream->finished == 0) {
		stream->finished = 1;
		stream->status = status;
	}
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return 0;
}

/* the stream's open state: it finishes the transfer (0x8100a3c6) */
extern "C" int SwMwMoveSinkStreamIsOpen(struct SwMwSinkStream *stream)
{
	SwMwMoveSinkStreamSetFinished(stream, 12015);
	return 0;
}

/* the stream's name (0x8100a3e2) */
extern "C" int SwMwMoveSinkStreamSetName(struct SwMwSinkStream *stream, const char *name)
{
	strncpy(stream->name, name, 0xFF);
	return 0;
}

/* a hook that has nothing to do (0x8100a406) */
extern "C" int SwMwMoveSinkStreamNoop(void)
{
	return 0;
}

/* the transfer's state while a command is outstanding (0x8100acb6) */
extern "C" int SwMwMoveSinkStreamPoll(struct SwMwSinkStream *stream)
{
	int status;

	status = 0;
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	if (stream->finished == 0 && stream->empty == 0)
		status = 12007;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* the stream's gate (0x8100b7ea) */
extern "C" struct SwMwGate *SwMwMoveSinkStreamGate(struct SwMwSinkStream *stream)
{
	return &stream->gate;
}
