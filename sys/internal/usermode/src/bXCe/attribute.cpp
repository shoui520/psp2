/*
 * ScebXCe's attributes: an attribute's name and value buffers, set, appended to and grown.
 */
#include "bxce_internal.h"

int bxceAttrAppendName(BxceAttr *attr, const char *name, int length)
{
	while (attr->nameCapacity - attr->nameLength <= length) {
		if (bxceBufferGrow(&attr->name, &attr->nameCapacity, 0) != 0)
			return SCE_BXCE_ERROR_NO_MEMORY;
	}

	bxceMemcpy(attr->name + attr->nameLength, name, length);
	attr->nameLength += length;
	return 0;
}

int bxceAttrAppendValue(BxceAttr *attr, const char *value, int length)
{
	char *d;
	int ret, i;

	while (attr->valueCapacity - attr->valueLength <= length) {
		ret = bxceBufferGrow(&attr->value, &attr->valueCapacity, 0);
		if (ret != 0)
			return ret;
	}

	d = attr->value + attr->valueLength;
	if (length > 8) {
		bxceMemcpy(d, value, length);
	} else if (length > 0) {
		for (i = length; i != 0; i--)
			*d++ = *value++;
	}
	attr->valueLength += length;
	return 0;
}

/* empties the attribute for reuse, keeping its buffers */
BxceAttr *bxceAttrReset(BxceAttr *attr)
{
	attr->nameLength = 0;
	attr->valueLength = 0;
	attr->next = NULL;
	return attr;
}

/* the name is terminated also when the buffer could not grow */
int bxceAttrSetName(BxceAttr *attr, const char *name, int length)
{
	int ret;

	attr->nameLength = 0;
	while (attr->nameCapacity - attr->nameLength <= length) {
		if (bxceBufferGrow(&attr->name, &attr->nameCapacity, 0) != 0) {
			ret = SCE_BXCE_ERROR_NO_MEMORY;
			goto out;
		}
	}

	bxceMemcpy(attr->name + attr->nameLength, name, length);
	attr->nameLength += length;
	ret = 0;
out:
	attr->name[length] = 0;
	return ret;
}

int bxceAttrSetValue(BxceAttr *attr, const char *value, int length)
{
	char *d;
	int ret, i;

	attr->valueLength = 0;
	while (attr->valueCapacity - attr->valueLength <= length) {
		ret = bxceBufferGrow(&attr->value, &attr->valueCapacity, 0);
		if (ret != 0)
			goto out;
	}

	d = attr->value + attr->valueLength;
	if (length > 8) {
		bxceMemcpy(d, value, length);
	} else if (length > 0) {
		for (i = length; i != 0; i--)
			*d++ = *value++;
	}
	attr->valueLength += length;
	ret = 0;
out:
	attr->value[length] = 0;
	return ret;
}

int bxceAttrInitialize(BxceAttr *attr)
{
	attr->nameLength = 0;
	attr->valueLength = 0;
	attr->value = NULL;
	attr->next = NULL;
	attr->name = NULL;
	attr->reserved = 0;

	attr->name = (char *)ScebXCe_B8DDE741(BXCE_ATTR_INITIAL_SIZE);
	if (attr->name == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;
	attr->value = (char *)ScebXCe_B8DDE741(BXCE_ATTR_INITIAL_SIZE);
	if (attr->value == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	attr->nameCapacity = BXCE_ATTR_INITIAL_SIZE;
	attr->valueCapacity = BXCE_ATTR_INITIAL_SIZE;
	attr->next = NULL;
	return 0;
}

int bxceAttrTerminate(BxceAttr *attr)
{
	if (attr->name != NULL)
		ScebXCe_EC560647(attr->name);
	if (attr->value != NULL)
		ScebXCe_EC560647(attr->value);
	attr->name = NULL;
	attr->value = NULL;
	return 0;
}
