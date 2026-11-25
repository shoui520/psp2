/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

void *mymemset(void *dstVoid, u8 fillByte, s32 count);
s32 SuspendIntc(s32 flag, void *param);
s32 ResumeIntc(s32 flag, void *param);
void InterruptDisableInTable(u32 intrNum);
void InterruptEnableInTable(u32 intrNum);
void AllLevelInterruptDisable(u32 intrNum);
