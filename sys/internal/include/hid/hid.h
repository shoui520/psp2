#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <stdint.h>

/* vitasdk: ksceHidKeyboardUnregisterReadHintCallback */
int SceHidForDriver_050351F2(SceUID callbackId);
int SceHidForDriver_0B7D27CF(SceUID pid);
/* vitasdk: ksceHidMouseRegisterEnumHintCallback */
int SceHidForDriver_141CA100(SceUID callbackId);
int SceHidForDriver_18DE9BB0(int *enable);	/* vitasdk: ksceHidKeyboardGetIntercept */
/* vitasdk: ksceHidKeyboardRegisterEnumHintCallback */
int SceHidForDriver_21A23FCD(SceUID callbackId);
/* vitasdk: ksceHidConsumerControlUnregisterReadHintCallback */
int SceHidForDriver_3EC4E469(SceUID callbackId);
/* vitasdk: ksceHidMouseRegisterReadHintCallback */
int SceHidForDriver_41690428(SceUID callbackId);
/* vitasdk: ksceHidControllerUnregisterReadHintCallback */
int SceHidForDriver_42B43C21(SceUID callbackId);
/* vitasdk: ksceHidControllerEnumerate */
int SceHidForDriver_480D225F(SceUID *handles, int maxCount);
/* vitasdk: ksceHidMouseUnregisterReadHintCallback */
int SceHidForDriver_63776265(SceUID callbackId);
/* vitasdk: ksceHidKeyboardUnregisterEnumHintCallback */
int SceHidForDriver_69DEE02F(SceUID callbackId);
/* vitasdk: ksceHidConsumerControlUnregisterEnumHintCallback */
int SceHidForDriver_6FFC9302(SceUID callbackId);
/* vitasdk: ksceHidConsumerControlRegisterReadHintCallback */
int SceHidForDriver_710B353C(SceUID callbackId);
int SceHidForDriver_754E48DF(unsigned int enable);	/* vitasdk: ksceHidKeyboardSetIntercept */
/* vitasdk: ksceHidConsumerControlRegisterEnumHintCallback */
int SceHidForDriver_7F444D9C(SceUID callbackId);
/* vitasdk: ksceHidControllerRegisterEnumHintCallback */
int SceHidForDriver_8DE66028(SceUID callbackId);
/* vitasdk: ksceHidMouseUnregisterEnumHintCallback */
int SceHidForDriver_9AFBB275(SceUID callbackId);
int SceHidForDriver_A26B1E1C(SceUID *handle, int count);	/* vitasdk: ksceHidMouseEnumerate */
/* vitasdk: ksceHidConsumerControlEnumerate */
int SceHidForDriver_A60E34F7(SceUID *handle, int count);
/* vitasdk: ksceHidKeyboardRegisterReadHintCallback */
int SceHidForDriver_B0B22694(SceUID callbackId);
/* vitasdk: ksceHidControllerRegisterReadHintCallback */
int SceHidForDriver_D658B068(SceUID callbackId);
int SceHidForDriver_EB323EC6(SceUID *pid);
/* vitasdk: ksceHidControllerRead */
int SceHidForDriver_F7470359(SceUID handle, void *reports, int count);
/* vitasdk: ksceHidControllerUnregisterEnumHintCallback */
int SceHidForDriver_FC1C743A(SceUID callbackId);
int sceHidConsumerControlEnumerate(SceUID *handle, int count);
int sceHidConsumerControlRegisterEnumHintCallback(SceUID callbackId);
int sceHidConsumerControlRegisterReadHintCallback(SceUID callbackId);
int sceHidConsumerControlUnregisterEnumHintCallback(SceUID callbackId);
int sceHidConsumerControlUnregisterReadHintCallback(SceUID callbackId);
int sceHidControllerEnumerate(SceUID *handles, int maxCount);
int sceHidControllerRead(SceUID handle, void *reports, int count);
int sceHidControllerRegisterEnumHintCallback(SceUID callbackId);
int sceHidControllerRegisterReadHintCallback(SceUID callbackId);
int sceHidControllerUnregisterEnumHintCallback(SceUID callbackId);
int sceHidControllerUnregisterReadHintCallback(SceUID callbackId);
int sceHidKeyboardGetIntercept(int *enable);
int sceHidKeyboardRegisterEnumHintCallback(SceUID callbackId);
int sceHidKeyboardRegisterReadHintCallback(SceUID callbackId);
int sceHidKeyboardSetIntercept(unsigned int enable);
int sceHidKeyboardUnregisterEnumHintCallback(SceUID callbackId);
int sceHidKeyboardUnregisterReadHintCallback(SceUID callbackId);
int sceHidMouseEnumerate(SceUID *handle, int count);
int sceHidMouseRegisterEnumHintCallback(SceUID callbackId);
int sceHidMouseRegisterReadHintCallback(SceUID callbackId);
int sceHidMouseUnregisterEnumHintCallback(SceUID callbackId);
int sceHidMouseUnregisterReadHintCallback(SceUID callbackId);

int sceHidKeyboardClear(SceUID handle);
int sceHidKeyboardEnumerate(SceUID *handle, int count);

/* The input reports sceHidKeyboardRead/Peek, sceHidMouseRead and
 * sceHidConsumerControlRead return. A keyboard report's first byte is the
 * HID error usage the device sent (1-3: ErrorRollOver, POSTFail,
 * ErrorUndefined), 0 for a valid report. */
typedef struct SceHidKeyboardReport {
	SceUInt8 error;
	SceUInt8 modifiers[2];
	SceUInt8 keycodes[13];
	SceUInt64 timestamp;
} SceHidKeyboardReport;

typedef struct SceHidMouseReport {
	SceUInt8 buttons;
	SceUInt8 reserved;
	SceInt16 rel_x;
	SceInt16 rel_y;
	SceInt8 wheel;
	SceInt8 tilt;
	SceUInt64 timestamp;
} SceHidMouseReport;

typedef struct SceHidConsumerControlReport {
	SceUInt16 usages[4];
	SceUInt64 timestamp;
} SceHidConsumerControlReport;

#ifndef __cplusplus
_Static_assert(sizeof(SceHidKeyboardReport) == 24, "SceHidKeyboardReport");
_Static_assert(_Alignof(SceHidKeyboardReport) == 8, "HID keyboard alignment");
_Static_assert(offsetof(SceHidKeyboardReport, error) == 0, "HID keyboard error");
_Static_assert(offsetof(SceHidKeyboardReport, modifiers) == 1,
	       "HID keyboard modifiers");
_Static_assert(offsetof(SceHidKeyboardReport, keycodes) == 3,
	       "HID keyboard keycodes");
_Static_assert(offsetof(SceHidKeyboardReport, timestamp) == 16,
	       "HID keyboard timestamp");
_Static_assert(sizeof(SceHidMouseReport) == 16, "SceHidMouseReport");
_Static_assert(offsetof(SceHidMouseReport, timestamp) == 8,
	       "HID mouse timestamp");
_Static_assert(sizeof(SceHidConsumerControlReport) == 16,
	       "SceHidConsumerControlReport");
_Static_assert(offsetof(SceHidConsumerControlReport, timestamp) == 8,
	       "HID consumer-control timestamp");
#endif

int sceHidKeyboardPeek(SceUInt32 handle, SceHidKeyboardReport *reports, int nReports);
