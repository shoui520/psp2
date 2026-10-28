#include "resource_obj.h"
#include "../cxml/util.h"

char *ResourceObjGetFileName(const ResourceObj *self)
{
	return self->filename.ptr;
}

int32_t ResourceObjGetLoadStatus(const ResourceObj *self)
{
	return self->load_status;
}

uint32_t ResourceObjGetCreateType(const ResourceObj *self)
{
	return self->create_type;
}

CxmlDocument *ResourceObjGetDocument(const ResourceObj *self)
{
	return self->document;
}

CxmlDocument *ResourceObjGetStringDocument(const ResourceObj *self)
{
	return self->string_document;
}

void ResourceObjGetTableElement(CxmlElement *result, const ResourceObj *self,
				TableType type)
{
	CxmlElement elm;
	const char *name;

	result->doc = NULL;
	result->offset = -1;
	if (!self->document)
		return;
	CxmlDocumentGetDocumentElement(&elm, self->document);
	*result = elm;
	if (result->doc && result->offset >= 0)
		CxmlUtilCheckElementName(result, "resource");
	switch (type) {
	case TableType_Page:
		name = "pagetable";
		break;
	case TableType_Style:
		name = "styletable";
		break;
	case TableType_String:
		name = "stringtable";
		break;
	case TableType_Texture:
		name = "texturetable";
		break;
	case TableType_Model:
		name = "modeltable";
		break;
	case TableType_Sound:
		name = "sounddatatable";
		break;
	case TableType_SoundIndex:
		name = "soundindextable";
		break;
	case TableType_File:
		name = "filetable";
		break;
	case TableType_Template:
		name = "templatetable";
		break;
	case TableType_Alias:
		name = "aliastable";
		break;
	default:
		return;
	}
	CxmlUtilFindChildElement(&elm, result, name);
	*result = elm;
}

unsigned char ResourceObjIsRegisteredIDHash(ResourceObj *self, uint32_t value)
{
	if (!self->document)
		return 0;
	return CxmlDocumentIsRegisteredIDHash(self->document, value);
}

int32_t ResourceObjGetStringElement(const ResourceObj *self, const CxmlElement *elm,
				    const uint16_t **src, uint32_t *length)
{
	return CxmlUtilGetWString(elm, "src", src, length);
}
