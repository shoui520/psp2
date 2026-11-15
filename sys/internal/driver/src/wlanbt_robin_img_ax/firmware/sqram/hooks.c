/*
 * RAM hooks the ROM calls (SQRAM 0xc000e21c..0xc000e5a8): the lock the
 * WAPI crypto test holds, and the Bluetooth/FM audio path.
 *
 * The audio path is a table of {kind, mode} pairs the host sets with the
 * vendor command 42; when a kind is enabled, its mode decides which of the
 * SoC's routing and clock bits are set. Kind 3 is the FM radio. No Marvell
 * source names these functions or registers: the filename and the names
 * describe what the code does.
 */
#include <stdint.h>
#include "robin/eventpower.h"
#include "robin/fm.h"
#include "robin/hostcmd.h"
#include "robin/mdev.h"
#include "robin/osa.h"
#include "robin/soc.h"
#include "robin/sync.h"
#include "robin/vendor.h"

/* What the host passes for a mode to turn a kind off. */
#define BT_AUDIO_PATH_MODE_OFF	0xff

/* The SoC registers the audio path changes (offsets from 0x80002000). */
#define BT_AUDIO_PATH_CONTROL		SOC_REG(0x0c)
#define BT_AUDIO_PATH_FILTER		SOC_EVENT_ENGINE_GATE
#define BT_AUDIO_PATH_ROUTE		SOC_REG(0x44)
#define BT_AUDIO_PATH_INTERFACE		SOC_REG(0x48)
#define BT_AUDIO_PATH_CLOCK_A		SOC_REG(0x9c)
#define BT_AUDIO_PATH_CLOCK_B		SOC_REG(0x11c)
#define BT_AUDIO_PATH_DATA		SOC_REG(0x188)
#define BT_AUDIO_PATH_SELECT		SOC_REG(0x18c)
#define BT_AUDIO_PATH_OUTPUT		SOC_REG(0x204)
#define BT_AUDIO_PATH_PERIPHERAL	PERIPH_REG(0xc080)

/* The modes the host set, one per kind (0x5a988). */
struct BtAudioPathModes {
	uint16_t count;
	struct {
		uint8_t kind;
		uint8_t mode;
	} entries[7];
};

extern struct OsaSemaphore *macTxNotifySemaphore;	/* 0x54bd8 */
extern struct OsaBufferPool *ramHook_cryptoTestPool;	/* 0x548ac */
extern uint32_t ramHook_cryptoTestRounds;		/* 0x548b0 */
extern int (*ramHook_cryptoTestEnter)(void);		/* 0x548b4 */
extern void (*ramHook_cryptoTestLeave)(int lock);	/* 0x548b8 */

static struct BtAudioPathModes btAudioPathModes __attribute__((section(".itcm.bss")));	/* 0x5a988 */

/* The pool the crypto test takes its buffers from, in the ROM's data. */
#define CRYPTO_TEST_POOL	((struct OsaBufferPool *)0x04000e84)

/* Takes the semaphore the host commands hold for the crypto test. */
int cryptoTestEnter(void)
{
	osaSemaphoreWait(macTxNotifySemaphore, 0xffffffff);
	return 0;
}

void cryptoTestLeave(int lock)
{
	osaSemaphoreRelease(macTxNotifySemaphore);
}

/* Sets what the WAPI crypto test of HostCmd 0x0078 runs with. */
void hostCmd0078HooksInit(void)
{
	ramHook_cryptoTestEnter = cryptoTestEnter;
	ramHook_cryptoTestLeave = cryptoTestLeave;
	ramHook_cryptoTestPool = CRYPTO_TEST_POOL;
	ramHook_cryptoTestRounds = 128;
}

void btAudioPathModesClear(void)
{
	btAudioPathModes.count = 0;
}

/* The preset hook: applies the mode when the path is enabled, else restores the registers. */
void btAudioPathPresetHook(int enable, unsigned int mode)
{
	if (enable)
		btAudioPathPresetApply(mode);
	else
		sysPathPresetRestore();
}

/* The clock hook: modes 2 and 3 switch the audio clock on. */
void btAudioPathClockHook(int enable, unsigned int mode)
{
	if (!enable)
		return;
	switch (mode & 3) {
	case 0:
	case 1:
		return;
	case 2:
	case 3:
		break;
	}
	if (BT_AUDIO_PATH_CLOCK_B & 0x8000000)
		BT_AUDIO_PATH_CLOCK_B |= 0x4000000;
	else
		BT_AUDIO_PATH_CLOCK_A &= 0xbfdbfffe;
	BT_AUDIO_PATH_CONTROL |= 0x20000000;
}

