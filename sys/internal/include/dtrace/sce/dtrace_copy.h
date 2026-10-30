/*
 * Copies between DTrace and a user process (Sony's dtrace/sce/dtrace_copy.c):
 * a fault sets the CPU's DTrace fault flags instead of raising one.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

int dtrace_copyin_user(void *kaddr, uintptr_t uaddr, size_t size);
int copyin(uintptr_t uaddr, void *kaddr, size_t size);
int copyout(const void *kaddr, uintptr_t uaddr, size_t size);
