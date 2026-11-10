/*
 * For a module that links GCC's runtime beside SNC's (libsnc), which defines the same 64-bit
 * division names: GCC's copies under labels of their own, so that the GCC code calling them
 * (bpabi.c's helpers) reaches GCC's, and SNC's code SNC's. Included before libgcc2.c or bpabi.c,
 * whose later declarations take the labels.
 */
#ifndef GNU_LABELS_H
#define GNU_LABELS_H

long long __divdi3(long long, long long) __asm__("__gnu_divdi3");
unsigned long long __udivdi3(unsigned long long, unsigned long long) __asm__("__gnu_udivdi3");

#endif
