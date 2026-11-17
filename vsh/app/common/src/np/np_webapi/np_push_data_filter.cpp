/*
 * SceNpWebApi: the push library's data filter (Sony's np_push/src/np_push_data_filter.cpp): the names
 * the extended data of a push event is checked against.
 */

#include "np_webapi_internal.h"

const void *const g_pushDataFilterVtable[] = {		/* 0x8100f0d0 */
	(const void *)npPushDataFilterDestruct,
	(const void *)npPushDataFilterDelete,
	NULL,
	NULL,
};

NpPushExDataNames *npPushDataFilterConstruct(NpPushExDataNames *names)
{
	SceNpCommonPs4_108738F2(names);
	names->reserved0[0] = (void *)g_pushDataFilterVtable;
	names->names = NULL;
	names->count = 0;
	return names;
}

NpPushExDataNames *npPushDataFilterDestruct(NpPushExDataNames *names)
{
	names->reserved0[0] = (void *)g_pushDataFilterVtable;
	npPushFree(names->names);
	SceNpCommonPs4_BA23AD76(names);
	return names;
}

int npPushDataFilterDelete(NpPushExDataNames *names)
{
	names->reserved0[0] = (void *)g_pushDataFilterVtable;
	npPushFree(names->names);
	SceNpCommonPs4_BA23AD76(names);
	return SceNpCommonPs4_BB5282C8(names);
}

int npPushDataFilterInit(NpPushExDataNames *names)
{
	void *memory;

	if (names->names != NULL)
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "Init",
			"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\np_push\\src\\np_push_data_filter.cpp", 32, "ASSERT",
			"this->names == 0"));
	memory = npPushAlloc(330);
	names->names = (char (*)[33])memory;
	if (memory == NULL)
		return -2141871612;
	sceClibMemset(memory, 0, 330);
	names->count = 0;
	return 0;
}
