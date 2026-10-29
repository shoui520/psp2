/* Static class initializers for the firmware-specific PAF class table. */
#include <stdint.h>

typedef struct PafStaticObject {
	const void *vtable;
	int32_t firstIndex;
	int32_t lastIndex;
	uint32_t state[17];
	uint16_t width;
	uint16_t height;
	uint32_t enabled;
} PafStaticObject;

static const void *PafStaticVtable(void)
{
#if defined(FW_TYPE_CEX)
	return (const void *)(uintptr_t)0x812e1608U;
#else
	return (const void *)(uintptr_t)0x812e22c0U;
#endif
}

void ScePafMisc_34B5499D(PafStaticObject *object)
{
	object->vtable = PafStaticVtable();
}

void ScePafMisc_7D7D1EB2(PafStaticObject *object)
{
	object->vtable = PafStaticVtable();
}

static void InitializeStaticObject(PafStaticObject *object)
{
	unsigned i;

	object->vtable = PafStaticVtable();
	object->firstIndex = -1;
	object->lastIndex = -1;
	for (i = 0; i < 17; i++)
		object->state[i] = 0;
	object->width = 0;
	object->height = 0;
	object->enabled = 1;
}

void ScePafMisc_2A230DDC(PafStaticObject *object)
{
	InitializeStaticObject(object);
}

void ScePafMisc_9057D20F(PafStaticObject *object)
{
	InitializeStaticObject(object);
}
