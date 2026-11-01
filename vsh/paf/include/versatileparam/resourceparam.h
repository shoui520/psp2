/*
 * paf::ResourceParam: the parameters of one resource element, by name, read from
 * or written as its attributes. Names after ScePaf-RE.
 */
#ifndef PAF_VERSATILEPARAM_RESOURCEPARAM_H
#define PAF_VERSATILEPARAM_RESOURCEPARAM_H

#include <stdint.h>
#include <cxml/cxml.h>
#include <std/vector>
#include <common/shared_ptr.h>

namespace paf {

class File;

namespace resource {
class ResourceObj;
}

class ResourceParam {
public:
	enum Semantic {
		SEMANTIC_INT = 0,
		SEMANTIC_INT2 = 1,
		SEMANTIC_INT3 = 2,
		SEMANTIC_INT4 = 3,
		SEMANTIC_FLOAT = 4,
		SEMANTIC_VEC2 = 5,
		SEMANTIC_VEC3 = 6,
		SEMANTIC_VEC4 = 7,
		SEMANTIC_BOOL = 8,
		SEMANTIC_STRING = 9,
		SEMANTIC_HASH = 10,
		SEMANTIC_IDREF = 11,
		SEMANTIC_IDHASHREF = 12,
		SEMANTIC_STRINGREF = 13,
		SEMANTIC_HASHREF = 14,
		SEMANTIC_WIDGETREF = 15,
		SEMANTIC_STYLEREF = 16,
		SEMANTIC_LABELREF = 17,
		SEMANTIC_TEXTUREREF = 18,
		/* WidgetParam's "sound" (name ours) */
		SEMANTIC_SOUNDREF = 19,
		/* the entries of a WidgetSoundParam (name ours) */
		SEMANTIC_SOUNDENTRYREF = 20,
		/* one ResolutionData per child element: its "id" and the attribute's floats */
		SEMANTIC_RESOARRAY = 21
	};

	enum ReadType {
		READTYPE_FROM_PARAMLIST = 0,
		READTYPE_FROM_XMLATTRIBUTE = 1
	};

	class SemanticParam {
	public:
		SemanticParam(const char *name, void *addr, Semantic type)
			: param_name(name), param_addr(addr), param_type(type)
		{
		}

		const char *param_name;
		void *param_addr;
		Semantic param_type;
	};

	ResourceParam();
	virtual ~ResourceParam();

	void AddParam(const char *name, void *addr, Semantic type);
	bool GetDirty();
	void SetDirty(bool flag);
	bool Read(const cxml::Element &elm, resource::ResourceObj *rscobj, ReadType type);
	void SetIDName(const char *idname);
	void SetTagName(const char *tagname);

	const char *m_idname;
	const char *m_tagname;
	bool m_dirty;
	vector<SemanticParam> m_param_list;
};

}

extern "C" {
/* reads the parameter from the element's attribute of its name (Sony's name not found) */
void ScePafResource_CB5A3D7C(paf::ResourceParam *param, const paf::cxml::Element &elm,
			     paf::vector<paf::ResourceParam::SemanticParam>::iterator it,
			     paf::resource::ResourceObj *rscobj);
/*
 * Writes the parameter as an attribute line at the indent; with print, to the TTY instead
 * (Sony's name not found).
 */
int32_t ScePafResource_0FF0516E(paf::ResourceParam *param,
				paf::common::SharedPtr<paf::File> fp,
				paf::vector<paf::ResourceParam::SemanticParam>::iterator it,
				uint32_t indent, int32_t print);
/*
 * Writes the parameters' element to the file at the indent: part 0 the whole element, 1 its
 * opening tag, 2 its closing tag; with print, to the TTY instead (Sony's name not found).
 */
int32_t ScePafResource_441E989F(paf::ResourceParam *param,
				paf::common::SharedPtr<paf::File> fp, int32_t part, int32_t indent,
				int32_t print);
/*
 * Writes a resource document's fixed tag at the indent: the XML header and <body>, a table's
 * opening or closing tag (0 header, 1 </body>, 2/3 pagetable, 4/5 styletable,
 * 6/7 texturetable, 8/9 templatetable); with print, to the TTY instead (Sony's name not found).
 */
int32_t ScePafResource_D74427E6(paf::common::SharedPtr<paf::File> fp, int32_t tag,
				int32_t indent, int32_t print);
}

#endif
