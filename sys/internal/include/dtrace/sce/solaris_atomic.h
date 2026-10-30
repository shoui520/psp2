/*
 * Solaris atomic operations, on the CPU's exclusive loads and stores.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

void atomic_add_32(volatile uint32_t *target, int32_t delta);
uint32_t atomic_add_32_nv(volatile uint32_t *target, int32_t delta);
void atomic_add_64(volatile uint64_t *target, int64_t delta);
uint32_t atomic_cas_32(volatile uint32_t *target, uint32_t cmp, uint32_t newval);
void *atomic_cas_ptr(volatile void *target, void *cmp, void *newval);
uint32_t atomic_or_32_nv(volatile uint32_t *target, uint32_t bits);
uint32_t atomic_and_32_nv(volatile uint32_t *target, uint32_t bits);
