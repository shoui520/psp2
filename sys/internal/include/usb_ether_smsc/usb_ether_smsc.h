/* Private imports of SceUsbEtherSmsc; unidentified APIs retain their NIDs. */
#pragma once
#include <scetypes.h>
#include <syscon/syscon.h>
#include <idstorage/idstorage.h>
#include <threadmgr/threadmgr.h>

int ScePowerForDriver_B24263F0(int kind, void (*handler)(int event, int value));
