/*
 * The transmit level of the Bluetooth controller (0x42488 - 0x42b3e). The level follows the temperature
 * the SoC's sensor reports (btLmpLcClearState, with the hysteresis of btLmpLcSetupState), the tables give
 * the radio's register words for each level, and btLmpLcManageState() hands the controller's ROM the
 * hooks that read them.
 *
 * The filename and the roles are inferred; the function names are the firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/clock.h"
#include "robin/eventpower.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/socpower.h"

/* A halfword of the link controller's register block. */
#define LINK_U16(offset)	(*(volatile uint16_t *)(btLinkContext + (offset)))

/* The level control's state: the table of its radio words is reset by btLmpLcConfigureState(). */
struct BtPowerState btPower __attribute__((section(".itcm.rw"))) = {
	.packet = 8,
};

/* The radio register word of each level, and the record word that goes with it. */
uint16_t btPowerIndexRegister[BT_POWER_LEVELS] __attribute__((section(".itcm.rw"))) = {
	0x0c1c, 0x0d1c, 0x0e1c, 0x0f1c, 0x101c, 0x111c, 0x121c, 0x141c,
	0x151c, 0x161c, 0x181c, 0x191c, 0x1b1c, 0x1d1c, 0x1f1c, 0x211c,
	0x231c, 0x251c, 0x281c, 0x2a1c, 0x2d1c, 0x301c, 0x331c, 0x371c,
	0x3a1c, 0x3e1c, 0x411c, 0x451c, 0x491c, 0x4e1c, 0x531c, 0x581c,
	0x5e1c, 0x641c, 0x6a1c, 0x711c, 0x781c, 0x7f1c, 0x871c, 0x901c,
	0x991c, 0xa41c, 0xae1c, 0xb91c, 0xc51c, 0xd31c, 0xe11c, 0xf01c,
	0x011d, 0x141d, 0xf32c, 0x032d, 0x152d, 0x282d, 0x3e2d, 0x2a3d,
	0x3e3d, 0x543d, 0x6c3d, 0x684d, 0x814d, 0x9c4d, 0x9c5d, 0xb95d,
	0xc46d, 0xd17d, 0xf27d,
};
uint16_t btPowerIndexRecord[BT_POWER_LEVELS] __attribute__((section(".itcm.rw"))) = {
	0x0c1c, 0x0d1c, 0x0e1c, 0x0f1c, 0x101c, 0x121c, 0x131c, 0x151c,
	0x161c, 0x181c, 0x1a1c, 0x1c1c, 0x1e1c, 0x201c, 0x221c, 0x251c,
	0x271c, 0x2a1c, 0x2d1c, 0x301c, 0x331c, 0x361c, 0x3a1c, 0x3d1c,
	0x421c, 0x461c, 0x4a1c, 0x4f1c, 0x541c, 0x581c, 0x5e1c, 0x641c,
	0x6a1c, 0x711c, 0x781c, 0x7f1c, 0x881c, 0x901c, 0x991c, 0xa31c,
	0xad1c, 0xb81c, 0xc41c, 0xd41c, 0xe01c, 0xef1c, 0xfe1c, 0x0f1d,
	0xf02c, 0x002d, 0x112d, 0x222d, 0x352d, 0x492d, 0x3a3d, 0x4e3d,
	0x633d, 0x7a3d, 0x724d, 0x894d, 0xa44d, 0xc84d, 0xdc4d, 0xfc4d,
	0x294e, 0x444e, 0x6f4e,
};
uint8_t btPowerLevelHeader __attribute__((section(".itcm.rw"))) = 1;			/* 0x55a32 */
uint8_t btPowerLevelContext __attribute__((section(".itcm.rw")));			/* 0x55797 */


/* Posts the state to the host transport's queue (message 0x1400). */
void btLmpLcHandleState(int state)
{
	uint32_t *message;

	sysEnterPrimaryEvent();
	message = osa_MsgAlloc(8);
	if (message != NULL) {
		message[0] = 0x1400;
		message[1] = state;
		osa_MsgSend((void **)&message, romQueueHct);
	}
}

