/* SceMotionDevDummy import additions and recovered timer interface. */
#pragma once
#include <psp2kern/types.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>

int ksceSblACMgrIsSystemProgram(SceUID pid);
void *__memset_chk(void *dst, int value, unsigned size, unsigned dstSize);
