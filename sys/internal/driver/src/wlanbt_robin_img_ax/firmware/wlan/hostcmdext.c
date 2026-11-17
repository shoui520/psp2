/*
 * Host commands the RAM code answers itself (0x262c0, 0x27de0): the antenna
 * selection and the version (the SoC register window and the stubs of 0x96
 * are in sys/socpower.c, the object Sony built them in). The filename is
 * inferred.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/event.h"
#include "robin/eventpower.h"
#include "robin/hostcmd.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

#define HOSTCMD_VERSION		0xdf0e4109	/* what the host reads as the firmware version */

/* The host command 0x20 selecting the antenna of the second radio (not used by this chip's first). */
unsigned int hostCmd0020Set(unsigned int secondary, unsigned int antenna)
{
	int level = macGetEventLevel();
	unsigned int posture = sysIrqDisable();
	int value;

	if (level == 1) {
		SOC_REG(0x200) |= 0x28;
		if (antenna == 1) {
			gpioPinWriteLow(3, 0);
			gpioPinSelect(3, 1);
			value = 1;
		} else {
			gpioPinWriteLow(3, 1);
			gpioPinSelect(3, 1);
			value = 0;
		}
		gpioPinWriteLow(5, value);
		gpioPinSelect(5, 1);
	}
	return sysIrqRestore(posture);
}


unsigned int hostCmd0003Version(void)
{
	return HOSTCMD_VERSION;
}
