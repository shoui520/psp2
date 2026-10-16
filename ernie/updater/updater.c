/*
 * Ernie updater ("Ernie Downloader", boot cluster 1: flash 0x1000-0xbfff and
 * 0x35400-0x3ffff): the resident program that is never rewritten by an update (the
 * update patches carry those ranges zeroed). It programs the application's flash
 * blocks (FlProg, FlChksum), checks the image (SHA-1), swaps the boot clusters, and
 * answers Kermit commands meanwhile. The DEM-3000L (0x00415000) carries a build with log
 * messages, version 0x01.0x00.0x33.0x00 by its banner; the other boards' builds have none.
 *
 * Skeleton. Every body is a placeholder; the names are those its log messages use.
 * Located in the DEM-3000L 1.0.3.6 flash: configStorageInit 0xb2fb, updaterInit 0x35958,
 * updaterInitErnieUpdMode 0x38ef, nvsInit 0x522c, UiSetMultiCnPort 0x3b123,
 * powerOnWlan 0x3a73f, powerOffWlan 0x3a7e9; the others are not yet told apart from the
 * command handlers that log them.
 */

#include "ernie.h"
#include "skeleton.h"

STUB(main)
STUB(configStorageInit)
STUB(updaterInit)
STUB(updaterInitErnieUpdMode)
STUB(nvsInit)
STUB(UiSetMultiCnPort)
STUB(powerOnWlan)
STUB(powerOffWlan)
STUB(powerCtrlDeviceReset)
STUB(ctrlVoltage)
STUB(powerCtrlLED)
