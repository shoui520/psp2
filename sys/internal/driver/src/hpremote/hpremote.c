/* SceHpremote: debounced headphone/remote inputs and codec detection. */
#include <stdint.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/suspend.h>
#include <psp2kern/kernel/sysclib.h>
#include <hpremote/hpremote.h>
#include <sysmem/sysevent_ids.h>

/* Field and internal function names are unknown; names describe observed use. */
struct HpremoteState {
	int event, thread, state;
	int callbacks[32];
	uint8_t hardwareFlags, input, codecInput, rawInput;
	uint32_t inputHistory;
	uint8_t multiInput, rawMultiInput, multiType, reserved0;
	uint32_t multiHistory, reserved1;
	uint8_t changed;
	int8_t status[3];
	void (*handler)(int, int, int, int, int, void *);
	void *handlerArg;
};

static struct HpremoteState remote;

static void inputChanged(int value) /* name unknown */
{
	remote.rawInput = value;
}

static void multiInputChanged(int value) /* name unknown */
{
	remote.rawMultiInput = value;
}

static void codecInputChanged(int value) /* name unknown */
{
	remote.codecInput = value;
	ksceKernelClearEventFlag(remote.event, value ? ~0x40u : ~4u);
	ksceKernelSetEventFlag(remote.event, value ? 4 : 0x40);
}

static int pollInputs(void *arg, void *ctx, unsigned char priority) /* name unknown */
{
	uint32_t info;
	unsigned int input = remote.rawInput, multi;
	int bits, i;

	remote.inputHistory = (remote.inputHistory << 1) | (input & 1);
	if (((remote.inputHistory & 15) == 0 ||
	     (remote.inputHistory & 15) == 15 ||
	     ((remote.hardwareFlags & 4) && input == 0)) &&
	    remote.input != input) {
		remote.input = input;
		remote.status[0] = input ? 1 : 0;
		remote.status[1] = remote.status[2] = input ? -1 : 0;
		remote.changed = 1;
		ksceKernelClearEventFlag(remote.event, input ? ~2u : ~1u);
		ksceKernelSetEventFlag(remote.event, input ? 1 : 2);
	}
	multi = remote.rawMultiInput;
	remote.multiHistory = (remote.multiHistory << 1) | (multi & 1);
	if (((remote.multiHistory & 15) == 0 ||
	     (remote.multiHistory & 15) == 15) && remote.multiInput != multi) {
		remote.multiInput = multi;
		remote.changed = 1;
	}
	ksceSysconGetMultiCnInfo(&info);
	remote.multiType = (info & 0xff00) == 0x300;
	if (remote.changed) {
		remote.changed = 0;
		if (remote.handler)
			remote.handler(remote.input, remote.multiType,
				       remote.status[0], remote.status[1],
				       remote.status[2], remote.handlerArg);
		bits = (remote.status[0] == 1) | ((remote.status[1] == 1) << 1) |
		       ((remote.status[2] == 1) << 2);
		for (i = 0; i < 32; i++)
			if (remote.callbacks[i] >= 0)
				ksceKernelNotifyCallback(remote.callbacks[i], bits);
	}
	return -1;
}

