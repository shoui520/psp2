/*
 * Connection control: role bookkeeping and the message to the "CB Proc" task.
 * The filename is inferred; no source filename survives.
 */
#include "robin/conn.h"
#include "robin/psctl.h"
#include "robin/buffer.h"
#include "robin/btrom.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

void wlanTrace(uint32_t tag, uint32_t value)
{
}

/* Posts (connection, event) to the control task queue; returns 1 if it cannot. */
int wlanPostConnectionEvent(struct WlanConnection *connection, uint32_t event)
{
	OsaBuffer *buffer = osa_BufferAlloc(&wlanEventPool, 0, 0);
	uint8_t *message;

	if (buffer != NULL) {
		message = (uint8_t *)buffer + buffer->dataOffset;
		__aeabi_memclr(message, 8);
		__aeabi_uwrite4((uintptr_t)connection, message);
		__aeabi_uwrite4(event, message + 4);
		if (osaMsgQueueSend(cbProcQueue, 4, (uintptr_t)buffer, 0) == 0)
			return 0;
	}
	/* also on a failed allocation: Sony frees the NULL buffer */
	osa_BufferFree(buffer);
	return 1;
}

/* True when the state semaphore of the control task could be taken at once. */
int wlanConnectEventTryWait(void)
{
	return osaSemaphoreWait(cbProcSemaphore, 0) == 0;
}

int wlanReleaseStateSemaphore(void)
{
	return osaSemaphoreRelease(cbProcSemaphore);
}

int wlanPsDisabled(void)
{
	return !wlanPsEnabled();
}
