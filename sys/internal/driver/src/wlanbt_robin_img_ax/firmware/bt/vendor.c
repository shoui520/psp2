/*
 * The Bluetooth vendor commands and the radio set-up around them
 * (include/robin/vendor.h).
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/btbuffer.h"
#include "robin/btconfig.h"
#include "robin/btpower.h"
#include "robin/buffer.h"
#include "robin/fm.h"
#include "robin/mdev.h"
#include "robin/btrom.h"
#include "robin/btsource.h"
#include "robin/bttable.h"
#include "robin/bttest.h"
#include "robin/eventpower.h"
#include "robin/btcontext.h"
#include "robin/hct.h"
#include "robin/rom.h"
#include "robin/sbc.h"
#include "robin/soc.h"
#include "robin/vendor.h"

/* A halfword of the link controller's register block. */
#define LINK_U16(offset)	(*(volatile uint16_t *)(btLinkContext + (offset)))

const struct BtRateDefault btRateDefaults[BT_RATE_DEFAULTS] = {
	{ 0x20, 0, 0x0002 }, { 0x00, 0, 0x3281 }, { 0x02, 0, 0x8230 }, { 0x04, 0, 0x0039 },
	{ 0x12, 0, 0x1014 }, { 0x14, 0, 0x8064 }, { 0x2a, 0, 0x0001 }, { 0x36, 0, 0x058e },
};

const uint16_t btRadioDefaults[BT_RADIO_DEFAULTS] = {
	0x8504, 0x0000, 0x0000, 0x0010, 0x0000, 0x0011, 0x0015, 0x3038,
	0x0128, 0x1c18, 0x0002, 0x0140, 0xe031, 0x0000, 0x22c6, 0x0000,
	0x0060, 0x000e, 0x0004, 0x000a, 0x0600, 0x0186, 0x0086, 0x0000,
	0x0000, 0x0000, 0xf01f, 0x0277, 0x007c, 0x0004, 0x0009, 0x4604,
	0x0000, 0x8014, 0x0000, 0x0000, 0x0000, 0x0000, 0x88b0, 0x8ac1,
	0x831e, 0x82c6, 0x8006, 0x8006, 0x0602, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x3190, 0x1000, 0x937e, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000,
};

/* Drops a register from a list of radio words. */
__attribute__((noinline)) void btSbcComputeContext(unsigned int reg, struct BtPowerWord *words, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (words[i].reg == reg) {
			words[i].reg = BT_POWER_SKIP;
			return;
		}
	}
}

/* Gives a register of a list of radio words a new value: 1 when it is in the list. */
__attribute__((noinline)) int btSbcQueryContext(unsigned int reg, uint8_t value, struct BtPowerWord *words, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (words[i].reg == reg) {
			words[i].value = value;
			return 1;
		}
	}
	return 0;
}

/* Applies a list of changes to a list of radio words, marking each change done. */
void btSbcResetContext(const struct BtPowerUpdate *updates, struct BtPowerWord *words, unsigned int count)
{
	struct BtPowerUpdate *update = (struct BtPowerUpdate *)updates;
	unsigned int i;

	for (i = 0; update[i].reg != BT_POWER_END; i++) {
		if (update[i].value == BT_POWER_SKIP)
			btSbcComputeContext(update[i].reg, words, count);
		else if (btSbcQueryContext(update[i].reg, update[i].value, words, count) == 0)
			continue;
		update[i].reg = BT_POWER_SKIP;
	}
}

/* The link controller's and the radio's register defaults. */
void btSbcSyncContext(void)
{
	uint8_t *rate;
	unsigned int i;

	LINK_U16(388) |= 1;
	LINK_U16(388) |= 0x18;
	LINK_U16(388) &= ~4;
	LINK_U16(388) |= 0x100;
	LINK_U16(62) = 8;
	LINK_U16(352) &= ~0xf0;
	LINK_U16(538) = 0x83ff;
	for (i = 0; i < BT_RADIO_DEFAULTS; i++)
		BT_RADIO_WINDOW[i] = btRadioDefaults[i];
	rate = (uint8_t *)btLinkRate;
	BT_RADIO_CONTROL = 0x707;
	for (i = 0; i < BT_RATE_DEFAULTS; i++)
		*(volatile uint16_t *)(rate + btRateDefaults[i].offset) = btRateDefaults[i].value;
}

