/* Copyright (C) 2011 The uOFW team
   See the file COPYING for copying permission.
*/

void cp0TimerIntrHandler();
void intrExcepHandler(void);
void SaveThreadContext();
void returnFromUserIntrHandler();
s32 defaultContextHook();
void intrExcepHandler2(void);
void invalidSyscallHandler();
void syscallExcepHandler(void);
void loc_00000DDC();
void syscallIsCop2Enabled();
void syscallCpuSuspendIntr();
void syscallCpuResumeIntr();
void syscallCpuResumeIntrWithSync();
void syscallInvertIntrBit();
void syscallSuspendIntrAndDisableCache();
void syscallResumeIntrAndEnableCache();
void syscallIsIntrContext();
void syscallGetIntrState();
void syscallGetCpuClockCounter();
void syscallGetCpuPrid();
void syscallGetCpuId();
void syscallIsKernelMode();
void syscallUnsupportedError();
void intcMaskAll();
void intcGetPending(s32*);
void intcGetMask(s32*);
void intcSetMask(s32*);

extern s32 mod_0400;
extern s32 mod_0468;
extern s32 mod_0DC4;
extern s32 mod_0E48;
extern s32 mod_0E50;

