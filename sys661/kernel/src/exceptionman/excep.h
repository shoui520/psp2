/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

void initExceptionVectors(void);
extern u32 syscallHandler;
void resetExceptionStub(void);
void nmiExceptionJumpStub(void);
void nmiExceptionJumpStubEnd(void);
void ExceptionManagerForKernel_79454858(void);
void defaultExceptionSpinLoop(void);
void nmiInit(void);
void nmiHandler(void);

