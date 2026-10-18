#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int SceHidForDriver_050351F2(SceUID callbackId);
int SceHidForDriver_0B7D27CF(SceUID pid);
int SceHidForDriver_141CA100(SceUID callbackId);
int SceHidForDriver_18DE9BB0(int *enable);
int SceHidForDriver_21A23FCD(SceUID callbackId);
int SceHidForDriver_3EC4E469(SceUID callbackId);
int SceHidForDriver_41690428(SceUID callbackId);
int SceHidForDriver_42B43C21(SceUID callbackId);
int SceHidForDriver_480D225F(SceUID *handles, int maxCount);
int SceHidForDriver_63776265(SceUID callbackId);
int SceHidForDriver_69DEE02F(SceUID callbackId);
int SceHidForDriver_6FFC9302(SceUID callbackId);
int SceHidForDriver_710B353C(SceUID callbackId);
int SceHidForDriver_754E48DF(unsigned int enable);
int SceHidForDriver_7F444D9C(SceUID callbackId);
int SceHidForDriver_8DE66028(SceUID callbackId);
int SceHidForDriver_9AFBB275(SceUID callbackId);
int SceHidForDriver_A26B1E1C(SceUID *handle, int count);
int SceHidForDriver_A60E34F7(SceUID *handle, int count);
int SceHidForDriver_B0B22694(SceUID callbackId);
int SceHidForDriver_D658B068(SceUID callbackId);
int SceHidForDriver_EB323EC6(SceUID *pid);
int SceHidForDriver_F7470359(SceUID handle, void *reports, int count);
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
