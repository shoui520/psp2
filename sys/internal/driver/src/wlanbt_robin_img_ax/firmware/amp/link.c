/*
 * The state machine of an AMP physical link: Create/Accept_Physical_Link,
 * the remote AMP_ASSOC, the PAL driver's reports and the disconnection. Each
 * link has one machine (ampLinkMachines), driven by the Mask ROM's
 * hierarchical state machine code. The filename and the names of states and
 * events are inferred; none survives in the image.
 */
#include <stddef.h>
#include "robin/amp.h"
#include "robin/btrom.h"
#include "robin/sys.h"

/* An event as the ROM hands it to a handler: its number and the record posted with it. */
struct AmpLinkMessage {
	int32_t code;
	const struct AmpLinkEvent *record;
};

/* The timer slot of the connection accept timeout and the link it times. */
static struct SysTimerSlotNode *ampLinkTimer __attribute__((section(".itcm.bss")));
static uint8_t ampLinkPending __attribute__((section(".itcm.bss")));

/* 0x0004a310: stops the connection accept timer. */
static __attribute__((noinline)) void ampLinkTimerStop(void)
{
	if (ampLinkTimer != NULL)
		sysTimerSlotStop(ampLinkTimer->index);
}

/* 0x00049d5c: the timer slot's callback. */
static int ampLinkTimeout(uint32_t index, uint32_t argument)
{
	if (ampLinkTimer != NULL)
		sysTimerSlotStop(ampLinkTimer->index);
	return ampLinkPostEvent(ampLinkPending, AMP_LINK_EVENT_TIMEOUT, NULL);
}

static inline void ampLinkEnter(struct AmpLinkMachine *machine, enum AmpLinkState state)
{
	machine->previousState = machine->state;
	machine->state = state;
}

/* A command the state does not allow. */
static void ampLinkDisallow(const struct AmpLinkEvent *record)
{
	ampHciAdjustCommandCount(-1);
	ampHciSendCommandStatus(record->opcode, AMP_HCI_COMMAND_DISALLOWED);
}

/* The link failed: the host hears Physical Link Complete with the status, and the link is freed. */
static void ampLinkFail(struct AmpLinkMachine *machine, int status)
{
	ampHciPhysicalLinkComplete(status, ampPhysicalLinks[machine->link].handle);
	ampPhysicalLinkReset(machine->link);
	ampHciAdjustCommandCount(-1);
	ampHciSendNopCommandStatus();
	romHsmTransition(machine, &machine->idle);
}

/* Disconnect_Physical_Link of a link that is already up: the PAL driver is told. */
static inline void ampLinkDisconnect(struct AmpLinkMachine *machine, const struct AmpLinkEvent *record)
{
	ampLinkTimerStop();
	ampHciAdjustCommandCount(-1);
	ampHciSendCommandStatus(record->opcode, AMP_HCI_SUCCESS);
	ampHciSendHostEvent(3, 0, "w", (uint32_t)&ampPhysicalLinks[record->link]);
	romHsmTransition(machine, &machine->disconnecting);
}

/* The link has no PAL driver event pending when the machine is idle. */
static const int32_t *ampLinkIdle(void *context, const int32_t *event)
{
	struct AmpLinkMachine *machine = context;
	const struct AmpLinkMessage *message = (const struct AmpLinkMessage *)event;
	const struct AmpLinkEvent *record = message->record;
	struct RomHsmState *target;
	int status = AMP_HCI_SUCCESS;

	switch (message->code) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		machine->state = AMP_LINK_IDLE;
		machine->previousState = AMP_LINK_IDLE;
		machine->link = AMP_PHYSICAL_LINKS;
		ampLinkTimerStop();
		return NULL;
	case AMP_LINK_EVENT_CREATE_PHYSICAL_LINK:
	case AMP_LINK_EVENT_ACCEPT_PHYSICAL_LINK:
		if (ampLinkTimer != NULL) {
			sysTimerSlotStart(ampLinkTimer->index, ampConnectionAcceptTimeout >> 4, 0);
			machine->link = record->link;
			ampLinkPending = record->link;
			target = &machine->waitAssoc;
		} else {
			status = AMP_HCI_LIMITED_RESOURCES;
			ampHciAdjustCommandCount(-1);
			ampPhysicalLinkReset(record->link);
			target = &machine->idle;
		}
		romHsmTransition(machine, target);
		ampHciSendCommandStatus(record->opcode, status);
		return NULL;
	case AMP_LINK_EVENT_WRITE_REMOTE_ASSOC:
	case AMP_LINK_EVENT_DISCONNECT_PHYSICAL_LINK:
	case AMP_LINK_EVENT_SHORT_RANGE_MODE:
		ampLinkDisallow(record);
		return NULL;
	case AMP_LINK_EVENT_ASSOC_FAILED:
		return NULL;
	}
	return event;
}

