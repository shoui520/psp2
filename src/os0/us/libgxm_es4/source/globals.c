/*
 * SceGxm: library-wide state.
 */

#include "gxm_internal.h"

GxmGlobals gxmGlobals;

int gxmSceneStateInit(void)
{
	gxmGlobals.sceneActive = 0;
	return 0;
}

/* only one scene may be open at a time */
int gxmSceneBegin(void)
{
	do {
		if (gxmLoadExclusive(&gxmGlobals.sceneActive) != 0)
			return SCE_GXM_ERROR_WITHIN_SCENE;
	} while (gxmStoreExclusive(&gxmGlobals.sceneActive, 1) != 0);
	return 0;
}

int gxmSceneEnd(void)
{
	do {
		if (gxmLoadExclusive(&gxmGlobals.sceneActive) != 1)
			return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	} while (gxmStoreExclusive(&gxmGlobals.sceneActive, 0) != 0);
	return 0;
}

SceBool gxmSceneIsActive(void)
{
	return gxmGlobals.sceneActive == 1;
}

int gxmSceneStateTerm(void)
{
	gxmGlobals.sceneActive = 0;
	return 0;
}

/* returns the value before the increment */
SceUInt32 gxmCounterIncrement(void)
{
	SceUInt32 value;

	do {
		value = gxmLoadExclusive(&gxmGlobals.counter);
	} while (gxmStoreExclusive(&gxmGlobals.counter, value + 1) != 0);
	return value;
}
