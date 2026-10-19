/* SceSblSmCommForKernel: secure-module communication. */
#pragma once

#include <scetypes.h>
#include <authmgr/sblauthmgr.h>

/* Both words are returned by scheduler wait. Callers return result as status;
 * additionalResult is preserved without assigning it a more specific meaning.
 */
typedef struct SceSblSmCommPair {
	int result;
	int additionalResult;
} SceSblSmCommPair;

_Static_assert(sizeof(SceSblSmCommPair) == 8, "secure-module stop result size");

typedef int SceSblSmCommId;

/* vitasdk: ksceSblSmCommStartSmFromFile */
int SceSblSmCommForKernel_7863A0CC(SceUInt32 priority, const char *path,
	SceUInt32 command, SelfAuthRequest *auth, SceSblSmCommId *id);
/* vitasdk: ksceSblSmCommStartSmFromData */
int SceSblSmCommForKernel_039C73B1(SceUInt32 priority, const void *data,
	SceUInt32 size, SceUInt32 command, SelfAuthRequest *auth, SceSblSmCommId *id);
int sceSblSmCommCallFunc(SceSblSmCommId id, SceUInt32 service,
	SceUInt32 *serviceResult, void *data, SceSize size);	/* vitasdk: ksceSblSmCommCallFunc */
int sceSblSmCommStopSm(SceSblSmCommId id, SceSblSmCommPair *result);
