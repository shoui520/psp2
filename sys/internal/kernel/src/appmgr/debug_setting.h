#ifndef APPMGR_DEBUG_SETTING_H
#define APPMGR_DEBUG_SETTING_H

typedef int (*AppmgrDebugSettingCallback)(int setting, int value, void *context);

int appmgrSetDebugSettingCallback(AppmgrDebugSettingCallback callback, void *context);
int _debugSettingCallBack(int setting, int value, void *context);

/* debug_setting.c */
int appmgrInitializeDebugSettings(void);
int appmgrGetFakeNoMemoryCard(void);
int appmgrGetFakeSdSlotBroken(void);
int appmgrIsTrcCheckNotifyEnabled(void);

#endif
