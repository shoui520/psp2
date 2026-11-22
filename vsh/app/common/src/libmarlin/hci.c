/*
 * SceLibMarlin's side of the Marlin HCI: what the core calls into the HCI layer.
 */
#include "mln_internal.h"

/* the core's first start: the HCI layer has nothing to set up */
int MlnImpl_81080BF4(void)
{
	return 0;
}

/* the core's last stop: nothing to take down */
void MlnImpl_81080BF8(void)
{
}
