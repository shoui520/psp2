#ifndef CTRL_INTERNAL_H
#define CTRL_INTERNAL_H

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
#include <psp2common/ctrl.h>
#include <sysmem/cpu.h>
#include <syscon/syscon.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/threadmgr.h>
#include <ctrl/ctrl.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/suspend.h>

SceUInt32 ctrlAlarmCallback(void *arg);
int ctrlSampleComplete(SceSysconPacket *packet, void *arg);

struct ctrlAnalogStickCheck {
	uint64_t timestamp;
	uint8_t triggered;
	uint8_t padding[7];
};

struct ctrlPortEvents {
	uint32_t active;
	uint32_t blocked;
	SceUID positive;
	SceUID negative;
};

struct ctrlRapidFireEntry {
	SceUID owner;
	uint32_t mask;
	uint32_t trigger;
	uint32_t target;
	uint8_t counter;
	uint8_t make;
	uint8_t breakCount;
	uint8_t delay;
};

struct ctrlAccumulatedAxis {
	uint32_t analog;
	uint32_t pressure;
	uint32_t extended;
};

struct ctrlEmulation {
	uint8_t userLx;
	uint8_t userLy;
	uint8_t userRx;
	uint8_t userRy;
	uint8_t kernelLx;
	uint8_t kernelLy;
	uint8_t kernelRx;
	uint8_t kernelRy;
	uint32_t analogDuration;
	uint32_t userButtons;
	uint32_t kernelButtons;
	uint32_t buttonDuration;
};

struct ctrlUserBuffer {
	SceUID mutex;
	uint32_t reserved;
	SceCtrlData samples[64];
};

struct ctrlNativeSample;

struct ctrlWirelessControllerInfo {
	uint8_t wireless[4];
	uint8_t reserved[12];
};

struct ctrlButtonRemappingInfo {
	uint8_t buttons[14];
	uint8_t reserved;
	uint8_t swapAnalogSticks;
};

struct ctrlButtonChangeCallback {
	uint32_t mask;
	void (*callback)(uint32_t buttons, uint32_t previous, void *arg);
	void *arg;
};

struct ctrlStorage {
	uint32_t bdRemoconNotify;
	uint32_t bdRemoconPendingNotify;
	uint32_t reserved0[2];
	SceKernelSpinlock lock;
	struct ctrlPortEvents ports[5];
	uint8_t powerTickPending;
	uint8_t vblankSampling;
	int16_t systemEventState;
	uint32_t samplingMode;
	uint32_t requestedSamplingMode;
	union {
		uint32_t pendingState;
		struct {
			uint8_t samplePending;
			uint8_t samplingModePending;
			uint8_t padding0[2];
		};
	};
	SceSysconPacket samplePacket;
	SceSysconPacket samplingModePacket;
	uint32_t processCount;
	SceUID processes[16];
	struct ctrlRapidFireEntry rapidFire[5][16];
	struct ctrlEmulation emulation[5][4];
	uint32_t cachedButtons[5];
	uint32_t notificationButtons;
	uint8_t cachedLx;
	uint8_t cachedLy;
	uint8_t cachedRx;
	uint8_t cachedRy;
	uint8_t cachedUp;
	uint8_t cachedRight;
	uint8_t cachedDown;
	uint8_t cachedLeft;
	uint32_t maskForNonShell;
	uint32_t maskForAll;
	uint32_t idleCancelMask;
	uint32_t idleCancelKeys[4];
	int idleCancelThresholds[2];
	struct ctrlButtonChangeCallback buttonChangeCallbacks[4];
	uint32_t foregroundSelected;
	uint32_t activeProcessChangePending;
	SceUID activeProcess;
	uint32_t pendingActiveProcess;
	uint32_t baryonVersion;
	SceUID samplingTimer;
	SceUID systemEvents;
	SceUID processEvents;
	SceUID sampleHeap;
	uint32_t processStorageKey;
	uint32_t vibrationEnabled;
	uint32_t reserved1;
	uint32_t ctrlpMode;
	uint32_t ctrlpProcess;
	uint32_t accumulatedButtons;
	struct ctrlAccumulatedAxis accumulatedAxes[4];
	uint32_t otherPortButtons;
	void (*ctrlpCallback)(int port, struct ctrlNativeSample *sample, int mode);
	const SceCtrlVirtualControllerDriver *virtualDriver;
	uint32_t hdmiRemoconAvailable;
	int32_t bdRemoconCallbacks[16];
	int32_t bdRemoconCallbackOwners[16];
	SceUID remoconReleaseCallbacks[16];
	SceUID remoconReleaseOwners[16];
	uint32_t remoconReleaseStates[16];
	uint32_t bdRemoconButtons;
	uint32_t bdRemoconConnected;
	uint32_t updateMode;
	uint32_t safeMode;
	uint32_t analogStickCheckTarget;
	uint32_t analogStickCheckMode;
	struct ctrlAnalogStickCheck analogStickChecks[4];
	struct ctrlButtonRemappingInfo remapping;
	uint8_t buttonRemapEnabled;
	uint8_t padding1[7];
	struct ctrlUserBuffer userBuffers[2];
};

extern struct ctrlStorage ctrlStorage;

_Static_assert(offsetof(struct ctrlStorage, lock) == 0x10, "controller lock ABI");
_Static_assert(offsetof(struct ctrlStorage, samplingMode) == 0x68,
	       "sampling mode ABI");
_Static_assert(offsetof(struct ctrlStorage, maskForNonShell) == 0x9f8,
	       "controller mask ABI");
_Static_assert(sizeof(struct ctrlStorage) == 0x1c88, "controller storage ABI");
_Static_assert(offsetof(struct ctrlStorage, userBuffers) == 0xc78, "user buffers ABI");
_Static_assert(sizeof(struct ctrlUserBuffer) == 0x808, "user buffer ABI");

struct ctrlSampleRing {
	uint32_t readIndex;
	uint32_t readIndexExt;
	uint32_t writeIndex;
	uint32_t reserved;
	struct ctrlNativeSample samples[64];
};

struct ctrlProcessPort {
	uint32_t eventBit;
	struct ctrlSampleRing *ring;
};

/* A process's controller state (process local storage, 0x58 bytes). */
struct ctrlProcessStorage {
	uint32_t processId;
	uint32_t systemProgram;
	uint32_t samplingMode;
	uint32_t samplingModeExt;
	uint32_t processState;
	uint32_t privileged;
	uint32_t activation;
	uint32_t specialAuthority;
	uint32_t mode3Blocked;
	uint32_t sdkVersion;
	uint32_t buttonInterceptPending;
	uint8_t buttonIntercept;
	uint8_t pendingButtonIntercept;
	uint8_t padding[2];
	struct ctrlProcessPort ports[5];
};

_Static_assert(sizeof(SceCtrlData) == 0x20, "SDK sample ABI");
_Static_assert(sizeof(struct ctrlNativeSample) == 0x28, "native sample ABI");
_Static_assert(sizeof(struct ctrlSampleRing) == 0xa10, "native ring ABI");
_Static_assert(offsetof(struct ctrlProcessStorage, ports) == 0x30,
	       "process port ABI");
_Static_assert(sizeof(struct ctrlProcessStorage) == 0x58, "process storage ABI");
#endif
#endif
