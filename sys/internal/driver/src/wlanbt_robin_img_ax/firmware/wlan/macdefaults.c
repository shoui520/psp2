/*
 * The MAC's register defaults and the local station's defaults
 * (0x4c80c..0x4cb36). Marvell published no source; names are inferred from
 * use.
 */
#include <stdint.h>
#include "robin/beacontrack.h"
#include "robin/btsource.h"
#include "robin/macdefaults.h"
#include "robin/macentry.h"
#include "robin/mactx.h"
#include "robin/phy.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/txhold.h"
#include "robin/txpower.h"

#define MAC_088_TX_CONTROL	MAC_REG(0x088)
#define MAC_138_PARAMETER	MAC_REG(0x138)
#define MAC_4D4_DESCRIPTOR	MAC_REG(0x4d4)
#define MAC_DD0_TX_VALUE	MAC_REG(0xdd0)
#define MAC_DD4_TX_TIMING	MAC_REG(0xdd4)

#define MAC_RATES		13	/* the rate indices the ROM keeps tables for */

extern uint32_t wlanPsWakeAdvance;			/* 0x56260 */

struct MacRegisterDefault macRegisterDefaults[MAC_REGISTER_DEFAULTS]
	__attribute__((section(".dtcm.rw"))) = {
	{ 0x0178, 0, 0x00006000, 0x00004000 },
	{ 0x0198, 0, 0x00000004, 0x00000004 },
	{ 0x0240, 0, 0xffffffff, 0x0f3c421a },
	{ 0x0244, 0, 0xffffffff, 0x140a1414 },
	{ 0x0248, 0, 0xffffffff, 0x0c2e240c },
	{ 0x024c, 0, 0xffffffff, 0x06060c06 },
	{ 0x02d4, 0, 0x1d3fffb5, 0x180e4000 },
	{ 0x02dc, 0, 0x000000ff, 0x00000014 },
	{ 0x031c, 0, 0x00008380, 0x00000380 },
	{ 0x0370, 0, 0x00000001, 0x00000001 },
	{ 0x04d4, 0, 0x7800ffff, 0x00000043 },
	{ 0x04f4, 0, 0x0000fff0, 0x00000650 },
	{ 0x04f8, 0, 0xffffffff, 0x012c1e14 },
	{ 0x04fc, 0, 0xffffffff, 0x00dc0584 },
	{ 0x06ac, 0, 0xffffffff, 0x000c0003 },
	{ 0x06b0, 0, 0xffffffff, 0x02d8016c },
	{ 0x06b4, 0, 0xffffffff, 0x026800d4 },
	{ 0x06b8, 0, 0xffffffff, 0x022000b1 },
	{ 0x06bc, 0, 0xffffffff, 0x020c00a7 },
	{ 0x06c0, 0, 0xffffffff, 0x02d800a2 },
	{ 0x06c4, 0, 0xffffffff, 0x02d8005e },
	{ 0x06c8, 0, 0xffffffff, 0x02d80056 },
	{ 0x06cc, 0, 0xffffffff, 0x020c0052 },
	{ 0x06d0, 0, 0xffffffff, 0x020c004e },
	{ 0x06d4, 0, 0xffffffff, 0x020c004e },
	{ 0x06d8, 0, 0xffffffff, 0x020c004a },
	{ 0x06dc, 0, 0xffffffff, 0x020c004a },
	{ 0x06e0, 0, 0xffffffff, 0x020c004a },
	{ 0x06e4, 0, 0xffffffff, 0x020c004a },
	{ 0x0778, 0, 0xffffffff, 0x40414141 },
	{ 0x077c, 0, 0xffffffff, 0x60177346 },
	{ 0x0804, 0, 0xef9fffff, 0x09020000 },
	{ 0x080c, 0, 0x1fdff7fc, 0x02c44000 },
	{ 0x0824, 0, 0xffff7ffa, 0x27080008 },
	{ 0x08c0, 0, 0xffffffff, 0xf8efffef },
	{ 0x0924, 0, 0x3f7f3f7f, 0x00010001 },
	{ 0x0928, 0, 0x0100ffff, 0x0000150c },
	{ 0x092c, 0, 0x00000800, 0x00000000 },
	{ 0x09f0, 0, 0x000000ff, 0x00000006 },
	{ 0x09f4, 0, 0x000000ff, 0x0000000c },
	{ 0x09f8, 0, 0x000000ff, 0x00000004 },
	{ 0x09fc, 0, 0x000000ff, 0x00000001 },
	{ 0x0ac0, 0, 0x331fff00, 0x3300a000 },
	{ 0x0ad8, 0, 0x000001c0, 0x000001c0 },
	{ 0x0af0, 0, 0x0307ff69, 0x0301ff40 },
	{ 0x0af4, 0, 0xffffffff, 0x0f9f018f },
	{ 0x0d1c, 0, 0x0000ffff, 0x00000032 },
	{ 0x0d20, 0, 0x0000ffff, 0x00000028 },
	{ 0x0d24, 0, 0xffffffff, 0x22222222 },
	{ 0x0d28, 0, 0xffffffff, 0x00000000 },
	{ 0x0e5c, 0, 0x00001000, 0x00001000 },
	{ 0x0ec0, 0, 0xffff0000, 0x02000000 },
	{ 0x0ec4, 0, 0xffff0000, 0x02000000 },
	{ 0x0ec8, 0, 0xffff0000, 0x02000000 },
	{ 0x0ecc, 0, 0xffff0000, 0x02000000 },
	{ 0x0ed0, 0, 0xffff0000, 0x02000000 },
	{ 0x0ed4, 0, 0xffff0000, 0x02000000 },
	{ 0x0ed8, 0, 0xffff0000, 0x02000000 },
	{ 0x0edc, 0, 0xffff0000, 0x02000000 },
	{ 0x0ef4, 0, 0xffffffff, 0x10007800 },
	{ 0x0ef8, 0, 0x00000fff, 0x00000e01 },
	{ 0xffff, 0, 0xffffffff, 0xffffffff },
};

