#ifndef SCE_DECI4P_CTRLP_H
#define SCE_DECI4P_CTRLP_H
#include <psp2kern/types.h>
#include <psp2/touch.h>
#include <ctrl/ctrl.h>
#include <bt/bt_pairing.h>
#include <threadmgr/threadmgr.h>

/* SceCtrl and SceTouch hand every sample to SceDeci4pCtrlp before the program sees it */
int sceTouchSetCtrlpCallback(int (*callback)(SceUInt32 panel, SceTouchData *sample));
/* vitasdk: ksceTouchGetPanelInfo */
int SceTouchForDriver_937DB4C0(SceUInt32 panel, SceTouchPanelInfo *info);

/* SceBt deletes a registered device; the caller passes its address as two words */
/* vitasdk: ksceBtDeleteRegisteredInfo */
int SceBtForDriver_E6F659E0(SceUInt32 mac0, SceUInt32 mac1);
#define SCE_BT_ERROR_REG_DELETE_CONNECTING	((int)0x802F0109)

#endif