/* Routes a kind's audio as the mode the host set for it says, or takes it off the path. */
void btAudioPathEnable(int enable, unsigned int kind)
{
	unsigned int mode = 0;
	unsigned int i;

	for (i = 0; i < btAudioPathModes.count; i = (uint8_t)(i + 1))
		if (btAudioPathModes.entries[i].kind == kind)
			mode = btAudioPathModes.entries[i].mode;

	switch (kind) {
	case 0:
		if (mode == 1) {
			if (enable)
				BT_AUDIO_PATH_OUTPUT &= ~0x2000;
			else
				BT_AUDIO_PATH_OUTPUT |= 0x2000;
		} else if (mode == 2) {
			if (enable) {
				BT_AUDIO_PATH_OUTPUT &= ~0x1000;
				BT_AUDIO_PATH_SELECT &= ~1;
			} else {
				BT_AUDIO_PATH_OUTPUT |= 0x1000;
				BT_AUDIO_PATH_SELECT |= 1;
			}
		} else {
			BT_AUDIO_PATH_INTERFACE |= 0x22;
			BT_AUDIO_PATH_ROUTE &= ~0x20000000;
			BT_AUDIO_PATH_FILTER &= ~0x1000;
			BT_AUDIO_PATH_SELECT |= 2;
		}
		break;
	case 1:
	case 2:
		if (mode == 1) {
			if (enable)
				BT_AUDIO_PATH_OUTPUT &= ~0x2000;
			else
				BT_AUDIO_PATH_OUTPUT |= 0x2000;
		} else if (mode == 2) {
			BT_AUDIO_PATH_SELECT |= 1;
			if (enable)
				BT_AUDIO_PATH_OUTPUT &= ~0x1000;
			else
				BT_AUDIO_PATH_OUTPUT |= 0x1000;
		} else {
			BT_AUDIO_PATH_INTERFACE &= ~0x22;
			BT_AUDIO_PATH_FILTER |= 0x1000;
			BT_AUDIO_PATH_SELECT &= ~2;
			BT_AUDIO_PATH_DATA &= ~0x100000;
		}
		break;
	case 3:
		if (mode == 1) {
			if (enable)
				BT_AUDIO_PATH_OUTPUT &= ~0x2000;
			else
				BT_AUDIO_PATH_OUTPUT |= 0x2000;
		} else if (mode == 2) {
			BT_AUDIO_PATH_SELECT |= 1;
			if (enable)
				BT_AUDIO_PATH_OUTPUT &= ~0x1000;
			else
				BT_AUDIO_PATH_OUTPUT |= 0x1000;
			BT_AUDIO_PATH_ROUTE |= 0x300000;
			BT_AUDIO_PATH_INTERFACE |= 0x40;
			if ((BT_AUDIO_PATH_INTERFACE >> 18 & 7) == 3)
				BT_AUDIO_PATH_INTERFACE &= ~(7u << 18);
			if ((BT_AUDIO_PATH_INTERFACE >> 25 & 3) == 1)
				BT_AUDIO_PATH_INTERFACE &= ~(3u << 25);
			BT_AUDIO_PATH_SELECT |= 1;
			BT_AUDIO_PATH_SELECT &= ~2;
			BT_AUDIO_PATH_DATA |= 0x100000;
		} else {
			BT_AUDIO_PATH_SELECT &= ~3;
			BT_AUDIO_PATH_DATA |= 0x100000;
			BT_AUDIO_PATH_FILTER &= ~0x1000;
			BT_AUDIO_PATH_INTERFACE &= ~2;
		}
		break;
	}
}

/* Puts the audio path's functions in the ROM's hook slots. */
void btAudioPathHooksInit(void)
{
	btFmEnableHook = btAudioPathEnable;
	btAudioClockHook = btAudioPathClockHook;
	btAudioPresetHook = btAudioPathPresetHook;
}

/*
 * Vendor command 42: {kind, mode} pairs, at most 6. A mode of 0xff takes the
 * kind off the path; any other replaces the kind's mode, or adds the kind.
 */
uint8_t btAudioPathModesSet(unsigned int length, const uint8_t *parameters)
{
	unsigned int count;
	unsigned int pair;
	unsigned int found;
	unsigned int i;
	unsigned int kind;
	unsigned int mode;

	if ((length & 1) != 0 || length > 12)
		return BT_STATUS_INVALID;
	count = length >> 1;
	for (pair = 0; pair < count; pair = (uint8_t)(pair + 1)) {
		kind = *parameters++;
		mode = *parameters++;
		if (mode == BT_AUDIO_PATH_MODE_OFF) {
			btAudioPathEnable(0, kind);
			continue;
		}
		found = 0;
		for (i = 0; i < btAudioPathModes.count; i = (uint8_t)(i + 1)) {
			if (btAudioPathModes.entries[i].kind == kind) {
				found = 1;
				btAudioPathModes.entries[i].mode = mode;
			}
		}
		if (!found) {
			btAudioPathModes.entries[btAudioPathModes.count].kind = kind;
			btAudioPathModes.entries[btAudioPathModes.count].mode = mode;
			btAudioPathModes.count++;
		}
	}
	return 0;
}

/* Sets the audio data line's polarity bit: clear when set, set when clear. */
void btAudioPathDataInvert(unsigned int clear)
{
	if (clear)
		BT_AUDIO_PATH_DATA &= ~0x4000;
	else
		BT_AUDIO_PATH_DATA |= 0x4000;
}

/* Applies a mode to the audio path, saving the registers the presets touch first. */
void btAudioPathPresetApply(unsigned int mode)
{
	BT_AUDIO_PATH_CONTROL |= 0x40000000;
	BT_AUDIO_PATH_DATA |= 0x20;
	BT_AUDIO_PATH_DATA &= ~0x200;
	BT_AUDIO_PATH_FILTER |= 0x1000;
	if (mode & 1) {
		BT_AUDIO_PATH_DATA &= ~0x80;
		BT_AUDIO_PATH_DATA &= ~0x4000;
	} else {
		BT_AUDIO_PATH_DATA &= ~0x80;
		BT_AUDIO_PATH_DATA |= 0x4000;
	}
	if (mode & 0x80) {
		sysPathPresetSave();
		BT_AUDIO_PATH_DATA |= 0x200280;
		BT_AUDIO_PATH_PERIPHERAL &= ~0x20000;
		BT_AUDIO_PATH_PERIPHERAL |= 0x10000;
	} else {
		sysPathPresetSave();
	}
}
