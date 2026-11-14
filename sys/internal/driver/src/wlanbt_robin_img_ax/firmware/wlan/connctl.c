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
#include "robin/wlantask.h"

/*
 * Roles of the connections the MAC serves: the "client" connection is the
 * station role (a station, or a group client), the other one is the AP, group
 * owner, AMP or IBSS role.
 */
struct WlanConnection *wlanConnectionClient __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionOther __attribute__((section(".sqram1.rw")));
uint8_t wlanRoleMask __attribute__((section(".sqram1.rw")));
struct OsaBufferPool wlanEventPool __attribute__((section(".dtcm.bss")));

int wlanRoleMaskIs(unsigned int mask)
{
	return mask == wlanRoleMask;
}

struct WlanConnection *wlanConnectionByRole(int role)
{
	if (role == 2)
		return wlanConnectionClient;
	return wlanConnectionOther;
}

int wlanConnectionHasPrimary(void)
{
	return wlanConnectionPrimary != NULL;
}

int wlanConnectionSetPrimary(struct WlanConnection *connection)
{
	wlanConnectionPrimary = connection;
	return 0;
}

struct WlanConnection *wlanConnectionGetPrimary(void)
{
	return wlanConnectionPrimary;
}

/* Records a new connection in its role slot; returns 1 when the AP role blocks it. */
int wlanConnectionActivate(struct WlanConnection *connection)
{
	unsigned int type;

	if (wlanHasApRole() && wlanConnectionClient->state == 7)
		return 1;
	type = connection->interfaceType;
	if (type == WLAN_IF_AMP) {
		if (wlanConnectionOther == NULL)
			wlanConnectionOther = connection;
		wlanRoleMask = 1;
		return 0;
	}
	if (type != WLAN_IF_STATION) {
		if (type != WLAN_IF_AP) {
			wlanConnectionOther = connection;
			wlanRoleMask = type == WLAN_IF_IBSS ? 2 : 0;
			if (type == 0)
				wlanRoleMask = 1;
			return 0;
		}
		if (connection->mode != 2) {
			if (connection->mode == 0)
				return 0;
			wlanConnectionOther = connection;
			wlanRoleMask = 1;
			return 0;
		}
	}
	wlanConnectionClient = connection;
	wlanRoleMask = 3;
	return 0;
}

/* Posts (connection, event) to the control task queue; returns 1 if it cannot. */
int wlanPostConnectionEvent(struct WlanConnection *connection, uint32_t event)
{
	OsaBuffer *buffer = osa_BufferAlloc(&wlanEventPool, 0, 0);
	uint8_t *message;
	int result;

	if (buffer == NULL)
		return 1;
	message = (uint8_t *)buffer + buffer->dataOffset;
	__aeabi_memclr(message, 8);
	__aeabi_uwrite4((uintptr_t)connection, message);
	__aeabi_uwrite4(event, message + 4);
	result = osaMsgQueueSend(cbProcQueue, 4, (uintptr_t)buffer, 0);
	if (result != 0) {
		osa_BufferFree(buffer);
		return 1;
	}
	return result;
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
