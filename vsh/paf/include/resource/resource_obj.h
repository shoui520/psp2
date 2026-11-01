/*
 * paf::resource::ResourceObj, as far as converted C++ uses it: a plugin's
 * resource (RCO), its tables by type. Names after ScePaf-RE; the class itself
 * is still C (src/resource/resource_obj.h).
 */
#ifndef PAF_RESOURCE_RESOURCE_OBJ_H
#define PAF_RESOURCE_RESOURCE_OBJ_H

#include <stdint.h>
#include <cxml/cxml.h>

namespace paf {
namespace resource {

enum TableType {
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
};

class ResourceObj {
public:
	cxml::Element GetTableElement(TableType type) const;
};

}
}

#endif
