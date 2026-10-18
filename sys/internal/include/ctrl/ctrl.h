/*
 * Imports of SceCtrl (os0:kd/bootimage/ctrl) absent from the vitasdk headers.
 */
#pragma once

#include <psp2kern/types.h>
#include <syscon/syscon.h>
#include <sysmem/sysroot.h>
#include <acmgr/acmgr.h>

int SceCtrlForDriver_FF9D4DD4(uint32_t mask,
		void (*callback)(uint32_t, uint32_t, void *), void *arg,
		uint32_t *buttons);

/* Notify the registered BD remote callbacks (SceCtrlForDriver NID 6E183B9B). */
int SceCtrlForDriver_6E183B9B(int notification);

/* name unknown: nonzero for a further privileged program class (ctrl's "privileged" test) */

/* name unknown: SceCtrl calls it with 0 at start-up */

/* the BD remote control's state as the HDMI driver reports it */
typedef struct SceHdmiBdRemoconState {
	unsigned int buttons;
	unsigned int state;
	unsigned int stateExt;
} SceHdmiBdRemoconState;

/* name unknown: registers the BD remote control callback (weak import in SceCtrl) */
int SceHdmiForDriver_05174BC2(int (*callback)(const SceHdmiBdRemoconState *state));

#include <psp2common/ctrl.h>

#include <ctrl/ctrl_sample.h>

/*
 * A virtual controller driver (SceCtrlForDriver_18823EEA): it
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

int SceCtrlForDriver_18823EEA(const SceCtrlVirtualControllerDriver *driver);
int _sceCtrlBdRemoconSendMergedNotify2(uint32_t buttons, uint32_t state,
				    int connected);
int SceCtrlForDriver_AA36F578(uint32_t connected);

int SceCtrlForDriver_03777131(int clearMask, int setMask);
int SceCtrlForDriver_06577FE8(unsigned int port, unsigned char slot, unsigned char userLx, unsigned char userLy, unsigned char userRx, unsigned char userRy, unsigned char kernelLx, unsigned char kernelLy, unsigned char kernelRx, unsigned char kernelRy, unsigned int duration);
int SceCtrlForDriver_06AA5E1A(int intercept);
int SceCtrlForDriver_0A679512(uint32_t key0, uint32_t key1, uint32_t key2, uint32_t key3);
int SceCtrlForDriver_12C13A72(int clearMask, int setMask);
int SceCtrlForDriver_145B13BC(int *mode);
int SceCtrlForDriver_19895843(int port, SceCtrlData *samples, int count);
int SceCtrlForDriver_1E750326(unsigned int port, unsigned char slot, unsigned int userButtons, unsigned int kernelButtons, unsigned int duration);
int SceCtrlForDriver_30801AE9(uint32_t *key0, uint32_t *key1, uint32_t *key2, uint32_t *key3);
int SceCtrlForDriver_345E74FD(int threshold0, int threshold1);
int SceCtrlForDriver_46ABDDE8(uint32_t *mask);
int SceCtrlForDriver_4CCB2153(unsigned int index);
int SceCtrlForDriver_4D8FE670(uint32_t enabled);
int SceCtrlForDriver_53381364(int port1, int port2);
int SceCtrlForDriver_5B708A15(SceUID pid, int mode);
int SceCtrlForDriver_5B849B22(void);
int SceCtrlForDriver_69E45836(void);
int SceCtrlForDriver_6BEB1CD7(int *intercept);
int SceCtrlForDriver_80F5E418(int mode);
int SceCtrlForDriver_8D4E0DD1(int port, SceCtrlData *samples, int count);
int SceCtrlForDriver_9B96A1AA(int port, SceCtrlData *samples, int count);
int SceCtrlForDriver_9DAB51C4(SceUID callback);
int SceCtrlForDriver_A248CDAE(int port, int index, const SceCtrlRapidFireRule *rule);
int SceCtrlForDriver_B25BD8FD(uint32_t *mask);
int SceCtrlForDriver_B26C2231(SceUID pid);
int SceCtrlForDriver_B9402CEF(int *threshold0, int *threshold1);
int SceCtrlForDriver_C8CE3E43(int enabled);
int SceCtrlForDriver_DA07E970(SceUID *pid);
int SceCtrlForDriver_E170883E(SceUID callback);
int SceCtrlForDriver_EA1D3A34(int port, SceCtrlData *samples, int count);
int SceCtrlForDriver_ED138A26(int port, int index);
int SceCtrlForDriver_ED8F0129(int port);
int SceCtrlForDriver_F11D0D30(SceCtrlPortInfo *info);
int SceCtrlForDriver_FA88D837(void);
int sceCtrlClearRapidFire(int port, int index);
int sceCtrlDisconnect(int port);
int sceCtrlGetAnalogStickCheckMode(uint32_t *mode);
int sceCtrlGetAnalogStickCheckTarget(uint32_t *target);
int sceCtrlGetBatteryInfo(int port, uint8_t *battery);
int sceCtrlGetButtonIntercept(int *intercept);
int sceCtrlGetControllerPortInfo(SceCtrlPortInfo *info);
int sceCtrlGetProcessStatus(SceUID pid, int *status);
int sceCtrlGetSamplingMode(int *mode);
int sceCtrlGetSamplingModeExt(int *mode);
int sceCtrlIsMultiControllerSupported(void);
int sceCtrlPeekBufferNegative(int port, SceCtrlData *samples, int count);
int sceCtrlPeekBufferNegative2(int port, SceCtrlData *samples, int count);
int sceCtrlPeekBufferPositive(int port, SceCtrlData *samples, int count);
int sceCtrlPeekBufferPositive2(int port, SceCtrlData *samples, int count);
int sceCtrlPeekBufferPositiveExt(int port, SceCtrlData *samples, int count);
int sceCtrlPeekBufferPositiveExt2(int port, SceCtrlData *samples, int count);
int sceCtrlReadBufferNegative(int port, SceCtrlData *samples, int count);
int sceCtrlReadBufferNegative2(int port, SceCtrlData *samples, int count);
int sceCtrlReadBufferPositive(int port, SceCtrlData *samples, int count);
int sceCtrlReadBufferPositive2(int port, SceCtrlData *samples, int count);
int sceCtrlReadBufferPositiveExt(int port, SceCtrlData *samples, int count);
int sceCtrlReadBufferPositiveExt2(int port, SceCtrlData *samples, int count);
int sceCtrlRegisterBdRMCCallback(SceUID callback);
int sceCtrlResetLightBar(int port);
int sceCtrlSetActuator(int port, const SceCtrlActuator *state);
int sceCtrlSetAnalogStickCheckMode(uint32_t mode);
int sceCtrlSetAnalogStickCheckTarget(uint32_t target);
int sceCtrlSetButtonIntercept(int intercept);
int sceCtrlSetCtrlpCallback(int (*callback)(SceUInt32 port, struct ctrlNativeSample *sample,
					    int mode));
int sceCtrlSetLightBar(int port, uint8_t red, uint8_t green, uint8_t blue);
int sceCtrlSetRapidFire(int port, int index, const SceCtrlRapidFireRule *rule);
int sceCtrlSetSamplingMode(int mode);
int sceCtrlSetSamplingModeExt(int mode);
int sceCtrlSingleControllerMode(int port);
int sceCtrlUnregisterBdRMCCallback(SceUID callback);
