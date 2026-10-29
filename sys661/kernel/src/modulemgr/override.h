/* Copyright (C) 2011 - 2015 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef OVERRIDE_H
#define	OVERRIDE_H

#include <common_header.h>

SceBool _CheckOverride(s32 apiType, void *modBuf, SceUID *pFd);
const char *_CheckPathOverride(s32 mode, const char *path, s32 *pReplaced);

#endif	/* OVERRIDE_H */

