/*
 * The NP community helper the common dialogs and several applications link: it starts
 * SceNpUtility's NP community once for the process and stops it. Nothing names the helper or
 * its file: every name here is ours.
 */
#ifndef NP_COMMUNITY_H
#define NP_COMMUNITY_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* how the community gets its memory */
#define NP_COMMUNITY_INIT_DEFAULT	0
#define NP_COMMUNITY_INIT_ALLOCATOR	1
#define NP_COMMUNITY_INIT_PARAM		2

int npCommunityInit(SceUInt32 type, void *(*alloc)(SceSize size), void (*free)(void *ptr),
		    int arg1, int arg2);
int npCommunityInitWithAllocator(void *(*alloc)(SceSize size), void (*free)(void *ptr));
int npCommunityTerm(void);

#ifdef __cplusplus
}
#endif

#endif