uint32_t wlanIbssAtimTimer __attribute__((section(".itcm.rw")));
uint32_t macDefaultsTxControlSet __attribute__((section(".itcm.rw")));
struct WlanDeviceConfig *wlanDeviceAddress __attribute__((section(".itcm.bss")));
uint8_t *wlanLocalContext __attribute__((section(".itcm.bss")));

uint32_t fmCoexCheckBuffer(int on)
{
	uint32_t value = MAC_2D4_FLAGS;

	if (on != 0)
		value |= 2;
	else
		value &= ~2u;
	MAC_2D4_FLAGS = value;
	return value;
}

__attribute__((noinline)) uint32_t fmCoexValidateBuffer(void)
{
	uint32_t value = (MAC_138_PARAMETER | 1) & ~0x1eu;

	MAC_138_PARAMETER = value;
	return value;
}

__attribute__((noinline)) uint32_t fmCoexPrepareBuffer(void)
{
	uint32_t value = (MAC_13C_CONFIG >> 1 << 1) & ~0xc09c06u;

	MAC_13C_CONFIG = (wlanMacEntrySelectGet() << 11) | value;
	return wlanMacConfigApply(3);
}

uint32_t fmCoexDispatchBuffer(void)
{
	fmCoexValidateBuffer();
	return fmCoexPrepareBuffer();
}

/* Gives the ROM's per-rate tables their defaults. */
void fmCoexParseBuffer(void)
{
	/* the ROM routine is called with the rate alone here, without the rate set */
	unsigned int (*responseRate)(unsigned int rate) = (unsigned int (*)(unsigned int))(uintptr_t)romLibManageContext;
	uint8_t rate;

	for (rate = 0; rate < MAC_RATES; rate++) {
		romLibSetupContext(rate, wlanRateCode(rate));
		romLibConvertContext(rate, responseRate(rate));
		romLibReleaseContext(rate, (uint8_t)wlanMacEntrySelectGet(), 0);
		romLibVerifyContext(rate, 0);
	}
	MAC_824_CONTROL &= ~0x8000u;
}

/* Applies macRegisterDefaults to the register block at registers. */
int fmCoexBuildBuffer(volatile uint32_t *registers)
{
	volatile uint32_t *reg;
	int i;

	for (i = 0; macRegisterDefaults[i].offset != 0xffff; i++) {
		reg = (volatile uint32_t *)((uintptr_t)registers + macRegisterDefaults[i].offset);
		*reg = macRegisterDefaults[i].value | (*reg & ~macRegisterDefaults[i].mask);
	}
	return i;
}

/* The time before a beacon the power-save code wakes at. */
uint32_t fmCoexComputeBuffer(void)
{
	uint32_t period = wlanLinkCheckPeriod();

	wlanPsWakeCycle = period - wlanPsWakeAdvance - sleepWakeLatency - 30;
	return wlanPsWakeCycle;
}

/* Starts the wake timer for the IBSS's ATIM window, if it has one. */
int fmCoexQueryBuffer(void)
{
	uint32_t delay = macMgmtDispatchDescriptor() << 10;

	if (delay == 0)
		return 0;
	return osaCreateTimer((OsaTimerCallback)wlanPsSelectTable, 0, delay, &wlanIbssAtimTimer, 8);
}

uint32_t fmCoexSyncBuffer(void)
{
	return macDefaultsTxControlSet;
}

