/*
 * ScebXCe's attribute list: the attributes of an element, in the order they were added, their
 * storage taken from and given back to the list's pool.
 */
#include "bxce_internal.h"

/* a cleared attribute from the pool, else a new one */
BxceAttr *bxceAttributeListNewAttr(BxceAttributeList *list)
{
	BxceAttr *attr;

	attr = bxceAttrPoolPop(list->pool);
	if (attr != NULL) {
		bxceAttrReset(attr);
		return attr;
	}

	attr = (BxceAttr *)ScebXCe_B8DDE741(sizeof(BxceAttr));
	if (attr == NULL)
		return NULL;

	if (bxceAttrInitialize(attr) != 0) {
		bxceAttrTerminate(attr);
		ScebXCe_EC560647(attr);
		return NULL;
	}

	return attr;
}

/* appends the attribute; -1 when the list already has one of its name (it stays appended) */
int bxceAttributeListAppend(BxceAttributeList *list, BxceAttr *attr)
{
	BxceAttr *it;

	it = list->head;
	if (it != NULL) {
		list->tail->next = attr;
	} else {
		it = attr;
		list->head = attr;
	}
	list->tail = attr;
	list->length++;

	for (; it != NULL; it = it->next) {
		if (it != attr && it->nameLength == attr->nameLength &&
		    it->name[0] == attr->name[0] &&
		    bxceMemcmp(it->name, attr->name, attr->nameLength) == 0)
			return -1;
	}

	return 0;
}

int ScebXCe_E7C8B20D(BxceAttributeList *list)
{
	list->pool = (BxceAttrPool *)ScebXCe_B8DDE741(sizeof(BxceAttrPool));
	if (list->pool == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	bxceAttrPoolInitialize(list->pool);
	list->head = NULL;
	list->tail = NULL;
	list->length = 0;
	return 0;
}

int ScebXCe_B367042F(BxceAttributeList *list)
{
	BxceAttr *attr, *next;

	if (list->pool != NULL) {
		for (attr = list->head; attr != NULL; attr = next) {
			int kept = bxceAttrPoolPush(list->pool, attr);

			next = attr->next;
			if (!kept) {
				bxceAttrTerminate(attr);
				ScebXCe_EC560647(attr);
			}
		}
	}

	list->head = NULL;
	list->tail = NULL;
	list->length = 0;
	return 0;
}

int ScebXCe_9C2F9897(BxceAttributeList *list)
{
	BxceAttr *attr;

	ScebXCe_B367042F(list);

	if (list->pool != NULL) {
		while ((attr = bxceAttrPoolPop(list->pool)) != NULL) {
			bxceAttrTerminate(attr);
			ScebXCe_EC560647(attr);
		}
		if (list->pool != NULL)
			ScebXCe_EC560647(list->pool);
		list->pool = NULL;
	}

	list->reserved = 0;
	return 0;
}

/* the attribute of this name */
BxceAttr *ScebXCe_2AD8DA26(const BxceAttributeList *list, const char *name, SceSize length)
{
	BxceAttr *attr;

	for (attr = list->head; attr != NULL; attr = attr->next) {
		if (attr->nameLength == length && bxceMemcmp(attr->name, name, length) == 0)
			return attr;
	}

	return NULL;
}

/* the index'th attribute; the index is not checked */
BxceAttr *ScebXCe_01DC4E22(const BxceAttributeList *list, SceSize index)
{
	BxceAttr *attr;

	for (attr = list->head; index != 0; index--)
		attr = attr->next;

	return attr;
}

int ScebXCe_69F4B8B6(const BxceAttributeList *list)
{
	return list->length;
}

/*
 * Adds an attribute. A failed or duplicate attribute is freed without its buffers, and a
 * duplicate stays linked, as in Sony's code.
 */
int ScebXCe_3DD761CF(BxceAttributeList *list, const char *name, const char *value)
{
	BxceAttr *attr;
	int length;

	attr = bxceAttributeListNewAttr(list);
	if (attr == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	length = bxceStrnlen(name, BXCE_ATTR_STRING_MAX);
	if (length <= 0) {
		ScebXCe_EC560647(attr);
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;
	}

	if (bxceAttrSetName(attr, name, length) != 0) {
		ScebXCe_EC560647(attr);
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	if (bxceAttrSetValue(attr, value, bxceStrnlen(value, BXCE_ATTR_STRING_MAX)) != 0) {
		ScebXCe_EC560647(attr);
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	if (bxceAttributeListAppend(list, attr) != 0) {
		if (attr != NULL)
			ScebXCe_EC560647(attr);
		return SCE_BXCE_ERROR_DUPLICATE_ATTRIBUTE_NAME;
	}

	return 0;
}
