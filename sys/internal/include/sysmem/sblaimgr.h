/* SceSysmem's SceSblAIMgrForDriver checks that vitasdk's psp2kern/kernel/aimgr.h does not declare. */

#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int ksceSblAimgrIsJapaneseFat(void);
int ksceSblAimgrIsPrototypeRev2(void);
int ksceSblAimgrIsPrototypeRev7(void);
int ksceSblAimgrIsToolRev3(void);
int ksceSblAimgrIsToolRev4(void);
int ksceSblAimgrIsToolRev5(void);
