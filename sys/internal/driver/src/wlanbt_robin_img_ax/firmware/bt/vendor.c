/*
 * The Bluetooth vendor commands and the radio set-up around them
 * (include/robin/vendor.h).
 */
#include <stdint.h>
#include "robin/btbuffer.h"
#include "robin/btconfig.h"
#include "robin/btpower.h"
#include "robin/buffer.h"
#include "robin/fm.h"
#include "robin/mdev.h"
#include "robin/btrom.h"
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
		*status = p2pSqramBuildEntry(length, parameters);
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

struct BtLinkRequests btLinkRequests __attribute__((section(".itcm.rw")));

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

/* Sends a link request's address and value to the link controller. */
void btSbcSetupContext(const uint8_t *address, uint8_t value)
{
	uint8_t *message;

	message = osa_MsgAlloc(12);
	*(uint32_t *)message = BT_MESSAGE_LINK_REQUEST;
	__aeabi_memcpy(message + 4, address, 6);
	message[10] = value;
	osa_MsgSend((void **)&message, BT_QUEUE_VENDOR_POWER);
}

/* A link's request ended (event kind 1): reports it to the host, with its value, and forgets it. */
void btSbcClearContext(const uint8_t *event)
{
	unsigned int link;

	if (event[12] != 1)
		return;
	link = *(const uint16_t *)(event + 10);
	if (btLinkRequests.state[link] == BT_LINK_REQUEST_IDLE)
		return;
	btBdhRomPrepareContext(-1, 0);
	btBdhRomConfigureContext(18, 0x202, "bmb", 12, event + 4, 6, btLinkRequests.value[link]);
	btLinkRequests.state[link] = BT_LINK_REQUEST_IDLE;
}
