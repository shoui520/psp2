/* Shared SceLowio state; mapped by module initialization. */
#include "lowio_internal.h"

uint8_t *s_misc, *s_video, *s_mailbox, *s_pervasive2;
uint8_t *s_reset, *s_gate, *s_uartClock, *s_baseClock;
uint8_t *s_tas[8];
SceKernelSpinlock s_pervasiveLock;
