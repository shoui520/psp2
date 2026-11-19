/*
 * libnet/netinternal.h: SceNet's SceNetInternal exports that other modules call. File name ours;
 * the link names are SceNetInternal_<NID> (no Sony name is known for them).
 */
#ifndef LIBNET_NETINTERNAL_H
#define LIBNET_NETINTERNAL_H

#include <net/netemu.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sceNetEmulationSet, as SceNetInternal exports it */
int SceNetInternal_DF815D48(SceNetEmulationParam *param, int flags);
/* the socket suspend policy */
int SceNetInternal_EE1985D9(int policy);
/* a process's policy (SceNetPsSleepCount.policy's bits) */
int SceNetInternal_EE94BF52(int processId, int *policy, int flags);
/* sets a thread's mode */
int SceNetInternal_9DBD2F60(int threadId, int mode, int flags);
/* a thread's mode */
int SceNetInternal_EF421A65(int threadId, int *mode, int flags);
/* the stack's sleep counts: up to COUNT SceNetPsSleepCount */
int SceNetInternal_DEF2357F(void *counts, int count, int flags);
/* the threads in the stack: up to COUNT SceNetPsThreadInfo */
int SceNetInternal_46E28336(void *list, int count, int flags);

#ifdef __cplusplus
}
#endif

#endif
