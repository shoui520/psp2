/*
 * Imports of SceCtrl (os0:kd/bootimage/ctrl) absent from the vitasdk headers.
 */
#pragma once

#include <psp2kern/types.h>

int SceCtrlForDriver_FF9D4DD4(uint32_t mask,
		void (*callback)(uint32_t, uint32_t, void *), void *arg,
		uint32_t *buttons);

/* Notify the registered BD remote callbacks (SceCtrlForDriver NID 6E183B9B). */
int ksceCtrlNotifyCallBacks(int notification);

/* nonzero when the model has the capability (1: multiple controller ports) */
int ksceKernelSysrootCheckModelCapability(unsigned int capability);

/* name unknown: nonzero for a further privileged program class (ctrl's "privileged" test) */
int SceSblACMgrForDriver_2E992B02(SceUID pid);

/* name unknown: SceCtrl calls it with 0 at start-up */
int SceSysconForDriver_901D6CD4(int arg);

/* the BD remote control's state as the HDMI driver reports it */
typedef struct SceHdmiBdRemoconState {
	unsigned int buttons;
	unsigned int state;
	unsigned int stateExt;
} SceHdmiBdRemoconState;

/* name unknown: registers the BD remote control callback (weak import in SceCtrl) */
int SceHdmiForDriver_05174BC2(int (*callback)(const SceHdmiBdRemoconState *state));

#include <psp2common/ctrl.h>

#include <sce/ctrl_sample.h>

/*
 * A virtual controller driver (ksceCtrlRegisterVirtualControllerDriver): it
 * takes the place of the built-in pads. The original read callback consumes
 * one native sample; the two entries vitasdk leaves unnamed follow their callers.
 */
typedef struct SceCtrlVirtualControllerDriver {
	int (*readButtons)(int port, struct ctrlNativeSample *sample);
	int (*setActuator)(int port, const SceCtrlActuator *pState);
	int (*getBatteryInfo)(int port, SceUInt8 *batt);
	int (*disconnect)(int port);
	int (*setTurnOffInterval)(int port);
	int (*getActiveControllerPort)(void);
	int (*changePortAssign)(int port1, int port2);
	int (*displayOff)(void);		/* SceCtrlForDriver_69E45836: power turned the display off */
	int (*getControllerPortInfo)(SceCtrlPortInfo *info);
	int (*setLightBar)(int port, SceUInt8 r, SceUInt8 g, SceUInt8 b);
	int (*resetLightBar)(int port);
	int (*foregroundChanged)(SceUID pid);	/* SceCtrlForDriver_B26C2231: a new foreground process */
	int (*singleControllerMode)(int port);
} SceCtrlVirtualControllerDriver;

_Static_assert(sizeof(SceCtrlVirtualControllerDriver) == 0x34, "SceCtrlVirtualControllerDriver");

int ksceCtrlRegisterVirtualControllerDriver(const SceCtrlVirtualControllerDriver *driver);
int _sceCtrlBdRemoconSendMergedNotify2(uint32_t buttons, uint32_t state,
				    int connected);
int ksceCtrlSetBdRemoconConnected(uint32_t connected);
