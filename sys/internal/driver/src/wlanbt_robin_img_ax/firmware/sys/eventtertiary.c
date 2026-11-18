/*
 * The tertiary event path's configuration (0x3460..0x352d).
 *
 * Marvell published no source; the filename and all names are inferred from
 * what the code does, and the register offsets are the hardware's.
 */
#include <stddef.h>
#include "robin/btpower.h"
#include "robin/amp.h"
#include "robin/eventpower.h"
#include "robin/eventroute.h"
#include "robin/hostcmd.h"
#include "robin/conn.h"
#include "robin/keyMgmtAp.h"
#include "robin/mdev.h"
#include "robin/nvram.h"
#include "robin/osa.h"
#include "robin/phy.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wlantask.h"
#include "eventpath_regs.h"

/* Bit 2 of radio register 18, read once; 0xff until then. */
static uint8_t sysTertiaryRadioFlag __attribute__((section(".itcm.rw"))) = 0xff;

/* 0x3460 */
void sysTertiaryPathConfigure(int fmActive)
{
	sysConfigureTertiaryEventPath();
	EVENT_PATH_COMMAND = (EVENT_PATH_COMMAND & 0xcffffffd) | 0x3800000;
	EVENT_PATH_FRAME &= 0xffffc7fe;
	if (!socStrapIsSet())
		EVENT_PATH_PENDING_CONTROL |= 0x10001;
	if (sysTertiaryRadioFlag == 0xff) {
		rfRegBegin();
		sysTertiaryRadioFlag = rfRegRead(18) & 4;
		rfRegEnd();
	}
	if (sysTertiaryRadioFlag == 0)
		EVENT_PATH_PENDING_CONTROL |= 0x10001;
	EVENT_PATH_STATUS = (EVENT_PATH_STATUS & 0x9ffffff0) + 0x4000000e;
}