/* Starts the control: the registers it owns, the level, and the sync of the SBC context. */
int btLmpLcProcessState(void)
{
	unsigned int rate;
	volatile unsigned int gate;

	LINK_U16(358) &= 0xcfff;
	LINK_U16(358) |= 0x1000;
	LINK_U16(38) = 22;
	LINK_U16(62) = 0x8040;
	LINK_U16(352) |= 0x40;
	rate = btPower.control;
	if (btPower.control == 260 && btPowerLevelContext != 7)
		rate = btPower.control >> 1;
	btBdhRomConfigureState(rate, 0);
	btPowerLevelHeader = 1;
	if (btPower.table == 1)
		btLmpLcApplyEntry();
	gate = SOC_EVENT_ENGINE_GATE;
	SOC_EVENT_ENGINE_GATE = gate & 0xfeffffff;
	SOC_EVENT_ENGINE_GATE = (gate & 0xfeffffff) | 0x8000;
	btSbcSyncContext();
	return btLmpLcNotifyEntry();
}

/* Reads the radio mode (selector 4) into the state. */
unsigned int btLmpLcUpdateState(void)
{
	uint8_t mode;

	btSbcFilterState(4, &mode);
	if (btPower.frame == 0) {
		btThresholdPayload = (mode & 1) ? 5 : 2;
		btPower.header = (mode & 2) != 0;
		return (mode >> 1) & 1;
	}
	return btPower.frame;
}

/*
 * Takes the radio's settings (selectors 2, 3, 5 and 6) and puts the level index of the first channel in
 * *first and of the channel in use in *channel; the second one is also the table of records' base.
 */
uint8_t *btLmpLcConfigureState(uint8_t *first, uint8_t *channel)
{
	int8_t reading;
	int sum;
	unsigned int index;

	btSbcFilterState(2, (uint8_t *)&btModeState);
	btSbcFilterState(3, (uint8_t *)&reading);
	btSbcFilterState(5, &btMaskFrame);
	btSbcFilterState(6, &btMaskPacket);
	btModeContext = btPower.packet;
	sum = (int8_t)(2 * (btModeState + reading)) + 40;
	*channel = sum / btPower.packet + 1;
	btPower.buffer = sum % btModeContext;
	if (btThresholdPayload == 5) {
		index = btPower.descriptor;
		if (index == 0) {
			for (index = 0; index < 0x3b; index++)
				btPowerIndexRecord[index] = btPowerIndexRecord[index + 8];
			for (; index < 0x43; index++)
				btPowerIndexRecord[index] = btPowerIndexRecord[BT_POWER_LEVELS - 1];
			btPower.descriptor = 1;
			btPower.entry = -8;
		}
		*first = *channel;
	} else {
		*first = 0x40 / btModeContext + 1;
	}
	btThresholdRecord = (*first - *channel) * btModeContext + btModeState;
	return &btThresholdRecord;
}

/* The device's bit of the peer packet mask goes to the register. */
uint8_t *btLmpLcCheckState(uint8_t a1)
{
	unsigned int bit = 1 << a1;
	uint16_t value = LINK_U16(462);

	if (btPeerPacket & bit)
		value |= bit;
	else
		value &= ~bit;
	LINK_U16(462) = value;
	return btLinkContext + 448;
}

/* The controller's command sets the radio mode, and is passed to the ROM. */
int btLmpLcValidateState(uint8_t *command)
{
	btPower.frame = 1;
	btThresholdPayload = command[9] == 0 ? 5 : 2;
	btPower.packet = command[10];
	btPower.header = command[11] == 0;
	btBdhRomDispatchState(command);
	return 0;
}

int btLmpLcPrepareState(void)
{
	uint8_t *context = btLcoRomComputeContext(0);

	if (btPower.header != 0 && btThresholdPayload == 5)
		context[2] &= 0xfb;
	else
		context[2] |= 4;
	return (int)(uintptr_t)context;
}

int btLmpLcDispatchState(uint32_t a1, uint32_t a2)
{
	btBdhRomBuildState(a2, a1);
	return 0;
}

/* The hook the ROM calls to set the control up: the registers, the radio mode and the level. */
int btLmpLcParseState(uint32_t a1, uint32_t a2)
{
	volatile unsigned int gate;

	btPower.queue = 0;
	btLscRomScheduleState(8, a2);
	LINK_U16(34) = 0;
	LINK_U16(28) = 0x200f;
	*(volatile uint16_t *)btLinkRecord = 0x505;
	gate = SOC_EVENT_ENGINE_GATE;
	SOC_EVENT_ENGINE_GATE = gate | 0x4000;
	btLmpLcUpdateState();
	btLmpLcProcessState();
	LINK_U16(28) |= 0x8000;
	LINK_U16(32) = 0x300;
	btBdhRomCheckState(btLinkContext);
	btPeerPacket = 0;
	btLmpLcConfigureState(&btPower.timer, &btPower.channel);
	btPower.param = 0;
	btPower.config = 0;
	btPower.status = 0;
	btPower.table = 1;
	return 1;
}

