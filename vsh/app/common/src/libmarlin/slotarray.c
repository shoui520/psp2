/*
 * A view of an array of object slots (0x8107A3D0-): the slots emptied, a
 * free one found, one set and one read. The core's handles are kept in one.
 * The file's name is ours.
 */
#include "mln_internal.h"

/* every slot emptied */
unsigned int MlnImpl_8107A3D0(MlnSlotArray *array)
{
	unsigned int i;

	if (array == NULL || array->items == NULL)
		return 3;
	for (i = 0; i < array->count; i++)
		array->items[i] = NULL;
	return 0;
}

/* the first empty slot; 5 when there is none */
unsigned int MlnImpl_8107A428(MlnSlotArray *array, unsigned int *index)
{
	unsigned int i;

	if (array == NULL || array->items == NULL)
		return 3;
	for (i = 0; i < array->count; i++)
		if (array->items[i] == NULL) {
			*index = i;
			return 0;
		}
	return 5;
}

/* a slot set */
unsigned int MlnImpl_8107A48C(MlnSlotArray *array, unsigned int index, void *object)
{
	if (array == NULL || array->items == NULL || index >= array->count)
		return 3;
	array->items[index] = object;
	return 0;
}

/* a slot read */
unsigned int MlnImpl_8107A4C0(MlnSlotArray *array, unsigned int index, void **out)
{
	if (array == NULL || array->items == NULL || index >= array->count)
		return 3;
	*out = array->items[index];
	return 0;
}
