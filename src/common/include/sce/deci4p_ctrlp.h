#ifndef SCE_DECI4P_CTRLP_H
#define SCE_DECI4P_CTRLP_H
#include <psp2kern/types.h>
#include <psp2/touch.h>
#include "sce/ctrl_sample.h"
#include "sce/bt_pairing.h"

/* SceCtrl and SceTouch hand every sample to SceDeci4pCtrlp before the program sees it */
int ksceCtrlSetCtrlpCallback(int (*callback)(SceUInt32 port, struct ctrlNativeSample *sample));
int ksceTouchSetCtrlpCallback(int (*callback)(SceUInt32 panel, SceTouchData *sample));
int ksceTouchGetPanelInfo(SceUInt32 panel, SceTouchPanelInfo *info);
SceInt64 sceKernelGetSystemTimeWide(void);

#endif
