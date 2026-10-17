/**
 * \kernelgroup{SceCpu}
 * \usage{psp2kern/kernel/cpu.h}
 */


#ifndef _PSP2KERN_KERNEL_CPU_H_
#define _PSP2KERN_KERNEL_CPU_H_

#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu/atomic.h>
#include <psp2kern/kernel/cpu/cache.h>
#include <psp2kern/kernel/cpu/corelock.h>
#include <psp2kern/kernel/cpu/mmu.h>
#include <psp2kern/kernel/cpu/ple.h>
#include <psp2kern/kernel/cpu/rw_spinlock.h>
#include <psp2kern/kernel/cpu/spinlock.h>
#include <psp2kern/kernel/sysclib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Call this when entering a syscall
 *
 * @param      state  The state
 */
#define ENTER_SYSCALL(state) \
	do { \
		asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r" (state)); \
		asm volatile ("mcr p15, 0, %0, c13, c0, 3" :: "r" (state << 16) : "memory"); \
	} while(0)

/**
 * @brief      Call this when exiting a syscall
 *
 * @param      state  The state
 */
#define EXIT_SYSCALL(state) \
	do { \
		asm volatile ("mcr p15, 0, %0, c13, c0, 3" :: "r" (state) : "memory"); \
	} while (0)


/**
 * @brief Returns the CPU ID of the calling processor
 *
 * @return The CPU ID
 */
int sceKernelCpuId(void);

/**
 * @brief Suspend intr of calling processor
 *
 * @return The prev intr status
 */
SceKernelIntrStatus SceCpuForDriver_821FC0EE(void);

/**
 * @brief Suspend intr of calling processor
 *
 * @param[in] prev_state The prev status
 *
 * @return The prev intr status
 */
SceKernelIntrStatus sceKernelCpuResumeIntr(SceKernelIntrStatus prev_state);


// sceKernelDomainTextMemcpy removed from 3.63
#if PSP2_SDK_VERSION >= 0x3630000 && defined(__USE_SCE_KERNEL_DOMAIN_TEXT_MEMCPY_IMPORT)
#undef __USE_SCE_KERNEL_DOMAIN_TEXT_MEMCPY_IMPORT
#endif

/**
 * @brief         The domain text memcpy
 *
 * @param[in] dst The destination
 * @param[in] src The source
 * @param[in] len The length
 *
 * @return        always SCE_OK.
 */
#if defined(__USE_SCE_KERNEL_DOMAIN_TEXT_MEMCPY_IMPORT)
int ksceKernelDomainTextMemcpy(void *dst, const void *src, SceSize len);
#else
/* SNC compat: GCC extended-asm helper removed */
#endif


// sceKernelGetVmaccessRange had prototype changed in some times
void SceCpuForKernel_C32687D0(SceUIntPtr *pRangeStart, SceUIntPtr *pRangeEnd);

void SceCpuForKernel_050B8EF6(void);
void SceCpuForKernel_7B0643E2(void);

SceUInt8 SceCpuForKernel_FD54A91D(void);
void SceCpuForKernel_C6426313(void);

SceBool sceKernelIsUncacheAddressInTmpFsGame(void *address);


#ifdef __cplusplus
}
#endif

#endif /* _PSP2KERN_KERNEL_CPU_H_ */
