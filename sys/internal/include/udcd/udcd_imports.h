/*
 * The imports of SceUdcd (os0:kd/bootimage/udcd) not declared elsewhere, with
 * the meaning seen at its call sites.
 */

#pragma once

#include <psp2kern/types.h>
#include <power/power.h>
#include <lowio/pervasive.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/aimgr.h>

/* ScePervasiveForDriver: the USB blocks of a bus */

/* ScePowerForDriver: USB charging */
int kscePowerBatteryEnableUsbCharging(void);
int kscePowerBatteryDisableUsbCharging(void);
int kscePowerBatteryStopUsbCharging(void);
int kscePowerBatterySetUsbStatus(int status);
int ScePowerForDriver_8C0D2051(unsigned int *status);
int ScePowerForDriver_F535D928(unsigned int mask, int enabled);

int ksceSblACMgrIsAllowedUsbSerial(void);
