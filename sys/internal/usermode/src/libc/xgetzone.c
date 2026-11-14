/*
 * _Getzone function: the time zone's names and offset. The library takes no
 * time zone from the environment: an empty zone.
 */
#include <stddef.h>

#include "xtime.h"

static const char *tzone = NULL;

const char *_Getzone(void)
{
	if (tzone == NULL)
		tzone = ":";
	return tzone;
}
