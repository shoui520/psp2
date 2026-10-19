#pragma once

int bbmcUsbExtIfGetMode(int *mode);
int bbmcUsbExtIfReleaseGpio(void);

/* usb_extif.c */
int bbmcInitializeUsbExtIf(void);
int bbmcFinalizeUsbExtIf(void);
int bbmcUsbExtIfAcquireGpio(void);
