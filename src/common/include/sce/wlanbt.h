#pragma once

typedef int (*SceWlanBtMonitorCallback)(unsigned int event,
				      unsigned int argument, unsigned int size,
				      const void *data);

int ksceWlanBtAttachMonitor(SceWlanBtMonitorCallback *monitor);
int ksceWlanBtDetachMonitor(SceWlanBtMonitorCallback *monitor);

/* SceWlanBtForDriver: 4C96C3B7 and 0FC89113. */
int ksceWlanBtSetConfiguration(int request);
unsigned int ksceWlanBtGetConfiguration(void);
