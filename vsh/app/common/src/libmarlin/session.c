/*
 * The core's sessions: the session record (its id, mode and start time)
 * kept for the platform (0x8104a448-0x8104a55c)
 */

#include "mln_internal.h"

/* starts a session: its id and mode from the platform, now as its start */
unsigned int MlnImpl_8104A448(void)
{
	int id = -1;
	unsigned int flags = 0;
	int mode = 0;
	unsigned int now;

	if (MlnImpl_8105F2C4(&id, &flags) == 0)
		mode = (flags & 1) ? 2 : 1;
	now = MlnTimeNow();
	if (now == (unsigned int)-1)
		return 5;
	return MlnImpl_81052D14(id, mode, now, MlnImpl_81007098());
}

/* starts a session unless one is running */
unsigned int MlnImpl_8104A4B8(void)
{
	int id = -1;
	int mode = 0;
	unsigned int ret;

	ret = MlnImpl_81052D84(&id, &mode);
	if (ret != 0x220B0001)
		return ret;
	return MlnImpl_8104A448();
}

/* ends the session */
unsigned int MlnImpl_8104A558(void)
{
	return 0;
}
