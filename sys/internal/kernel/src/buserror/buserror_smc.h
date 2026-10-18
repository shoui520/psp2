#ifndef BUSERROR_SMC_H
#define BUSERROR_SMC_H

#include <scetypes.h>

void busErrorSmc10C(void);
void busErrorSmc10D(void);
void busErrorSmc122(SceUInt32 command, SceUInt32 ttbr0, SceUInt32 vbar, void *sysroot);

#endif