static int remoteThread(SceSize args, void *argp) /* name unknown */
{
	uint32_t timeout, events, mask;
	int ret;

	if (remote.hardwareFlags & 2)
		SceSysconForDriver_5CDDA14D(0);
	ret = SceSysconForDriver_142D5E82();
	remote.state = ret ? 6 : 4;
	timeout = ret ? 1000000 : ~0u;
	mask = ret ? 0 : 1;
	for (;;) {
		events = 0;
		if (!timeout)
			ksceKernelPollEventFlag(remote.event, 0xc0000008,
						SCE_KERNEL_EVF_WAITMODE_OR, &events);
		else
			ksceKernelWaitEventFlag(remote.event, mask | 0xc0000008,
					       SCE_KERNEL_EVF_WAITMODE_OR, &events,
					       (int32_t)timeout < 0 ? NULL : &timeout);
		if (events & 0x40000000) {
			remote.state = 0;
			ksceKernelClearEventFlag(remote.event, ~0x40000000u);
		} else if (events & 8) {
			if (SceSysconForDriver_142D5E82()) {
				remote.state = 5;
			} else {
				remote.state = 4;
				ksceKernelClearEventFlag(remote.event, ~8u);
			}
		} else if (remote.state > 4 &&
			   ((events & 2) || (remote.state > 11 && (events & 0x40 & mask)))) {
			remote.state = 2;
		}
		if (events & 0x80000000) {
			if (SceSysconForDriver_C3504ADE() == 1 && (remote.hardwareFlags & 2))
				SceSysconForDriver_5CDDA14D(0);
			return 0;
		}
		timeout = 0;
		switch (remote.state) {
		case 0:
		case 1:
		case 2:
			SceCodecForDriver_E483B663(0);
			if (remote.hardwareFlags & 2)
				SceSysconForDriver_5CDDA14D(0);
			if (remote.state == 0) {
				remote.state = 3;
				mask = 0;
				timeout = ~0u;
			} else if (remote.state == 1) {
				remote.state = 3;
				mask = 2;
			} else {
				remote.state = 4;
				mask = 1;
				timeout = ~0u;
			}
			break;
		case 3:
			if (events & 2) {
				remote.state = 4;
				mask = 1;
			} else {
				mask = 2;
			}
			timeout = ~0u;
			break;
		case 4:
			if (events & 1)
				remote.state = 6;
			else {
				mask = 1;
				timeout = ~0u;
			}
			break;
		case 5:
			ksceKernelClearEventFlag(remote.event, ~8u);
			mask = 0;
			if (remote.hardwareFlags & 2) {
				SceSysconForDriver_5CDDA14D(0);
				remote.state = 6;
				timeout = 1000000;
			} else {
				remote.state = 8;
			}
			break;
		case 6:
			if (remote.hardwareFlags & 2) {
				if (SceSysconForDriver_5CDDA14D(1) < 0) {
					timeout = 1000000;
					mask = 0;
				} else {
					remote.state = 7;
					timeout = 2000000;
					mask = 4;
				}
			} else {
				remote.state = 8;
				mask = 0;
				if (remote.hardwareFlags & 4)
					timeout = 50000;
			}
			break;
		case 7:
			if (!(events & 4)) {
				remote.state = 5;
				break;
			}
			/* fall through */
		case 8:
			if (SceCodecForDriver_E483B663(1) < 0) {
				remote.status[2] = 0;
				remote.state = 12;
				remote.changed = 1;
				if (remote.hardwareFlags & 2)
					SceSysconForDriver_5CDDA14D(0);
				mask = 2;
			} else {
				remote.state = 9;
				timeout = 4000;
				mask = (remote.hardwareFlags & 2) ? 0x42 : 2;
			}
			break;
		case 9:
			if (!SceCodecForDriver_CF398D8A()) {
				remote.status[2] = 1;
				remote.changed = 1;
				remote.state = 12;
				mask = (remote.hardwareFlags & 2) ? 0x42 : 2;
				break;
			}
			remote.state = 10;
			timeout = 3000000;
			if ((remote.hardwareFlags & 2) && !(events & 4)) {
				if (SceSysconForDriver_5CDDA14D(1) < 0) {
					remote.status[2] = 0;
					remote.changed = 1;
					SceCodecForDriver_E483B663(0);
					SceSysconForDriver_5CDDA14D(0);
					remote.state = 12;
					mask = 2;
				} else {
					timeout = 2000000;
					mask = 4;
				}
				break;
			}
			mask = (remote.hardwareFlags & 2) ? 0x42 : 2;
			if (!(remote.hardwareFlags & 1)) {
				remote.status[2] = 0;
				remote.changed = 1;
				SceCodecForDriver_E483B663(0);
				remote.state = 12;
				mask = 2;
			}
			break;
		case 10:
			if ((remote.hardwareFlags & 2) && !(events & 4)) {
				remote.state = 5;
				break;
			}
			if (SceCodecForDriver_E483B663(1) < 0) {
				remote.state = 12;
				remote.status[2] = 0;
				SceSysconForDriver_5CDDA14D(0);
				mask = 2;
			} else {
				remote.state = 11;
				timeout = 4000;
				mask = (remote.hardwareFlags & 2) ? 0x42 : 2;
			}
			break;
		case 11:
			if (!SceCodecForDriver_CF398D8A()) {
				remote.status[2] = 1;
				remote.changed = 1;
				mask = (remote.hardwareFlags & 2) ? 0x42 : 2;
			} else {
				remote.status[2] = 0;
				remote.changed = 1;
				SceCodecForDriver_E483B663(0);
				SceSysconForDriver_5CDDA14D(0);
				mask = 2;
			}
			remote.state = 12;
			break;
		case 12:
			timeout = ~0u;
			break;
		}
	}
}

static int sysEvent(int resume, int event, void *args, void *opt) /* name unknown */
{
	if (resume != 1 || event != SYSEVENT_RESUME_DEVICE(12))
		return 0;
	ksceKernelSetEventFlag(remote.event, 8);
	codecInputChanged(0);
	remote.input = SceSysconForDriver_142D5E82();
	remote.rawInput = remote.input;
	remote.inputHistory = remote.input ? ~0u : 0;
	ksceKernelClearEventFlag(remote.event, remote.input ? ~2u : ~1u);
	ksceKernelSetEventFlag(remote.event, remote.input ? 1 : 2);
	remote.rawMultiInput = SceSysconForDriver_ACC7F71E();
	remote.multiInput = !remote.rawMultiInput;
	remote.multiHistory = remote.multiInput ? ~0u : 0;
	remote.status[0] = remote.input == 1;
	remote.status[1] = remote.status[2] = remote.input == 1 ? -1 : 0;
	remote.changed = 1;
	return 0;
}

