/* Copyright (C) 2011 The uOFW team
   See the file COPYING for copying permission.
*/

void cp0TimerIntrHandler(void);
void intrExcepHandler(void);
void SaveThreadContext(void);
void returnFromUserIntrHandler(void);
s32 defaultContextHook(void);
void intrExcepHandler2(void);
void invalidSyscallHandler(void);
void syscallExcepHandler(void);
void syscallMonitorReturn(void);
void syscallIsCop2Enabled(void);
void syscallCpuSuspendIntr(void);
void syscallCpuResumeIntr(void);
void syscallCpuResumeIntrWithSync(void);
void syscallInvertIntrBit(void);
void syscallSuspendIntrAndDisableCache(void);
void syscallResumeIntrAndEnableCache(void);
void syscallIsIntrContext(void);
void syscallGetIntrState(void);
void syscallGetCpuClockCounter(void);
void syscallGetCpuPrid(void);
void syscallGetCpuId(void);
void syscallIsKernelMode(void);
void syscallUnsupportedError(void);
void intcMaskAll(void);
void intcGetPending(s32 *);
void intcGetMask(s32 *);
void intcSetMask(s32 *);

extern s32 contextHookBeforeInsn;
extern s32 contextHookAfterInsn;
extern s32 syscallMonitorHookInsn;
extern s32 syscallReturnScrubStartInsn;
extern s32 syscallReturnScrubT3Insn;
