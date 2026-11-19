/*
 * Hooks of the Bluetooth host interface (include/robin/vendor.h): the
 * baseband reset, a link's power report, the configuration store's hooks
 * and vendor command 0xe0's subcommands. Marvell published no source; names
 * are inferred from use.
 */
#include <stdint.h>
#include "robin/btconfig.h"
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/btsource.h"
#include "robin/sbc.h"
#include "robin/soc.h"
#include "robin/vendor.h"

/* Resets the baseband through the ROM, keeping three radio registers. */
void btSbcProcessPacket(void)
{
	uint16_t first, second, third;

	first = BT_RADIO_KEPT_FIRST;
	second = BT_RADIO_KEPT_SECOND;
	third = BT_RADIO_KEPT_THIRD;
	SOC_CONTROL &= ~SOC_CONTROL_BT_RESET;
	SOC_CONTROL |= SOC_CONTROL_BT_RESET;
	btLdaResetHook();
	btLdaRomSelectState();
	BT_RADIO_KEPT_FIRST = first;
	BT_RADIO_KEPT_SECOND = second;
	BT_RADIO_KEPT_THIRD = third;
}

/*
 * A link's message: its handle at +4 and a kind at +6. Kind 6 also copies
 * the device's own rate step to the link.
 */
void btSbcUpdatePacket(const uint8_t *message)
{
	unsigned int handle, kind, link;
	const uint8_t *slot;
	uint8_t *records;

	handle = *(const BtHalf *)(message + 4);
	kind = message[6];
	slot = romHctHostSlot[handle];
	link = slot[2];
	records = btHccRomApplyState();
	btLscRomComputeState(message);
	btLdrRomHandleState(records + BT_LINK_RECORD * handle + 22, 10);
	btLmpLcDispatchState(link, kind);
	if (kind == 6)
		btDeviceRate[slot[2] + BT_DEVICE_RATE_COPY] = btDeviceRate[BT_DEVICE_RATE_OWN];
}

/* The configuration store's hooks (btSbcPreparePacket()). */
int btSbcConfigurePacket(unsigned int item, unsigned int length, uint8_t *value)
{
	return btSbcFilterState(item, value, length);
}

int btSbcCheckPacket(unsigned int item, unsigned int length, const uint8_t *value)
{
	return btSbcHandleContext(item, value, length);
}

int btSbcValidatePacket(unsigned int item)
{
	return btSbcProcessContext(item);
}

void btSbcPreparePacket(void)
{
	btConfigClearHook = btSbcClearState;
	btVendorKeyHook = btSbcConfigurePacket;
	btConfigWriteHook = btSbcCheckPacket;
	btConfigItemHook = btSbcValidatePacket;
}

/* Vendor command 0xe0: its subcommand (byte 2) picks the hook; others are not supported. */
int btSbcDispatchPacket(const uint8_t *command)
{
	int (*hook)(const uint8_t *command);

	switch (command[2]) {
	case 1:
		hook = btVendorSubcommandHooks[5];
		break;
	case 2:
		hook = btVendorSubcommandHooks[6];
		break;
	case 3:
		hook = btVendorSubcommandHooks[4];
		break;
	case 4:
		hook = btVendorSubcommandHooks[1];
		break;
	case 5:
		hook = btVendorSubcommandHooks[2];
		break;
	case 6:
		hook = btVendorSubcommandHooks[0];
		break;
	case 7:
		hook = btVendorSubcommandHooks[3];
		break;
	default:
		return BT_STATUS_UNSUPPORTED;
	}
	return hook(command);
}

/* The vendor commands nothing else takes: 0xe0, and a Command Complete with its first five bytes. */
int btSbcParsePacket(uintptr_t first, uintptr_t second, uintptr_t third, uintptr_t fourth, uintptr_t fifth,
		     uintptr_t sixth)
{
	const uint8_t *command = (const uint8_t *)third;
	int status;

	(void)second;
	(void)fourth;
	(void)fifth;
	(void)sixth;
	if (command[0] == BT_VENDOR_SUBCOMMAND_GROUP)
		status = btSbcDispatchPacket(command);
	else
		status = BT_STATUS_UNSUPPORTED;
	btBdhRomPrepareContext(-1, 0);
	return btBdhRomConfigureContext(BT_EVENT_COMMAND_COMPLETE, 0x120, "Rhbbbbbb", first, status, command[0],
					command[1], command[2], command[3], command[4]);
}
