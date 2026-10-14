/* Extended application-launch path options. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

typedef struct AppmgrLaunchOptions {
	SceSize size;
	char mapfilePath[1024];
	char app0Directory[1040];
} AppmgrLaunchOptions;

_Static_assert(sizeof(AppmgrLaunchOptions) == 2068,
	       "application launch options size");
_Static_assert(offsetof(AppmgrLaunchOptions, size) == 0,
	       "application launch options size field");
_Static_assert(offsetof(AppmgrLaunchOptions, mapfilePath) == 4,
	       "application launch map-file path");
_Static_assert(offsetof(AppmgrLaunchOptions, app0Directory) == 1028,
	       "application launch app0 directory");