/* Puts the fixed level's tables in place of the defaults, when the level is fixed. */
void btSbcApplyContext(void)
{
	unsigned int i;

	if (btPowerLevelContext != 7)
		return;
	for (i = 0; i < 7; i++) {
		btPowerMaskContext[14][i] = btPowerFixedRows[0][i];
		btPowerMaskContext[16][i] = btPowerFixedRows[1][i];
	}
	for (i = 0; i < BT_POWER_MASKS; i++)
		btPowerMaskContext[i][4] = btPowerFixedColumn[i];
	btPowerIndexCommand[4] = btPowerFixedCommand;
	__aeabi_memcpy(btPowerIndexRegister, btPowerFixedRegister, sizeof(btPowerIndexRegister));
	__aeabi_memcpy(btPowerIndexRecord, btPowerFixedRecord, sizeof(btPowerIndexRecord));
	btSbcResetContext(btPowerFixedWords, btPowerLevelPacket, BT_POWER_WORDS);
}

/*
 * Vendor command 105: writes a register of the transport (4 bytes of
 * parameters) or the wider form (6 bytes), and answers with the parameters.
 */
void btSbcResolveContext(unsigned int length, const uint8_t *parameters, uintptr_t unused2,
			 uintptr_t unused3, uint8_t *status)
{
	struct BtSdioRegisterRequest request;

	(void)unused2;
	(void)unused3;
	if (length == 4) {
		request.form = 0;
		request.first = parameters[0] | parameters[1] << 8;
		request.second = parameters[2] | parameters[3] << 8;
		if (mdevIoctl(btSdioActiveHandle, BT_SDIO_IOCTL_REGISTER, (uintptr_t)&request) != 0)
			*status = BT_STATUS_INVALID;
		btBdhRomConfigureContext(0xff, 0, "Vbbbbbb", 105, parameters[0], parameters[1], parameters[2],
					 parameters[3], *status);
	} else if (length == 6) {
		request.form = 1;
		request.first = parameters[0] | parameters[1] << 8;
		request.second = parameters[2] | parameters[3] << 8;
		request.third = parameters[4];
		request.fourth = parameters[5];
		if (mdevIoctl(btSdioActiveHandle, BT_SDIO_IOCTL_REGISTER, (uintptr_t)&request) != 0)
			*status = BT_STATUS_INVALID;
		btBdhRomConfigureContext(0xff, 0, "Vbbbbbbbb", 105, parameters[0], parameters[1], parameters[2],
					 parameters[3], parameters[4], parameters[5], *status);
	} else {
		*status = BT_STATUS_INVALID;
		btBdhRomConfigureContext(0xff, 0, "Vbb", 105, BT_STATUS_INVALID);
	}
}

void btSbcScheduleContext(void)
{
	uint32_t *message;

	message = osa_MsgAlloc(4);
	if (message != NULL) {
		message[0] = BT_MESSAGE_VENDOR_LINK;
		osa_MsgSend((void **)&message, BT_QUEUE_VENDOR_LINK);
	}
}

/* Completes vendor command 97. */
void btSbcNotifyContext(void)
{
	romVendorPending = 0;
	btBdhRomCheckContext(4, 0, BT_VENDOR_GROUP, 97);
}

