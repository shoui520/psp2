/* Shared SceLowio state; mapped by module initialization. */
#include "lowio_internal.h"

ScePervasiveMiscRegisters *s_misc;
ScePervasiveVidRegisters *s_video;
ScePervasiveMailboxRegisters *s_mailbox;
ScePervasive2Registers *s_pervasive2;
ScePervasiveResetRegisters *s_reset;
ScePervasiveGateRegisters *s_gate;
volatile SceUartClkgenRegisters *s_uartClock;
ScePervasiveBaseClkRegisters *s_baseClock;
SceKernelSpinlock s_pervasiveLock;