int module_start(SceSize args, void *argp)
{
	uint32_t info;
	int uid;

	memset(&remote, 0, sizeof(remote));
	memset(remote.callbacks, -1, sizeof(remote.callbacks));
	uid = ksceKernelCreateEventFlag("SceHprm", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (uid < 0)
		return 0;
	remote.event = uid;
	remote.input = SceSysconForDriver_142D5E82();
	remote.rawInput = remote.input;
	remote.inputHistory = remote.input ? ~0u : 0;
	ksceKernelSetEventFlag(remote.event, remote.input == 1 ? 1 : 2);
	remote.status[0] = remote.input == 1;
	remote.status[1] = remote.status[2] = remote.input == 1 ? -1 : 0;
	remote.changed = 1;
	info = ksceSysconGetHardwareInfo();
	if ((info & 0xff0000) < 0x800000) {
		remote.hardwareFlags = 3;
	} else if ((ksceSysconGetHardwareInfo() & 0xff0000) >= 0x820000) {
		remote.hardwareFlags = 5;
	}
	remote.codecInput = SceSysconForDriver_C3504ADE();
	ksceKernelSetEventFlag(remote.event, remote.codecInput == 1 ? 4 : 0x40);
	remote.multiInput = SceSysconForDriver_ACC7F71E();
	remote.rawMultiInput = remote.multiInput;
	remote.multiHistory = remote.multiInput ? ~0u : 0;
	if (remote.multiInput) {
		ksceSysconGetMultiCnInfo(&info);
		remote.multiType = (info & 0xff00) == 0x300;
	}
	SceSysconForDriver_63352A39(inputChanged, NULL);
	SceSysconForDriver_DE613081(codecInputChanged, NULL);
	SceSysconForDriver_35E1689F(multiInputChanged, NULL);
	uid = ksceKernelCreateThread("SceHpRemote", remoteThread, 0x40, 0x1000, 0, 0x80000, NULL);
	if (uid < 0)
		return 0;
	remote.thread = uid;
	ksceKernelStartThread(uid, 0, NULL);
	ksceKernelRegisterSubIntrHandler(8, 24, "SceHprmVblank", pollInputs, NULL);
	ksceKernelEnableSubIntr(8, 24);
	sceKernelRegisterSysEventHandler("SceHprm", sysEvent, NULL);
	return 0;
}

int SceHpremoteForDriver_D22913DB(void (*handler)(int, int, int, int, int, void *), void *arg)
{
	int intr = SceCpuForDriver_821FC0EE();

	remote.handler = handler;
	remote.handlerArg = arg;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

int SceHpremoteForDriver_C7154136(int callback)
{
	int intr = SceCpuForDriver_821FC0EE(), ret = -1, i;

	for (i = 0; i < 16; i++) {
		if (remote.callbacks[i] < 0) {
			remote.callbacks[i] = callback;
			ret = 0;
			break;
		}
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

int SceHpremoteForDriver_68D64A0F(int callback)
{
	int intr = SceCpuForDriver_821FC0EE(), ret = -1, i;

	for (i = 0; i < 16; i++) {
		if (remote.callbacks[i] == callback) {
			remote.callbacks[i] = -1;
			ret = 0;
			break;
		}
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

int SceHpremoteForDriver_71B5FB67(void)
{
	return remote.input;
}

int SceHpremoteForDriver_2229EF51(void)
{
	return remote.multiType;
}

int SceHpremoteForDriver_7E69EDA4(void)
{
	return remote.status[0] < 0 ? 0 : remote.status[0];
}

int SceHpremoteForDriver_208DB1BD(void)
{
	return remote.status[1] < 0 ? 0 : remote.status[1];
}

int SceHpremoteForDriver_219C58F1(void)
{
	return remote.status[2] < 0 ? 0 : remote.status[2];
}

int SceHpremoteForDriver_7735C403(void)
{
	ksceKernelSetEventFlag(remote.event, 0x40000000);
	return 0;
}

int SceHpremoteForDriver_4D1E622C(void)
{
	ksceKernelSetEventFlag(remote.event, 8);
	return 0;
}

int SceHpremoteForDriver_7B038374(void **state)
{
	if (state)
		*state = &remote;
	return remote.state;
}