/* Waiting for the remote AMP_ASSOC. */
static const int32_t *ampLinkWaitAssoc(void *context, const int32_t *event)
{
	struct AmpLinkMachine *machine = context;
	const struct AmpLinkMessage *message = (const struct AmpLinkMessage *)event;
	const struct AmpLinkEvent *record = message->record;
	struct AmpPhysicalLink *link;

	switch (message->code) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		ampLinkEnter(machine, AMP_LINK_WAIT_ASSOC);
		return NULL;
	case AMP_LINK_EVENT_CREATE_PHYSICAL_LINK:
	case AMP_LINK_EVENT_ACCEPT_PHYSICAL_LINK:
	case AMP_LINK_EVENT_SHORT_RANGE_MODE:
		ampLinkDisallow(record);
		return NULL;
	case AMP_LINK_EVENT_WRITE_REMOTE_ASSOC:
		link = &ampPhysicalLinks[record->link];
		ampHciSendHostEvent(record->state == 0 ? 1 : 2, 0, "w", (uint32_t)link);
		ampHciAdjustCommandCount(-1);
		ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbb", record->opcode,
				AMP_HCI_SUCCESS, link->handle);
		return NULL;
	case AMP_LINK_EVENT_DISCONNECT_PHYSICAL_LINK:
		ampLinkTimerStop();
		ampHciAdjustCommandCount(-2);
		ampHciSendCommandStatus(record->opcode, AMP_HCI_SUCCESS);
		ampHciDisconnectPhysicalLinkComplete(AMP_HCI_SUCCESS,
						     ampPhysicalLinks[machine->link].handle);
		ampHciPhysicalLinkComplete(AMP_HCI_UNKNOWN_CONNECTION,
					   ampPhysicalLinks[machine->link].handle);
		ampPhysicalLinkReset(machine->link);
		romHsmTransition(machine, &machine->idle);
		return NULL;
	case AMP_LINK_EVENT_TIMEOUT:
		ampLinkFail(machine, AMP_HCI_CONNECTION_ACCEPT_TIMEOUT);
		return NULL;
	case AMP_LINK_EVENT_PAL_CHANNEL:
		if (record->state == 0)
			ampHciSendEvent(0x41, 2, "b", ampPhysicalLinks[machine->link].handle);
		romHsmTransition(machine, &machine->starting);
		return NULL;
	case AMP_LINK_EVENT_PAL_NO_RESOURCES:
		ampLinkTimerStop();
		ampLinkFail(machine, AMP_HCI_CONNECTION_LIMIT);
		return NULL;
	case AMP_LINK_EVENT_ASSOC_FAILED:
		ampHciPhysicalLinkComplete(record->word1 & 0xff, ampPhysicalLinks[machine->link].handle);
		ampPhysicalLinkReset(machine->link);
		romHsmTransition(machine, &machine->idle);
		return NULL;
	}
	return event;
}