uint8_t *btLmpLcBuildState(void)
{
	if (btPower.queue == 0)
		btPower.queue = 1;
	return (uint8_t *)&btPower;
}

uint8_t *btLmpLcComputeState(void)
{
	if (btPower.queue == 1)
		btPower.queue = 0;
	return (uint8_t *)&btPower;
}

int btLmpLcQueryState(uint8_t a1)
{
	return -a1;
}

/* The device's record of the level at the index. */
static uint16_t *btPowerFillValue(int link, int index)
{
	struct BtPowerValue *value = &btLinkValues[link];

	value->index = btPowerIndexRegister[index];
	value->range = (unsigned int)index <= btPowerLevelHeader + 48u ? 0 : 3;
	value->record = btPowerIndexRecord[index];
	value->mode = btPower.entry + btPowerLevelHeader + 47 >= index ? 6 : 7;
	return &value->range;
}

/* The device's record of the level of the channel in use. */
int btLmpLcResetState(int link)
{
	LINK_U16(360) |= 1 << link;
	btPowerFillValue(link, btLmpLcSelectState(btPower.channel));
	return btPower.channel;
}

__attribute__((noinline)) uint16_t *btLmpLcSyncState(int link, int index)
{
	LINK_U16(360) |= 1 << link;
	return btPowerFillValue(link, index);
}

uint16_t *btLmpLcApplyState(int link, uint8_t channel)
{
	return btLmpLcSyncState(link, btLmpLcSelectState(channel));
}

/* The level of the channel in the registers of the two links that follow it. */
unsigned int btLmpLcResolveState(uint8_t channel)
{
	unsigned int index = btLmpLcSelectState(channel);
	uint16_t range = index <= btPowerLevelHeader + 48u ? 0 : 3;

	LINK_U16(382) = btPowerIndexRegister[index];
	LINK_U16(384) = range;
	LINK_U16(522) = btPowerIndexRegister[index];
	LINK_U16(524) = range;
	return index;
}

int btLmpLcScheduleState(uint8_t *timer)
{
	if (timer != NULL)
		*timer = btPower.timer;
	return btPower.timer;
}

uint8_t *btLmpLcNotifyState(void)
{
	uint8_t *record = btLinkRecord;

	*(volatile uint16_t *)(record + 18) &= 0xff3f;
	return record;
}

uint8_t *btLmpLcFormatState(void)
{
	uint8_t *record = btLinkRecord;

	*(volatile uint16_t *)(record + 18) = (*(volatile uint16_t *)(record + 18) & 0xff3f) + 128;
	return record;
}

int btLmpLcInspectState(void)
{
	return btLmpLcAdjustState(btPower.timer);
}

/* The level at the radio's threshold temperature goes to the registers. */
unsigned int btLmpLcTrackState(void)
{
	unsigned int index = btThresholdRegister;

	if (btThresholdRegister != -1) {
		index = (uint8_t)(2 * btThresholdRegister + btPowerLevelHeader + 40);
		LINK_U16(522) = btPowerIndexRegister[index];
		LINK_U16(524) = index <= btPowerLevelHeader + 48u ? 0 : 3;
	}
	return index;
}

/* Installs the hooks. */
unsigned int (*btLmpLcManageState(void))(void)
{
	btPowerHooks.command = btLmpLcParseState;
	btPowerHooks.profile = btLmpLcBuildState;
	btPowerHooks.event = btLmpLcComputeState;
	btPowerHooks.payload = btLmpLcQueryState;
	btPowerHooks.context = btLmpLcResetState;
	btPowerHooks.entry = btLmpLcApplyState;
	btPowerHooks.table = btLmpLcResolveState;
	btPowerHooks.state = btLmpLcScheduleState;
	btPowerHooks.reg = btLmpLcAdjustState;
	btPowerHooks.record = btLmpLcInspectState;
	btPowerHooks.policy = btLmpLcNotifyState;
	btPowerHooks.cipher = btLmpLcFormatState;
	btPowerThresholdRate = btLmpLcCheckState;
	btPowerThresholdBitmap = btLmpLcTrackState;
	return btLmpLcTrackState;
}

