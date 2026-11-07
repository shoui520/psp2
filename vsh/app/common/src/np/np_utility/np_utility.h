/*
 * SceNpUtility's NP community exports (vs0:sys/external/np_utility.suprx), as the common
 * dialogs call them through the community helper (../np_community/np_community.h). No Sony
 * name is known for any of them; what each does is read from its callers.
 */
#ifndef NP_UTILITY_H
#define NP_UTILITY_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the community's state: 0 until it is initialized, 1 after */
int SceNpUtility_16954FFF(void);
/* sets the community's state */
int SceNpUtility_5D6FC99E(int state);

/* the community's three ways to start: with its own memory, with the caller's allocator, or
   with the caller's two values */
int SceNpUtility_5FC4B7D5(int id);
int SceNpUtility_4A3FDD3B(int id, void *(*alloc)(SceSize size), void (*free)(void *ptr));
int SceNpUtility_B5711AF9(int id, int arg1, int arg2);

/* the community's start, in order, and their undoing in reverse */
int SceNpUtility_E38548AB(int id);
int SceNpUtility_9F142A92(int id);
int SceNpUtility_17385829(int id);
int SceNpUtility_6DF884DA(int id);
int SceNpUtility_26B4AC00(int id);
int SceNpUtility_7A86C37E(int id);
int SceNpUtility_418E1324(int id);

#ifdef __cplusplus
}
#endif

#endif