/* The PAL driver is starting the link. */
static const int32_t *ampLinkStarting(void *context, const int32_t *event)
{
	struct AmpLinkMachine *machine = context;
	const struct AmpLinkMessage *message = (const struct AmpLinkMessage *)event;
	const struct AmpLinkEvent *record = message->record;

	switch (message->code) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		ampLinkEnter(machine, AMP_LINK_STARTING);
		return NULL;
	case AMP_LINK_EVENT_CREATE_PHYSICAL_LINK:
	case AMP_LINK_EVENT_ACCEPT_PHYSICAL_LINK:
	case AMP_LINK_EVENT_WRITE_REMOTE_ASSOC:
	case AMP_LINK_EVENT_SHORT_RANGE_MODE:
		ampLinkDisallow(record);
		return NULL;
	case AMP_LINK_EVENT_DISCONNECT_PHYSICAL_LINK:
		ampLinkDisconnect(machine, record);
		return NULL;
	case AMP_LINK_EVENT_TIMEOUT:
		ampHciSendHostEvent(3, 0, "w", (uint32_t)&ampPhysicalLinks[machine->link]);
		ampLinkFail(machine, AMP_HCI_CONNECTION_ACCEPT_TIMEOUT);
		return NULL;
	case AMP_LINK_EVENT_PAL_STARTED:
		romHsmTransition(machine, &machine->securing);
		return NULL;
	case AMP_LINK_EVENT_PAL_START_TIMEOUT:
		ampLinkTimerStop();
		ampLinkFail(machine, AMP_HCI_CONNECTION_TIMEOUT);
		return NULL;
	}
	return event;
}

/* The link is being secured (the PAL driver's four-way handshake). */
static const int32_t *ampLinkSecuring(void *context, const int32_t *event)
{
	struct AmpLinkMachine *machine = context;
	const struct AmpLinkMessage *message = (const struct AmpLinkMessage *)event;
	const struct AmpLinkEvent *record = message->record;

	switch (message->code) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		ampLinkEnter(machine, AMP_LINK_SECURING);
		return NULL;
	case AMP_LINK_EVENT_CREATE_PHYSICAL_LINK:
	case AMP_LINK_EVENT_ACCEPT_PHYSICAL_LINK:
	case AMP_LINK_EVENT_WRITE_REMOTE_ASSOC:
	case AMP_LINK_EVENT_SHORT_RANGE_MODE:
		ampLinkDisallow(record);
		return NULL;
	case AMP_LINK_EVENT_DISCONNECT_PHYSICAL_LINK:
		ampLinkDisconnect(machine, record);
		return NULL;
	case AMP_LINK_EVENT_TIMEOUT:
		ampHciSendHostEvent(3, 0, "w", (uint32_t)&ampPhysicalLinks[machine->link]);
		ampLinkFail(machine, AMP_HCI_CONNECTION_ACCEPT_TIMEOUT);
		return NULL;
	case AMP_LINK_EVENT_PAL_SECURITY_FAILED:
		ampLinkTimerStop();
		ampLinkFail(machine, AMP_HCI_AUTHENTICATION_FAILURE);
		return NULL;
	case AMP_LINK_EVENT_PAL_SECURED:
		ampLinkTimerStop();
		ampHciPhysicalLinkComplete(AMP_HCI_SUCCESS, ampPhysicalLinks[machine->link].handle);
		ampHciAdjustCommandCount(-1);
		ampHciSendNopCommandStatus();
		romHsmTransition(machine, &machine->connected);
		return NULL;
	}
	return event;
}

