#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*SceWlanBtMonitorCallback)(unsigned int event,
				      unsigned int argument, unsigned int size,
				      const void *data);

int sceWlanBtAttachMonitor(SceWlanBtMonitorCallback *monitor);
int sceWlanBtDetachMonitor(SceWlanBtMonitorCallback *monitor);

/* SceWlanBtForDriver: 4C96C3B7 and 0FC89113. */
int sceWlanBtSetConfiguration(int request);
unsigned int sceWlanBtGetConfiguration(void);

unsigned int sceWlanGetConfiguration(void);
int sceWlanGetWakeOnTargetProcess(void);
int sceWlanSetConfiguration(int request);
int sceWlanUnregisterCallback(void);

#ifdef __cplusplus
}
#endif