/* Vendor command 36: a link's power step (a link below 8, a step below 4) for the link controller. */
void btSbcFormatContext(unsigned int length, const uint8_t *parameters, uint8_t *status)
{
	uint8_t *message;
	unsigned int link;
	unsigned int step;

	*status = BT_STATUS_INVALID;
	if (length != 3)
		return;
	link = parameters[0] | parameters[1] << 8;
	step = parameters[2];
	if (link < 8 && step < 4) {
		*status = 0;
		message = osa_MsgAlloc(8);
		*(uint32_t *)message = BT_MESSAGE_LINK_POWER;
		*(uint16_t *)(message + 4) = link;
		message[6] = step;
		osa_MsgSend((void **)&message, BT_QUEUE_VENDOR_POWER);
	}
}

/*
 * The vendor commands (group 63): the reply's parameters go to
 * returnParameters and their length to *returnLength; *status is the reply's
 * status, left as the caller set it where the command does not change it.
 */
void btSbcInspectContext(unsigned int opcode, unsigned int length, const uint8_t *parameters, uintptr_t argument,
			 uint8_t *returnLength, uint8_t *status, uint8_t *returnParameters)
{
	uint32_t config[3];
	uint32_t version;
	unsigned int rate;

	if (opcode >> 10 != BT_VENDOR_GROUP)
		goto unknown;
	switch (opcode & 0x3ff) {
	case 6:
		/* The transport's header settings: a rate that 60 divides. */
		rate = parameters[0];
		if (rate == 0 || rate % 60 != 0)
			goto invalid;
		romHctHeaderSize = rate;
		romLcHeaderSize = rate;
		romHctDescriptor = (uint8_t)(722u / parameters[0]);
		romLcDescriptor = romHctDescriptor;
		return;
	case 8:
		if (length != 10)
			goto invalid;
		/* The original leaves the rest of the format words as the stack held them. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
		config[0] = (config[0] & ~0xc0000u) | 0x10000000;
#pragma GCC diagnostic pop
		config[2] = 0;
		btSbcPcmStreamHandler(config);
		return;
	case 15:
		version = hostCmd0003Version();
		returnParameters[0] = version;
		returnParameters[1] = version >> 8;
		returnParameters[2] = version >> 16;
		returnParameters[3] = version >> 24;
		version = mdevRomValidateState();
		returnParameters[4] = version;
		returnParameters[5] = version >> 8;
		returnParameters[6] = version >> 16;
		returnParameters[7] = version >> 24;
		*returnLength = 8;
		return;
	case 24:
		*status = p2pSqramHandleTable(length, parameters);
		return;
	case 30:
		btLmpLcRecordFrame(length, parameters, returnLength, status, returnParameters);
		return;
	case 32:
		btSbcConfigureState(parameters);
		return;
	case 35:
		if (btLmpLcQueryContext(length, parameters) == 0)
			*status = BT_STATUS_INVALID;
		btBdhRomConfigureContext(0xff, 0, "Vbbb", 35, parameters[0], *status);
		return;
	case 36:
		btSbcFormatContext(length, parameters, status);
		return;
	case 42:
		*status = btAudioPathModesSet(length, parameters);
		return;
	case 61:
		btSbcFilterState(BT_CONFIG_ITEM_KEY, returnParameters);
		*returnLength = 16;
		return;
	case 62:
		btSbcHandleContext(BT_CONFIG_ITEM_KEY, parameters + 3, 16);
		return;
	case 75:
		*status = btLdaRomQueryState(length, parameters);
		return;
	case 85:
		*returnLength = mdevRomScheduleState(returnParameters, btVendorTaskState, btTaskTable);
		return;
	case 87:
		btLmpLcValidateState((uint8_t *)parameters);
		return;
	case 89:
		if (btLmpLcSelectContext(0, length, parameters) == 0)
			*status = BT_STATUS_INVALID;
		btBdhRomConfigureContext(0xff, 0, "Vbbbb", 89, parameters[0], parameters[1], *status);
		return;
	case 90:
		if (hctTransportReady(0) != 0)
			btLmpLcFormatContext(0, 0, 3, 0);
		return;
	case 91:
		btSbcValidateEntry(length, parameters, argument, returnLength, status, returnParameters);
		return;
	case 94:
		if (parameters[0] != 0)
			BT_SOC_CONTROL &= ~2u;
		else
			BT_SOC_CONTROL |= 2;
		return;
	case 98:
		if (btSbcConfigureContext(parameters, returnLength, returnParameters) != 0)
			goto invalid;
		return;
	case 104:
		btLinkStateMask = parameters[0];
		return;
	case 105:
		btSbcResolveContext(length, parameters, argument, (uintptr_t)returnLength, status);
		return;
	case 106:
		/* The low byte only. */
		*(uint8_t *)&hctDeviceContext = parameters[0];
		return;
	case 107:
		btSbcCheckEntry(length, parameters, status, returnLength, returnParameters);
		return;
	default:
		goto unknown;
	}

invalid:
	*status = BT_STATUS_INVALID;
	return;
unknown:
	*returnLength = 0;
	*status = BT_STATUS_UNKNOWN;
}