/* The link is up. */
static const int32_t *ampLinkConnected(void *context, const int32_t *event)
{
	struct AmpLinkMachine *machine = context;
	const struct AmpLinkMessage *message = (const struct AmpLinkMessage *)event;
	const struct AmpLinkEvent *record = message->record;
	unsigned int slot;

	switch (message->code) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		ampLinkEnter(machine, AMP_LINK_CONNECTED);
		return NULL;
	case AMP_LINK_EVENT_CREATE_PHYSICAL_LINK:
	case AMP_LINK_EVENT_ACCEPT_PHYSICAL_LINK:
	case AMP_LINK_EVENT_WRITE_REMOTE_ASSOC:
		ampHciAdjustCommandCount(-1);
		ampHciSendCommandStatus(record->opcode, AMP_HCI_COMMAND_DISALLOWED);
		return NULL;
	case AMP_LINK_EVENT_DISCONNECT_PHYSICAL_LINK:
		ampLinkTimerStop();
		ampHciAdjustCommandCount(-1);
		ampHciSendCommandStatus(record->opcode, AMP_HCI_SUCCESS);
		for (slot = 0; slot < AMP_LOGICAL_LINKS; slot = (uint8_t)(slot + 1))
			ampLogicalLinkRemove(ampPhysicalLinks[record->link].logicalLinks[slot].handle,
					     ampDisconnectReason);
		ampHciSendHostEvent(3, 0, "w", (uint32_t)&ampPhysicalLinks[record->link]);
		romHsmTransition(machine, &machine->disconnecting);
		return NULL;
	case AMP_LINK_EVENT_SHORT_RANGE_MODE:
		ampHciSendCommandStatus(record->opcode, AMP_HCI_SUCCESS);
		ampHciSendHostEvent(10, 0, "w", (uint32_t)&ampPhysicalLinks[record->link]);
		return NULL;
	case AMP_LINK_EVENT_PHYSICAL_LINK_GONE:
		romHsmTransition(machine, &machine->idle);
		return NULL;
	}
	return event;
}

/* Disconnect_Physical_Link was accepted; the PAL driver confirms. */
static const int32_t *ampLinkDisconnecting(void *context, const int32_t *event)
{
	struct AmpLinkMachine *machine = context;
	const struct AmpLinkMessage *message = (const struct AmpLinkMessage *)event;
	const struct AmpLinkEvent *record = message->record;

	switch (message->code) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		ampLinkEnter(machine, AMP_LINK_DISCONNECTING);
		return NULL;
	case AMP_LINK_EVENT_CREATE_PHYSICAL_LINK:
	case AMP_LINK_EVENT_ACCEPT_PHYSICAL_LINK:
	case AMP_LINK_EVENT_WRITE_REMOTE_ASSOC:
	case AMP_LINK_EVENT_DISCONNECT_PHYSICAL_LINK:
	case AMP_LINK_EVENT_SHORT_RANGE_MODE:
		ampLinkDisallow(record);
		return NULL;
	case AMP_LINK_EVENT_PAL_LOST:
		ampLinkTimerStop();
		ampHciDisconnectPhysicalLinkComplete(AMP_HCI_SUCCESS,
						     ampPhysicalLinks[machine->link].handle);
		if (machine->previousState != AMP_LINK_CONNECTED)
			ampHciPhysicalLinkComplete(AMP_HCI_UNKNOWN_CONNECTION,
						   ampPhysicalLinks[machine->link].handle);
		ampPhysicalLinkReset(machine->link);
		ampHciAdjustCommandCount(-1);
		ampHciSendNopCommandStatus();
		/* fall through */
	case AMP_LINK_EVENT_PHYSICAL_LINK_GONE:
		romHsmTransition(machine, &machine->idle);
		return NULL;
	}
	return event;
}

/* 0x0004a2aa: sets the machine's states up and takes the timeout's timer slot, once. */
void ampLinkMachineInit(struct AmpLinkMachine *machine)
{
	romHsmStateInit(&machine->idle, NULL, ampLinkIdle);
	romHsmStateInit(&machine->waitAssoc, NULL, ampLinkWaitAssoc);
	romHsmStateInit(&machine->starting, NULL, ampLinkStarting);
	romHsmStateInit(&machine->securing, NULL, ampLinkSecuring);
	romHsmStateInit(&machine->connected, NULL, ampLinkConnected);
	romHsmStateInit(&machine->disconnecting, NULL, ampLinkDisconnecting);
	if (ampLinkTimer != NULL)
		sysTimerSlotStop(ampLinkTimer->index);
	else
		ampLinkTimer = sysTimerSlotAcquire(ampLinkTimeout, 0);
}
