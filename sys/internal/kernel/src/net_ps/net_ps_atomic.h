/* Architectural primitives for SceNetPs's exclusive-monitor operations. */
#ifndef NET_PS_ATOMIC_H
#define NET_PS_ATOMIC_H

static inline void netPsMemoryBarrier(void)
{
	__asm__ volatile ("dmb sy" ::: "memory");
}

static inline unsigned int netPsLoadExclusive(unsigned int *address)
{
	unsigned int value;

	__asm__ volatile ("ldrex %0, %1" : "=r" (value) : "Q" (*address));
	return value;
}

static inline unsigned int netPsStoreExclusive(unsigned int *address,
					       unsigned int value)
{
	unsigned int status;

	__asm__ volatile ("strex %0, %2, %1"
			  : "=&r" (status), "+Q" (*address)
			  : "r" (value) : "memory");
	return status;
}

#endif
