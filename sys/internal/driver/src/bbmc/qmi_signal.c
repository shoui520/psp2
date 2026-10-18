/* Protocol-driver interface waits and event signaling. */
#include <threadmgr/threadmgr.h>
#include "qmi_backend.h"

/* name unknown: wait on one interface bit with an optional timeout value. */
int qmiWaitInterface(unsigned int kind, unsigned int channel, SceUInt32 timeout)
{
	SceUInt32 bits;
	SceUInt32 *waitTimeout;

	if (channel > 1)
		return (int)0x80330002;
	bits = 0;
	waitTimeout = timeout != 0 ? &timeout : NULL;
	if (kind != 0)
		return (int)0x80330002;
	return sceKernelWaitEventFlag(g_qmiBackendControl->interfaceEvent,
		1U << channel, 5, &bits, waitTimeout);
}

/* name unknown: signal either an interface bit or a permitted control bit. */
int qmiSignalBackend(unsigned int kind, unsigned int channel,
	unsigned int eventOffset)
{
	if (channel <= 1) {
		if (kind == 1) {
			if ((eventOffset & ~4U) == 0 || eventOffset == 16 ||
			    eventOffset == 8) {
				sceKernelSetEventFlag(g_qmiBackendControl->controlEvent,
					1U << (channel + eventOffset));
				return 0;
			}
		} else if (kind == 0) {
			sceKernelSetEventFlag(g_qmiBackendControl->interfaceEvent,
				1U << channel);
			return 0;
		}
	}
	return (int)0x80330002;
}