struct BtRoleSwitches btRoleSwitches __attribute__((section(".itcm.rw")));

/* An FM command the host sends as a vendor command: passed to the "fm" device whole. */
void btSbcTrackContext(unsigned int opcode, unsigned int length, const uint8_t *command)
{
	OsaBuffer *buffer;
	unsigned int status;

	buffer = osa_BufferAlloc(&fmPool, length, 0);
	if (buffer != NULL) {
		__aeabi_memcpy((uint8_t *)buffer + buffer->dataOffset, command, length);
		mdevForwardEvent(fmDevice, buffer, length, 1);
		btBdhRomPrepareContext(1, 0);
		status = 0;
	} else {
		status = 13;
	}
	btBdhRomValidateContext(status, opcode);
}

int btSbcManageContext(uintptr_t first, uintptr_t second, uintptr_t third, uintptr_t fourth, uintptr_t fifth,
		       uintptr_t sixth)
{
	return btSbcParsePacket(first, second, third, fourth, fifth, sixth);
}

void btSbcAcquireContext(void)
{
}

void btSbcReleaseContext(void)
{
	fmDevice = mdevOpenDevice("fm", 0);
}

/* Asks the link controller to switch a link's role. */
void btSbcSetupContext(const uint8_t *address, uint8_t role)
{
	uint8_t *message;

	message = osa_MsgAlloc(12);
	*(uint32_t *)message = BT_MESSAGE_LINK_REQUEST;
	__aeabi_memcpy(message + 4, address, 6);
	message[10] = role;
	osa_MsgSend((void **)&message, BT_QUEUE_VENDOR_POWER);
}

/* A link went down (event kind 1) during a role switch: reports the switch with the role asked for. */
void btSbcClearContext(const uint8_t *event)
{
	unsigned int link;

	if (event[12] != 1)
		return;
	link = *(const uint16_t *)(event + 10);
	if (btRoleSwitches.state[link] == BT_ROLE_SWITCH_IDLE)
		return;
	btBdhRomPrepareContext(-1, 0);
	btBdhRomConfigureContext(BT_EVENT_ROLE_CHANGE, 0x202, "bmb", 12, event + 4, 6, btRoleSwitches.role[link]);
	btRoleSwitches.state[link] = BT_ROLE_SWITCH_IDLE;
}

/*
 * Encryption Change: reported to the host, and the next step of a role
 * switch: the switch once encryption is paused, the Role Change event once
 * it is resumed.
 */