void fmCoexApplyBuffer(int enable, uint32_t value)
{
	if (enable != 0) {
		MAC_DD4_TX_TIMING = 0x2a0;
		MAC_088_TX_CONTROL = fmCoexResolveBuffer();
		MAC_DD0_TX_VALUE = value;
		MAC_4D4_DESCRIPTOR |= 0x40;
		enable = 1;
	}
	macDefaultsTxControlSet = enable;
}

/*
 * The original builds the word in an uninitialised stack slot and leaves bit
 * 21 and the top byte as they were there; this build gives them 0.
 */
__attribute__((noinline)) uint32_t fmCoexResolveBuffer(void)
{
	union MacTxControl control;

	control.bits.enable = 1;
	control.bits.mode = 0;
	control.bits.rate = 0;
	control.bits.option = 0;
	control.bits.format = 1;
	control.bits.priority = 0;
	control.bits.last = 1;
	control.bits.antenna = wlanTxPowerRateClass(14);
	control.bits.flagA = 0;
	control.bits.flagB = 0;
	control.bits.flagC = 0;
	return control.word;
}

/* Gives the local station's state its defaults. */
int fmCoexScheduleBuffer(struct WlanLocalState *state)
{
	struct WlanDeviceConfig *device = &state->device;
	unsigned int i;

	wlanDeviceAddress = device;
	wlanLocalState = state;
	wlanLocalContext = state->context;
	state->mode = 0xff;
	state->sequence = 0;
	state->powerSave = 0;
	state->wepKeys[0].index = 1;
	__aeabi_memcpy(state->wepKeys[0].key, "ABCDEFGHIJKLM", 13);
	state->wepKeys[0].length = 0;
	state->wepKeys[1].index = 2;
	state->wepKeys[1].length = 0;
	__aeabi_memcpy(state->wepKeys[1].key, "NOPQRSTUVWXYZ", 13);
	state->wepKeys[2].index = 3;
	state->wepKeys[2].length = 0;
	__aeabi_memcpy4(state->wepKeys[2].key, "abcdefghijklm", 13);
	state->wepKeys[3].index = 4;
	state->wepKeys[3].length = 0;
	__aeabi_memcpy(state->wepKeys[3].key, "nopqrstuvwxyz", 13);
	state->keyFlags = 0;
	__aeabi_memclr4(state->keyAddress, 6);
	state->keyIndex = 0;
	__aeabi_memclr(state->keyData, 12);
	state->keyValid = 0;
	state->linkFlags[0] = 0;
	state->linkFlags[1] = 0;
	state->linkLimit = 10;
	state->linkState = 0;
	state->linkCounters[0] = 0;
	state->linkCounters[1] = 0;
	state->linkActive = 0;
	state->enabled = 1;
	device->parameters[1] = 96;
	device->parameters[0] = 512;
	for (i = 0; i < 4; i++)
		device->timeouts[i] = 500;
	device->rtsThreshold[0] = 2347 & 0xff;
	device->rtsThreshold[1] = 2347 >> 8;
	device->shortRetryLimit = 9;
	device->longRetryLimit = 4;
	device->fragThreshold[0] = 2346 & 0xff;
	device->fragThreshold[1] = 2346 >> 8;
	btSbcAcquireBuffer(state);
	return 1;
}

/* The channel list of the local context: each channel's number and its byte of register 0x8000a100. */
__attribute__((noinline)) void btSbcManageBuffer(struct WlanLocalState *state)
{
	uint8_t *channels = state->context;
	unsigned int i;

	for (i = 0; i < WLAN_CHANNELS; i++) {
		wlanChannelBytes[i] = MAC_CHANNEL_REGISTER(i);
		channels[2 * i + 1] = wlanChannelBytes[i];
		channels[2 * i] = i;
	}
}

/* The local station's rates, channels and address. */
int btSbcAcquireBuffer(struct WlanLocalState *state)
{
	struct WlanDeviceConfig *device = &state->device;

	state->rates[0] = 0x82;
	state->rates[1] = 0x84;
	state->rates[2] = 0x8b;
	state->rates[3] = 0x96;
	state->rates[4] = 12;
	state->rates[5] = 18;
	state->rates[6] = 24;
	state->rates[7] = 36;
	state->rates[8] = 48;
	state->rates[9] = 72;
	state->rates[10] = 96;
	state->rates[11] = 108;
	state->rates[12] = 0;
	state->rates[13] = 0;
	btSbcManageBuffer(state);
	if (MAC_ADDRESS_LOW == 0xffffffff || MAC_ADDRESS_LOW == 0) {
		*(BtWord *)device->address = WLAN_DEFAULT_ADDRESS_LOW;
		*(BtHalf *)&device->address[4] = WLAN_DEFAULT_ADDRESS_HIGH;
	} else {
		*(BtWord *)device->address = MAC_ADDRESS_LOW;
		*(BtHalf *)&device->address[4] = MAC_ADDRESS_HIGH;
	}
	return 1;
}
