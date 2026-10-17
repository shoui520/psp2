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

int ksceSblACMgrIsAllowedUsbSerial(void);