void btSbcConvertContext(unsigned int status, unsigned int link, unsigned int enabled)
{
	uint8_t *records;
	uint8_t *record;
	unsigned int state;

	records = btHccRomApplyState();
	btBdhRomConfigureContext(BT_EVENT_ENCRYPTION_CHANGE, 0x80, "bhb", status, link, enabled);
	state = btRoleSwitches.state[link];
	record = records + BT_LINK_RECORD * link;
	if (state == BT_ROLE_SWITCH_PAUSING) {
		btSbcSetupContext(record + BT_LINK_RECORD_ADDRESS, btRoleSwitches.role[link]);
		btRoleSwitches.state[link] = status == 0 ? BT_ROLE_SWITCH_RUNNING : BT_ROLE_SWITCH_IDLE;
	} else if (state == BT_ROLE_SWITCH_RESUMING) {
		btBdhRomPrepareContext(-1, 0);
		if (btRoleSwitches.status[link] != 0)
			status = btRoleSwitches.status[link];
		btBdhRomConfigureContext(BT_EVENT_ROLE_CHANGE, 0x202, "bmb", status, record + BT_LINK_RECORD_ADDRESS, 6,
					 btRoleSwitches.role[link]);
		btSbcDispatchEntry(BT_EVENT_ROLE_CHANGE);
		btRoleSwitches.state[link] = BT_ROLE_SWITCH_IDLE;
	} else {
		btBdhRomPrepareContext(-1, link);
		btSbcDispatchEntry(BT_EVENT_ENCRYPTION_CHANGE);
	}
}

/*
 * HCI Switch_Role (a BD_ADDR and the role): pauses the link's encryption
 * first when the link can switch, else asks for the switch at once.
 */
