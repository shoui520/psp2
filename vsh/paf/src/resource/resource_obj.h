#ifndef PAF_RESOURCE_RESOURCE_OBJ_H
#define PAF_RESOURCE_RESOURCE_OBJ_H

#include "../cxml/cxml.h"

typedef enum TableType {
	TableType_Unknown = -1,
	TableType_Page = 0,
	TableType_Style = 1,
	TableType_String = 2,
	TableType_Texture = 3,
	TableType_Model = 4,
	TableType_Sound = 5,
	TableType_File = 6,
	TableType_Anim = 7,
	TableType_Shader = 8,
	TableType_Template = 9
} TableType;

typedef struct ResourceObj ResourceObj;

#define ResourceObjGetTableElement ScePafResource_A7368FD6

void ResourceObjGetTableElement(CxmlElement *result, const ResourceObj *self,
				TableType type);

#endif
