/* SceSblSmCommForKernel: secure-module communication. */
#pragma once

#include <psp2kern/types.h>

/* Both words are returned by scheduler wait. Callers return result as status;
 * additionalResult is preserved without assigning it a more specific meaning.
 */
typedef struct SceSblSmCommPair {
	int result;
	int additionalResult;
} SceSblSmCommPair;

_Static_assert(sizeof(SceSblSmCommPair) == 8, "secure-module stop result size");

typedef int SceSblSmCommId;

int ksceSblSmCommStartSmFromFile(SceUInt32 priority, const char *path,
	SceUInt32 command, SceAuthInfo *auth, SceSblSmCommId *id);
int ksceSblSmCommStartSmFromData(SceUInt32 priority, const void *data,
	SceUInt32 size, SceUInt32 command, SceAuthInfo *auth, SceSblSmCommId *id);
int ksceSblSmCommCallFunc(SceSblSmCommId id, SceUInt32 service,
	SceUInt32 *serviceResult, void *data, SceSize size);
int ksceSblSmCommStopSm(SceSblSmCommId id, SceSblSmCommPair *result);