void btSbcVerifyContext(const uint8_t *command)
{
	const uint8_t *address = command + 7;
	unsigned int opcode = *(const uint16_t *)(command + 4);
	unsigned int role;
	uint8_t *records;
	uint8_t *record;
	uint16_t slot;
	unsigned int link;

	if (command[6] != 7 || (role = address[6]) >= 2) {
		btBdhRomValidateContext(BT_STATUS_INVALID, opcode);
		return;
	}
	records = btHccRomApplyState();
	link = 0xffff;
	if (btLinkAddressHook(address, &slot) != 0 && btLscRomPrepareState(slot) != 0 &&
	    btLdrRomHandleState(records + BT_LINK_RECORD * slot + 22, 43) == 0 &&
	    btLscRomValidateState(slot) == 0) {
		record = records + BT_LINK_RECORD * slot;
		if (*(uint16_t *)(record + 30) == 0 && record[BT_LINK_RECORD_ROLE] != role &&
		    btLdrRomApplyState(slot, record[BT_LINK_RECORD_ROLE], 0) == 0)
			link = slot;
	}
	if (link != 0xffff) {
		btRoleSwitches.state[link] = BT_ROLE_SWITCH_PAUSING;
		btRoleSwitches.role[link] = address[6];
		btSbcPrepareEntry(BT_LMP_PAUSE_ENCRYPTION, link, 0);
	} else {
		/* The original clears the state of "link" 0xffff too, 64 KiB past the table. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
		btRoleSwitches.state[link] = BT_ROLE_SWITCH_IDLE;
#pragma GCC diagnostic pop
		btSbcSetupContext(address, address[6]);
	}
	btBdhRomPrepareContext(1, 0);
	btBdhRomValidateContext(0, opcode);
}

/* The link controller switched a link's role: resumes encryption first when it was paused. */
void btSbcSelectContext(unsigned int status, const uint8_t *address, unsigned int role)
{
	uint16_t slot;
	unsigned int state = BT_ROLE_SWITCH_IDLE;

	btHccRomApplyState();
	if (btLinkAddressHook(address, &slot) != 0)
		state = btRoleSwitches.state[slot];
	if (state == BT_ROLE_SWITCH_RUNNING) {
		btSbcPrepareEntry(BT_LMP_PAUSE_ENCRYPTION, slot, 1);
		btRoleSwitches.state[slot] = BT_ROLE_SWITCH_RESUMING;
		btRoleSwitches.role[slot] = role;
		btRoleSwitches.status[slot] = status;
		return;
	}
	btBdhRomPrepareContext(-1, 0);
	btBdhRomConfigureContext(BT_EVENT_ROLE_CHANGE, 0x202, "bmb", status, address, 6, role);
	btSbcDispatchEntry(BT_EVENT_ROLE_CHANGE);
}

uint8_t btSbcAdjustContext(unsigned int link)
{
	return btRoleSwitches.state[link];
}

/* The vendor task's messages: 1 when the message was taken (and freed). */
int btSbcRecordContext(uint8_t *message)
{
	const uint8_t *parameters;
	unsigned int opcode;
	unsigned int group;
	unsigned int command;
	unsigned int length;
	uint32_t status;
	uint8_t key[8];
	uint8_t record[12];
	int taken = 0;

	switch (*(uint32_t *)message) {
	case 5:
		taken = btLmpLcResetFrame(message);
		break;
	case 19:
		taken = btLmpLcNotifyFrame(message);
		break;
	case 21:
		btSbcClearContext(message);
		taken = btLmpLcApplyFrame(message);
		break;
	case 31:
		taken = btLmpLcScheduleFrame(0);
		break;
	case 32:
		taken = btLmpLcScheduleFrame(message[4]);
		break;
	case 34:
		if (btHostSettings[11] != 2 && *(void **)(message + 20) != NULL) {
			romHctReleaseBuffer(*(void **)(message + 20));
			*(void **)(message + 20) = NULL;
		}
		return 0;
	case 68:
		btSbcConvertContext(0, *(uint16_t *)(message + 4), message[6]);
		goto taken;
	case 69:
		btSbcConvertContext(message[4], *(uint16_t *)(message + 6), message[8]);
		goto taken;
	case 81:
		btSbcSelectContext(0, message + 4, message[10]);
		goto taken;
	case 82:
		btSbcSelectContext(message[4], message + 5, message[11]);
		goto taken;
	case 86:
		if (btHandleStates[*(uint16_t *)(message + 4)].flags == 2)
			btHandleStates[*(uint16_t *)(message + 4)].flags = 1;
		return 0;
	case BT_MESSAGE_P2P_TABLE:
		p2pSqramCheckTable();
		return 0;
	case BT_MESSAGE_LINK_PACKET:
		taken = btLmpLcInspectPacket(message);
		break;
	case BT_MESSAGE_HCI_COMMAND:
		opcode = *(uint16_t *)(message + BT_COMMAND_OPCODE);
		length = message[BT_COMMAND_LENGTH];
		parameters = message + BT_COMMAND_PARAMETERS;
		group = opcode >> 10;
		command = opcode & 0x3ff;
		switch (group) {
		case 1:
			if (command != 6)
				return 0;
			taken = btSbcParseEntry(message);
			break;
		case 2:
			if (command != 11)
				return 0;
			btSbcVerifyContext(message);
			goto taken;
		case 3:
			if (command == 3) {
				btRfTestStop();
			} else if (command == 5) {
				btLmpLcResolveFrame(length, parameters);
				goto taken;
			} else if (command == 51 && (unsigned int)(message[7] | message[8] << 8) > 0x3fd) {
				message[7] = 0xfd;
				message[8] = 3;
			}
			return 0;
		case 5:
			if (command != 7)
				return 0;
			btLmpLcSetupFrame(parameters);
			goto taken;
		case 63:
			switch (command) {
			case 25:
				btBdhRomCheckContext(4, btRfTestCommand(length, parameters), group, command);
				goto taken;
			case 29:
				btVendorPolicyFlag = parameters[0] == 0 ? 1 : 0;
				return 0;
			case 34:
				status = 0;
				btBdhRomQueryState(parameters, &status);
				if ((uint8_t)status == 0) {
					btLdaRomDispatchState(0);
					btVendorKeyHook(0xfe, 6, key);
					btLdaRomBuildState(record, key);
					btLdaRomConfigureState(btVendorP2pRecords, record, sizeof(record));
					btLdaRomConfigureState(btVendorP2pRecords + 76, record, sizeof(record));
				}
				btBdhRomCheckContext(4, (uint8_t)status, 63, 34);
				goto taken;
			case 97:
				if (btSbcUpdateContext(parameters) != 0) {
					btBdhRomCheckContext(4, BT_STATUS_INVALID, 63, 97);
					goto taken;
				}
				romVendorPending = btSbcNotifyContext;
				btSbcScheduleContext();
				goto taken;
			default:
				return 0;
			}
		default:
			return 0;
		}
		break;
	default:
		return 0;
	}
	if (taken == 0)
		return 0;
	goto free;
taken:
	taken = 1;
free:
	osa_MsgFree((void **)&message);
	return taken;
}

int btSbcFilterContext(const uint8_t *message)
{
	(void)message;
	return 0;
}

/* The controller's reset: the ROM's default settings, and the link timing records' words. */
void btSbcHandleEntry(void)
{
	struct __attribute__((packed)) {
		uint8_t kind;
		uint32_t first;
		uint32_t second;
		uint8_t third;
		uint32_t fourth;
		uint8_t fifth;
		uint32_t sixth;
	} settings;
	uint8_t *timing;

	timing = btLtsRomValidateState(0);
	settings.kind = 2;
	settings.first = 0xf0000;
	settings.second = 0;
	settings.third = 1;
	settings.fourth = 1;
	settings.fifth = 1;
	settings.sixth = 1;
	btBdhRomResetState((const uint8_t *)&settings);
	btLmpLcPrepareState();
	btEventFilterCount = 0;
	btEventFilterAccept = 1;
	btTableProfile = 0;
	if (btVendorPolicyFlag == 0)
		btLinkControlWord[2] |= 0xf0;
	*(BtHalf *)(timing + 6) = 0x3c04;
	*(BtHalf *)(timing + 46) = 0x3c45;
	*(BtHalf *)(timing + 86) = 0x3c86;
}

/* A command complete's return parameters, patched before they go to the host. */
int btSbcProcessEntry(unsigned int status, const uint8_t *parameters, unsigned int group, unsigned int command)
{
	unsigned int version;

	(void)status;
	(void)parameters;
	if (group == 4) {
		if (command == 2) {
			if (btLocalFeatureMode < 4) {
				btHciReturnParameters[24] &= 0xef;
				btHciReturnParameters[28] &= ~1;
			}
		} else if (command == 1) {
			version = btLmpLcCheckContext();
			btHciReturnParameters[10] = version;
			btHciReturnParameters[11] = version >> 8;
		}
	} else if (group == 63 && command == 9) {
		return 1;
	}
	return 0;
}

int btSbcUpdateEntry(void)
{
	return 0;
}

int btSbcConfigureEntry(void)
{
	return 0;
}

/* Vendor command 107: a message for the HCT task. */
void btSbcCheckEntry(unsigned int length, const uint8_t *parameters, uint8_t *status, uint8_t *returnLength,
		     uint8_t *returnParameters)
{
	uint8_t *message = NULL;

	(void)returnLength;
	(void)returnParameters;
	*status = 0;
	if (length - 1 > 21) {
		*status = BT_STATUS_INVALID;
		return;
	}
	message = osa_MsgAlloc(28);
	if (message == NULL) {
		*status = 7;
		return;
	}
	*(uint32_t *)message = BT_MESSAGE_VENDOR_COMMAND;
	message[8] = parameters[0];
	message[9] = 0;
	if (parameters[0] != 0 && length > 1) {
		if (length < 7 || parameters[5] > 16) {
			*status = BT_STATUS_INVALID;
			osa_MsgFree((void **)&message);
		} else {
			*(uint16_t *)(message + 4) = parameters[1] | parameters[2] << 8;
			*(uint16_t *)(message + 6) = parameters[3] | parameters[4] << 8;
			message[9] = parameters[5];
			__aeabi_memcpy(message + 10, parameters + 6, parameters[5]);
		}
	}
	if (*status == 0)
		osa_MsgSend((void **)&message, romQueueHct);
}

/* Vendor command 91: asks the link controller, and reports its answer as a vendor event. */
void btSbcValidateEntry(unsigned int length, const uint8_t *parameters, uintptr_t argument,
			uint8_t *returnLength, uint8_t *status, uint8_t *returnParameters)
{
	unsigned int result = 0;

	(void)argument;
	(void)returnParameters;
	switch (btLmpLcResetContext(length, parameters)) {
	case 2:
		result = 12;
		/* fall through */
	case 0:
		if (length == 3)
			btBdhRomConfigureContext(0xff, 0, "Vbbbbb", 91, parameters[0], parameters[1], parameters[2],
						 result);
		else
			btBdhRomConfigureContext(0xff, 0, "Vbbbb", 91, parameters[0], result, 0);
		break;
	default:
		break;
	}
	*returnLength = 0;
	*status = result;
}

/* A message to the link controller's task. */
void btSbcPrepareEntry(unsigned int request, unsigned int link, unsigned int value)
{
	uint8_t *message;

	message = osa_MsgAlloc(8);
	*(uint32_t *)message = request;
	*(uint16_t *)(message + 4) = link;
	message[6] = value;
	osa_MsgSend((void **)&message, romQueueLcp);
}

/* After an event the role switch or a disconnection sends: a Command Status for no command. */
__attribute__((noinline)) void btSbcDispatchEntry(unsigned int event)
{
	(void)event;
	if (btVendorEventMode == 3)
		btBdhRomConfigureContext(15, 0x140, "bRh", 0, 0);
}

/*
 * HCI Disconnect (a handle and the reason): marks the handle disconnecting,
 * and asks the link controller to drop a SCO link that rides on it first.
 */
int btSbcParseEntry(const uint8_t *command)
{
	unsigned int opcode = *(const uint16_t *)(command + BT_COMMAND_OPCODE);
	const uint8_t *parameters = command + BT_COMMAND_PARAMETERS;
	unsigned int handle = parameters[0] | parameters[1] << 8;
	unsigned int reason;
	unsigned int kind;
	unsigned int sco;

	if (command[BT_COMMAND_LENGTH] != 3 || handle >= 0xf00)
		return 0;
	if (btHandleStates[handle].state == 2 || btHandleStates[handle].state == 0)
		return 0;
	reason = parameters[2];
	if (handle >= 8 || reason == 0)
		return 0;
	kind = btHccRomBuildState(handle);
	sco = btHccRomResetState(handle);
	if (kind == 7 || kind == 11 || sco < 8 || sco >= 11)
		return 0;
	btScoDisconnectLinks[sco - 8] = handle;
	if (btHandleStates[sco].state != 2 && btHandleStates[sco].state != 0)
		btSbcPrepareEntry(0x12, sco, reason);
	btHandlePending[handle] = reason;
	btHandleStates[handle].state = 2;
	btBdhRomPrepareContext(1, handle);
	btBdhRomValidateContext(0, opcode);
	return 1;
}

/* Registers the two ROM drivers. */
void btSbcBuildEntry(void)
{
	unsigned int i;

	for (i = 0; i < 2; i++) {
		btSbcDriverStates[i] = 0;
		btSbcDrivers[i].index = i;
		btSbcDrivers[i].name = btSbcDriverNames[i];
		btSbcDrivers[i].callbacks[0] = btSbcRomAdjustEntry;
		btSbcDrivers[i].callbacks[1] = btSbcRomRecordEntry;
		btSbcDrivers[i].callbacks[2] = btSbcRomFilterEntry;
		btSbcDrivers[i].callbacks[3] = btSbcRomHandleTable;
		btSbcDrivers[i].callbacks[4] = btSbcRomProcessTable;
		btSbcDrivers[i].callbacks[5] = btSbcRomConfigureTable;
		btSbcDrivers[i].callbacks[6] = btSbcRomUpdateTable;
		mdevRegisterDriver(&btSbcDrivers[i], btSbcRomUpdateTable);
	}
}
