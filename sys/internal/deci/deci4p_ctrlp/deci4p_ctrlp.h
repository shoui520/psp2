#ifndef SCE_DECI4P_CTRLP_H
#define SCE_DECI4P_CTRLP_H
#include <psp2kern/types.h>
#include <psp2/touch.h>
#include <ctrl/ctrl.h>
#include <bt/bt_pairing.h>
#include <threadmgr/threadmgr.h>

/* SceCtrl and SceTouch hand every sample to SceDeci4pCtrlp before the program sees it */
int ksceTouchSetCtrlpCallback(int (*callback)(SceUInt32 panel, SceTouchData *sample));
int ksceTouchGetPanelInfo(SceUInt32 panel, SceTouchPanelInfo *info);

#endif