/* Hands the ROM the frames the controller received since the last time (14 or more clock ticks). */
uint8_t *btLmpLcAcquireState(void)
{
	uint32_t now = *(volatile uint32_t *)(btLinkEvent + 8);
	unsigned int elapsed = (now - btPeerFrame) & 0x3ffffff;

	if (elapsed >= 14) {
		btPeerFrame = *(volatile uint32_t *)(btLinkEvent + 8);
		btBdhRomValidateState(now, elapsed, btPeerBuffer);
		btBdhRomParseState();
		btLdaRomNotifyState();
	}
	LINK_U16(400) = *(volatile uint16_t *)btLinkStatus;
	return btLinkContext + 384;
}

/* Every device that is in use gets its record again. */
void btLmpLcReleaseState(void)
{
	unsigned int device;

	for (device = 0; device < BT_POWER_DEVICES; device = (uint8_t)(device + 1)) {
		if (btDevicePolicy[device] != 6)
			btPowerHooks.entry(device, btDeviceRate[device]);
	}
}

/*
 * The temperature moves the level: it falls by one when the sample is 0x5b or less, and rises by one when
 * it is 0x76 or more; once it has moved, the level changes back at 0x70 and 0x61, and by two when the
 * sample went through the other threshold.
 */
int btLmpLcSetupState(void)
{
	int result = btPower.param;

	if (btPower.status == 0) {
		if (btPower.param >= 0x76) {
			result = 1;
			btPower.status = 1;
			btPowerLevelHeader++;
		} else if (btPower.param <= 0x5b) {
			result = 255;
			btPower.status = -1;
			btPowerLevelHeader--;
		}
		return result;
	}
	if (btPower.status == 1) {
		if (btPower.param > 0x5b) {
			if (btPower.param > 0x70)
				return result;
			btPower.status = 0;
			btPowerLevelHeader--;
			return result;
		}
		result = 255;
		btPowerLevelHeader = btPowerLevelHeader - 2;
		btPower.status = -1;
		return result;
	}
	if (btPower.status != -1)
		return result;
	if (btPower.param < 0x76) {
		if (btPower.param >= 0x61) {
			btPower.status = 0;
			btPowerLevelHeader++;
		}
		return result;
	}
	result = 1;
	btPowerLevelHeader = btPowerLevelHeader + 2;
	btPower.status = 1;
	return result;
}

/* Samples the temperature; a changed sample moves the level. */
void btLmpLcClearState(void)
{
	int sample;

	if (btPowerLevelContext != 7) {
		sample = socSensorSample();
		if (sample != btPower.param) {
			btPower.param = sample;
			btLmpLcSetupState();
			btPower.config = 1;
		}
	}
}

/* Tells the radio (registers 71 and 377) the temperature. */
int btLmpLcConvertState(void)
{
	uint8_t value;
	unsigned int control;

	value = btBdhRomProcessState(71, 1);
	btBdhRomUpdateState(71, value & 0x7f);
	control = btBdhRomProcessState(71, 1);
	btBdhRomUpdateState(377, btPower.param);
	btBdhRomUpdateState(71, control | 0x80);
	return 0;
}

/* A changed level is passed on to the radio and to the devices. */
int btLmpLcVerifyState(void)
{
	if (btPower.config != 0) {
		btLmpLcConvertState();
		btLmpLcReleaseState();
		btPower.config = 0;
		return 0;
	}
	return btPower.config;
}

/* The level index of the channel: 66 at most. */
__attribute__((noinline)) unsigned int btLmpLcSelectState(uint8_t channel)
{
	unsigned int index = (uint8_t)(btModeContext * (channel - 1) + btPower.buffer + btPowerLevelHeader);

	return index >= 0x43 ? 66 : index;
}

/* The level in steps of two, from the middle of the table. */
__attribute__((noinline)) int btLmpLcAdjustState(uint8_t channel)
{
	return (int8_t)(((int)btLmpLcSelectState(channel) - 40) / 2);
}

/* The clocks the transmitter needs (32000 kHz). */
unsigned int btLmpLcFilterState(void)
{
	clockRequestCore(0x7d00, 1);
	clockRequestBus(0x7d00, 1);
	clockRequestAux(0x7d00, 1);
	return clockRequestVote(0x7d00, 1);
}
