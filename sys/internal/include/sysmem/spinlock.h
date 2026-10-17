/*
 * The lock words of SceCpu's spinlocks (sysmem's cpu_spinlock.S; the SDK's kernel/cpu/spinlock.h
 * declares nothing). No source names these types: they keep vitasdk's names.
 */
#pragma once

/* a spinlock: one owner at a time */
typedef int SceKernelSpinlock;

/* a reader-writer spinlock: many readers or one writer */
typedef int SceKernelRWSpinlock;

/* the interrupt state a ...CpuSuspendIntr lock returns and its unlock restores */
typedef int SceKernelIntrStatus;
