#include "util.h"
#include "../paf_types.h"

extern int sce_paf_strcmp(const char *left, const char *right);

unsigned char CxmlUtilCheckElementName(const CxmlElement *elm, const char *valid_name)
{
	const char *name = CxmlElementGetName(elm);

	return *name == *valid_name && !sce_paf_strcmp(name, valid_name);
}

void CxmlUtilFindChildElement(CxmlElement *result, const CxmlElement *parent,
			      const char *name)
{
	CxmlElement child, next;

	CxmlElementGetFirstChild(&child, parent);
	while (child.doc && child.offset >= 0) {
		const char *child_name = CxmlElementGetName(&child);

		if (*child_name == *name && !sce_paf_strcmp(child_name, name)) {
			*result = child;
			return;
		}
		CxmlElementGetNextSibling(&next, &child);
		child = next;
	}
	result->doc = NULL;
	result->offset = -1;
}

void CxmlUtilFindChildElementByID(CxmlElement *result, const CxmlElement *parent,
				  const char *name, const char *idname, const char *idvalue)
{
	CxmlElement child, next;
	CxmlAttribute attr;
	const char *child_id;
	uint32_t len;

	CxmlElementGetFirstChild(&child, parent);
	attr.doc = NULL;
	attr.element_offset = -1;
	attr.offset = -1;
	while (child.doc && child.offset >= 0) {
		const char *child_name = CxmlElementGetName(&child);

		if (*child_name == *name && !sce_paf_strcmp(child_name, name)) {
			if (!idname || !idvalue) {
				*result = child;
				return;
			}
			if (CxmlElementGetAttributeByName(&child, idname, &attr) == SCE_PAF_OK &&
			    ((CxmlAttributeGetType(&attr) == AttributeType_ID &&
			      CxmlAttributeGetID(&attr, &child_id) == SCE_PAF_OK) ||
			     (CxmlAttributeGetType(&attr) == AttributeType_String &&
			      CxmlAttributeGetString(&attr, &child_id, &len) == SCE_PAF_OK)) &&
			    *child_id == *idvalue && !sce_paf_strcmp(child_id, idvalue)) {
				*result = child;
				return;
			}
		}
		CxmlElementGetNextSibling(&next, &child);
		child = next;
	}
	result->doc = NULL;
	result->offset = -1;
}

void CxmlUtilFindChildElementByIDHash(CxmlElement *result, const CxmlElement *parent,
				      const char *name, const char *idhashname, uint32_t idhashvalue)
{
	CxmlElement child, next;
	CxmlAttribute attr;
	uint32_t hash;

	if (!idhashvalue) {
		result->doc = NULL;
		result->offset = -1;
		return;
	}
	CxmlElementGetFirstChild(&child, parent);
	attr.doc = NULL;
	attr.element_offset = -1;
	attr.offset = -1;
	while (child.doc && child.offset >= 0) {
		const char *child_name = CxmlElementGetName(&child);

		if (*child_name == *name && !sce_paf_strcmp(child_name, name)) {
			if (!idhashname) {
				*result = child;
				return;
			}
			if (CxmlElementGetAttributeByName(&child, idhashname, &attr) == SCE_PAF_OK &&
			    ((CxmlAttributeGetType(&attr) == AttributeType_IDHash &&
			      CxmlAttributeGetIDHash(&attr, &hash) == SCE_PAF_OK) ||
			     (CxmlAttributeGetType(&attr) == AttributeType_Hash &&
			      CxmlAttributeGetHash(&attr, &hash) == SCE_PAF_OK)) &&
			    hash == idhashvalue) {
				*result = child;
				return;
			}
		}
		CxmlElementGetNextSibling(&next, &child);
		child = next;
	}
	result->doc = NULL;
	result->offset = -1;
}

int32_t CxmlUtilFindEmptyAttribute(const CxmlElement *elm, CxmlAttribute *attr)
{
	CxmlAttribute current = { NULL, -1, -1 };
	uint32_t i;

	for (i = 0; i < (uint32_t)CxmlElementNumAttribute(elm); i++) {
		int32_t ret = CxmlElementGetAttributeByIndex(elm, (int32_t)i, &current);

		if (ret < 0)
			return ret;
		if (CxmlAttributeGetType(&current) == AttributeType_None) {
			*attr = current;
			return SCE_PAF_OK;
		}
	}
	return SCE_PAF_ERROR_CXML_NOT_EXIST;
}

int32_t CxmlUtilGetInt(const CxmlElement *elm, const char *attrname, int32_t *value)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetInt(&attr, value);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetFloat(const CxmlElement *elm, const char *attrname, float *value)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetFloat(&attr, value);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetString(const CxmlElement *elm, const char *attrname,
			 const char **value, uint32_t *len)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetString(&attr, value, len);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetWString(const CxmlElement *elm, const char *attrname,
			  const uint16_t **value, uint32_t *len)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetWString(&attr, value, len);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetHash(const CxmlElement *elm, const char *attrname, uint32_t *value)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetHash(&attr, value);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetIntArray(const CxmlElement *elm, const char *attrname,
			   const int32_t **addr, uint32_t *num)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetIntArray(&attr, addr, num);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetFloatArray(const CxmlElement *elm, const char *attrname,
			     const float **addr, uint32_t *num)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetFloatArray(&attr, addr, num);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetFile(const CxmlElement *elm, const char *attrname, CxmlFile *file)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetFile(&attr, file);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetID(const CxmlElement *elm, const char *attrname, const char **value)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetID(&attr, value);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetIDRef(const CxmlElement *elm, const char *attrname,
			const char **value, CxmlElement *entity)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetIDRef(&attr, value, entity);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetIDHash(const CxmlElement *elm, const char *attrname, uint32_t *value)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetIDHash(&attr, value);
	return ret < 0 ? ret : SCE_PAF_OK;
}

int32_t CxmlUtilGetIDHashRef(const CxmlElement *elm, const char *attrname,
			    uint32_t *value, CxmlElement *entity)
{
	CxmlAttribute attr = { NULL, -1, -1 };
	int32_t ret = CxmlElementGetAttributeByName(elm, attrname, &attr);

	if (ret < 0)
		return ret;
	ret = CxmlAttributeGetIDHashRef(&attr, value, entity);
	return ret < 0 ? ret : SCE_PAF_OK;
}
