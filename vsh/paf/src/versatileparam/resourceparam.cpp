/*
 * paf::ResourceParam: the parameters of a resource element by name, each read from
 * the element's attribute of that name and written back as one.
 */
#include <stddef.h>
#include <stdint.h>
#include <cxml/cxml.h>
#include <file/file.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <versatileparam/idparam.h>
#include <versatileparam/resourceparam.h>
#include <versatileparam/versatileparam.h>
#include <widget/core/widget.h>

using namespace paf;

/* writes num spaces, one at a time; on a short write, returns what the write returned */
#define WRITE_SPACES(fp, num, print)						\
	for (uint32_t i = 0; i < (num); i++) {					\
		if (!(print)) {							\
			int32_t written = (fp)->Write(" ", 1);			\
										\
			if (written != 1) {					\
				sce_paf_printf_dummy();				\
				return written;					\
			}							\
		} else {							\
			sce_paf_printf_dummy();					\
		}								\
	}

/* writes the line of len bytes; on a short write, returns what the write returned */
#define WRITE_LINE(fp, line, len, print)					\
	do {									\
		if (!(print)) {							\
			int32_t written = (fp)->Write(line, len);		\
										\
			if (written != (len)) {					\
				sce_paf_printf_dummy();				\
				return written;					\
			}							\
		} else {							\
			sce_paf_printf_dummy();					\
		}								\
	} while (0)

namespace paf {

void ResourceParam::AddParam(const char *name, void *addr, Semantic type)
{
	m_param_list.push_back(SemanticParam(name, addr, type));
}

}

void ScePafResource_CB5A3D7C(ResourceParam *param, const cxml::Element &elm,
			     vector<ResourceParam::SemanticParam>::iterator it,
			     resource::ResourceObj *rscobj)
{
	switch (it->param_type) {
	case ResourceParam::SEMANTIC_INT:
		scePafResourceGetAttributeInteger(&elm, it->param_name, (int32_t *)it->param_addr);
		break;
	case ResourceParam::SEMANTIC_INT2: {
		const int32_t *addr;
		uint32_t num;

		if (scePafResourceGetAttributeIntegerArray(&elm, it->param_name, &addr, &num) == 0) {
			if (num != 0) {
				((int32_t *)it->param_addr)[0] = addr[0];
				num--;
			} else {
				((int32_t *)it->param_addr)[0] = 0;
			}
			if (num != 0)
				((int32_t *)it->param_addr)[1] = addr[1];
			else
				((int32_t *)it->param_addr)[1] = 0;
		}
		break;
	}
	case ResourceParam::SEMANTIC_INT3: {
		const int32_t *addr;
		uint32_t num;

		if (scePafResourceGetAttributeIntegerArray(&elm, it->param_name, &addr, &num) == 0) {
			if (num != 0) {
				((int32_t *)it->param_addr)[0] = addr[0];
				num--;
			} else {
				((int32_t *)it->param_addr)[0] = 0;
			}
			if (num != 0) {
				((int32_t *)it->param_addr)[1] = addr[1];
				num--;
			} else {
				((int32_t *)it->param_addr)[1] = 0;
			}
			if (num != 0)
				((int32_t *)it->param_addr)[2] = addr[2];
			else
				((int32_t *)it->param_addr)[2] = 0;
		}
		break;
	}
	case ResourceParam::SEMANTIC_INT4: {
		const int32_t *addr;
		uint32_t num;

		if (scePafResourceGetAttributeIntegerArray(&elm, it->param_name, &addr, &num) == 0) {
			if (num != 0) {
				((int32_t *)it->param_addr)[0] = addr[0];
				num--;
			} else {
				((int32_t *)it->param_addr)[0] = 0;
			}
			if (num != 0) {
				((int32_t *)it->param_addr)[1] = addr[1];
				num--;
			} else {
				((int32_t *)it->param_addr)[1] = 0;
			}
			if (num != 0) {
				((int32_t *)it->param_addr)[2] = addr[2];
				num--;
			} else {
				((int32_t *)it->param_addr)[2] = 0;
			}
			if (num != 0)
				((int32_t *)it->param_addr)[3] = addr[3];
			else
				((int32_t *)it->param_addr)[3] = 0;
		}
		break;
	}
	case ResourceParam::SEMANTIC_FLOAT:
		scePafResourceGetAttributeFloat(&elm, it->param_name, (float *)it->param_addr);
		break;
	case ResourceParam::SEMANTIC_VEC2: {
		const float *addr;
		uint32_t num;

		if (scePafResourceGetAttributeFloatArray(&elm, it->param_name, &addr, &num) == 0) {
			if (num != 0) {
				((math::v2 *)it->param_addr)->set_x(addr[0]);
				num--;
			}
			if (num != 0)
				((math::v2 *)it->param_addr)->set_y(addr[1]);
		}
		break;
	}
	case ResourceParam::SEMANTIC_VEC3: {
		const float *addr;
		uint32_t num;

		if (scePafResourceGetAttributeFloatArray(&elm, it->param_name, &addr, &num) == 0 &&
		    num != 0) {
			((math::v3 *)it->param_addr)->set_x(addr[0]);
			if (--num != 0) {
				((math::v3 *)it->param_addr)->set_y(addr[1]);
				if (--num != 0)
					((math::v3 *)it->param_addr)->set_z(addr[2]);
			}
		}
		break;
	}
	case ResourceParam::SEMANTIC_VEC4: {
		const float *addr;
		uint32_t num;

		if (scePafResourceGetAttributeFloatArray(&elm, it->param_name, &addr, &num) == 0 &&
		    num != 0) {
			((math::v4 *)it->param_addr)->set_x(addr[0]);
			if (--num != 0) {
				((math::v4 *)it->param_addr)->set_y(addr[1]);
				if (--num != 0) {
					((math::v4 *)it->param_addr)->set_z(addr[2]);
					if (--num != 0)
						((math::v4 *)it->param_addr)->set_w(addr[3]);
				}
			}
		}
		break;
	}
	case ResourceParam::SEMANTIC_BOOL: {
		int32_t value;

		if (scePafResourceGetAttributeInteger(&elm, it->param_name, &value) == 0) {
			switch (value) {
			case 0:
				*(bool *)it->param_addr = false;
				break;
			case 1:
				*(bool *)it->param_addr = true;
				break;
			}
		}
		break;
	}
	case ResourceParam::SEMANTIC_STRING: {
		const char *value;
		uint32_t len;

		if (scePafResourceGetAttributeString(&elm, it->param_name, &value, &len) == 0)
			*(const char **)it->param_addr = value;
		break;
	}
	case ResourceParam::SEMANTIC_STRINGREF: {
		const char *value;
		uint32_t len;

		if (scePafResourceGetAttributeString(&elm, it->param_name, &value, &len) == 0) {
			VersatileParam *ref = (VersatileParam *)it->param_addr;

			if (value == NULL)
				ref->m_id.m_idhash = 0;
			else
				ref->m_id.SetID(string(value));

			cxml::Element node =
				cxml::util::FindChildElement(elm, ref->GetParamName(), "id", value);

			if (elm.doc != NULL && elm.offset >= 0)
				*ref->m_node = node;
		}
		break;
	}
	case ResourceParam::SEMANTIC_HASHREF: {
		uint32_t value = 0;

		if (scePafResourceGetAttributeStyleId(&elm, it->param_name, &value) == 0) {
			VersatileParam *ref = (VersatileParam *)it->param_addr;

			ref->m_id.m_idhash = value;

			cxml::Element node =
				cxml::util::FindChildElement(elm, ref->GetParamName(), "id", value);

			if (elm.doc != NULL && elm.offset >= 0)
				*ref->m_node = node;
		}
		break;
	}
	case ResourceParam::SEMANTIC_IDREF: {
		const char *value;
		cxml::Element entity;

		if (scePafResourceGetAttributeIdStr(&elm, it->param_name, &value, &entity) == 0)
			((IDParam *)it->param_addr)->SetID(string(value));
		break;
	}
	case ResourceParam::SEMANTIC_IDHASHREF:
	case ResourceParam::SEMANTIC_STYLEREF:
	case ResourceParam::SEMANTIC_LABELREF:
	case ResourceParam::SEMANTIC_TEXTUREREF:
	case ResourceParam::SEMANTIC_SOUNDREF:
	case ResourceParam::SEMANTIC_SOUNDENTRYREF: {
		uint32_t value = 0;
		cxml::Element entity;

		if (scePafResourceGetAttributeIdInt(&elm, it->param_name, &value, &entity) == 0)
			((IDParam *)it->param_addr)->m_idhash = value;
		break;
	}
	case ResourceParam::SEMANTIC_HASH:
	case ResourceParam::SEMANTIC_WIDGETREF: {
		uint32_t value = 0;

		if (scePafResourceGetAttributeStyleId(&elm, it->param_name, &value) == 0)
			((IDParam *)it->param_addr)->m_idhash = value;
		break;
	}
	case ResourceParam::SEMANTIC_RESOARRAY: {
		cxml::Element child;
		ui::ResolutionData *data = (ui::ResolutionData *)it->param_addr;

		for (cxml::Element next = elm.GetFirstChild(); next.doc != NULL && next.offset >= 0;
		     next = child.GetNextSibling()) {
			uint32_t id = 0;
			const float *addr;
			uint32_t num;

			child = next;
			if (scePafResourceGetAttributeStyleId(&child, "id", &id) != 0) {
				data->m_id = 0;
				break;
			}

			IDParam reso(s_empty_id, id);

			if (scePafResourceGetAttributeFloatArray(&child, it->param_name, &addr, &num) != 0) {
				data->m_id = 0;
			} else {
				data->m_id = id;
				if (num != 0) {
					data->m_value[0] = addr[0];
					if (--num != 0) {
						data->m_value[1] = addr[1];
						if (--num != 0) {
							data->m_value[2] = addr[2];
							num--;
						}
					}
				}
			}
			data++;
		}
		break;
	}
	default:
		sce_paf_printf_dummy();
		break;
	}
}

namespace paf {

bool ResourceParam::Read(const cxml::Element &elm, resource::ResourceObj *rscobj, ReadType type)
{
	int32_t num = elm.NumAttribute();

	switch (type) {
	case READTYPE_FROM_PARAMLIST:
		for (vector<SemanticParam>::iterator it = m_param_list.begin();
		     it != m_param_list.end(); ++it) {
			if (it->param_type == SEMANTIC_RESOARRAY) {
				ScePafResource_CB5A3D7C(this, elm, it, rscobj);
				continue;
			}
			for (uint32_t i = 0; i < (uint32_t)num; i++) {
				cxml::Attribute attr;

				if (elm.GetAttribute(i, &attr) == 0 &&
				    sce_paf_strcmp(attr.GetName(), it->param_name) == 0) {
					ScePafResource_CB5A3D7C(this, elm, it, rscobj);
					break;
				}
			}
		}
		break;
	case READTYPE_FROM_XMLATTRIBUTE:
		for (uint32_t i = 0; i < (uint32_t)num; i++) {
			cxml::Attribute attr;

			if (elm.GetAttribute(i, &attr) != 0)
				continue;

			vector<SemanticParam>::iterator it = m_param_list.begin();

			if (it->param_type == SEMANTIC_RESOARRAY) {
				ScePafResource_CB5A3D7C(this, elm, it, rscobj);
				continue;
			}
			for (; it != m_param_list.end(); ++it) {
				if (sce_paf_strcmp(attr.GetName(), it->param_name) == 0) {
					ScePafResource_CB5A3D7C(this, elm, it, rscobj);
					break;
				}
			}
		}
		break;
	default:
		return false;
	}
	return true;
}

}

int32_t ScePafResource_0FF0516E(ResourceParam *param, common::SharedPtr<File> fp,
				vector<ResourceParam::SemanticParam>::iterator it, uint32_t indent,
				int32_t print)
{
	char line[256];
	int32_t len;

	switch (it->param_type) {
	case ResourceParam::SEMANTIC_INT:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%d\"\n", it->param_name,
				       ((int32_t *)it->param_addr)[0]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_INT2:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%d, %d\"\n", it->param_name,
				       ((int32_t *)it->param_addr)[0], ((int32_t *)it->param_addr)[1]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_INT3:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%d, %d, %d\"\n", it->param_name,
				       ((int32_t *)it->param_addr)[0], ((int32_t *)it->param_addr)[1],
				       ((int32_t *)it->param_addr)[2]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_INT4:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%d, %d, %d, %d\"\n", it->param_name,
				       ((int32_t *)it->param_addr)[0], ((int32_t *)it->param_addr)[1],
				       ((int32_t *)it->param_addr)[2], ((int32_t *)it->param_addr)[3]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_FLOAT:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%.3f\"\n", it->param_name,
				       (double)((float *)it->param_addr)[0]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_VEC2:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%.3f, %.3f\"\n", it->param_name,
				       (double)((float *)it->param_addr)[0],
				       (double)((float *)it->param_addr)[1]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_VEC3:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%.3f, %.3f, %.3f\"\n",
				       it->param_name, (double)((float *)it->param_addr)[0],
				       (double)((float *)it->param_addr)[1],
				       (double)((float *)it->param_addr)[2]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_VEC4:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%.3f, %.3f, %.3f, %.3f\"\n",
				       it->param_name, (double)((float *)it->param_addr)[0],
				       (double)((float *)it->param_addr)[1],
				       (double)((float *)it->param_addr)[2],
				       (double)((float *)it->param_addr)[3]);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_BOOL:
		if (*(bool *)it->param_addr) {
			if (indent != 0)
				WRITE_SPACES(fp, indent * 4 + 4, print);
			len = sce_paf_snprintf(line, sizeof(line), "%s=\"%d\"\n", it->param_name, 1);
			WRITE_LINE(fp, line, len, print);
		} else {
			if (indent != 0)
				WRITE_SPACES(fp, indent * 4 + 4, print);
			len = sce_paf_snprintf(line, sizeof(line), "%s=\"%d\"\n", it->param_name, 0);
			WRITE_LINE(fp, line, len, print);
		}
		break;
	case ResourceParam::SEMANTIC_STRING:
		if (*(const char **)it->param_addr == NULL)
			break;
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%s\"\n", it->param_name,
				       *(const char **)it->param_addr);
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_STRINGREF:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%s\"\n", it->param_name,
				       ((VersatileParam *)it->param_addr)->m_id.m_id.c_str());
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_HASHREF:
		if (((VersatileParam *)it->param_addr)->m_id.m_idhash == 0)
			break;
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%s\"\n", it->param_name,
				       ((VersatileParam *)it->param_addr)->m_id.m_id.c_str());
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_IDREF: {
		string id = ((IDParam *)it->param_addr)->m_id;

		if (id == "")
			break;
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%s\"\n", it->param_name,
				       ((IDParam *)it->param_addr)->m_id.c_str());
		WRITE_LINE(fp, line, len, print);
		break;
	}
	case ResourceParam::SEMANTIC_IDHASHREF:
	case ResourceParam::SEMANTIC_STYLEREF:
	case ResourceParam::SEMANTIC_LABELREF:
	case ResourceParam::SEMANTIC_TEXTUREREF:
	case ResourceParam::SEMANTIC_SOUNDREF:
	case ResourceParam::SEMANTIC_SOUNDENTRYREF:
		if (((IDParam *)it->param_addr)->m_idhash == 0)
			break;
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%s\"\n", it->param_name,
				       ((IDParam *)it->param_addr)->m_id.c_str());
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_HASH:
	case ResourceParam::SEMANTIC_WIDGETREF:
		if (((IDParam *)it->param_addr)->m_idhash == 0)
			break;
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%s\"\n", it->param_name,
				       ((IDParam *)it->param_addr)->m_id.c_str());
		WRITE_LINE(fp, line, len, print);
		break;
	case ResourceParam::SEMANTIC_RESOARRAY:
		if (indent != 0)
			WRITE_SPACES(fp, indent * 4 + 4, print);
		len = sce_paf_snprintf(line, sizeof(line), "%s=\"%.3f, %.3f, %.3f\"\n",
				       it->param_name,
				       (double)((ui::ResolutionData *)it->param_addr)->m_value[0],
				       (double)((ui::ResolutionData *)it->param_addr)->m_value[1],
				       (double)((ui::ResolutionData *)it->param_addr)->m_value[2]);
		WRITE_LINE(fp, line, len, print);
		break;
	default:
		sce_paf_printf_dummy();
		return -1;
	}
	return 0;
}

int32_t ScePafResource_441E989F(ResourceParam *param, common::SharedPtr<File> fp, int32_t part,
				int32_t indent, int32_t print)
{
	char open[256];
	char id[256];
	char close[256];
	int32_t len;

	switch (part) {
	case 0:
	case 1:
		if (sce_paf_strlen(param->m_tagname) == 0)
			goto no_tag;
		if (indent != 0)
			WRITE_SPACES(fp, (uint32_t)indent * 4, print);
		len = sce_paf_snprintf(open, sizeof(open), "<%s\n", param->m_tagname);
		WRITE_LINE(fp, open, len, print);
		if (sce_paf_strlen(param->m_idname) != 0) {
			if (indent != 0)
				WRITE_SPACES(fp, (uint32_t)indent * 4 + 4, print);
			len = sce_paf_snprintf(id, sizeof(id), "id=\"%s\"\n", param->m_idname);
			WRITE_LINE(fp, id, len, print);
		}
		if (param->m_param_list.size() != 0) {
			for (vector<ResourceParam::SemanticParam>::iterator it =
				     param->m_param_list.begin();
			     it != param->m_param_list.end(); ++it) {
				int32_t ret = ScePafResource_0FF0516E(param, fp, it, indent, print);

				if (ret != 0) {
					sce_paf_printf_dummy();
					return ret;
				}
			}
		}
		if (part != 0) {
			char end[] = ">\n";

			if (indent != 0)
				WRITE_SPACES(fp, (uint32_t)indent * 4 + 4, print);
			WRITE_LINE(fp, end, 2, print);
		} else {
			char end[] = "/>\n";

			if (indent != 0)
				WRITE_SPACES(fp, (uint32_t)indent * 4, print);
			WRITE_LINE(fp, end, 3, print);
		}
		break;
	case 2:
		if (sce_paf_strlen(param->m_tagname) == 0) {
			sce_paf_printf_dummy();
			return -1;
		}
		if (indent != 0)
			WRITE_SPACES(fp, (uint32_t)indent * 4, print);
		len = sce_paf_snprintf(close, sizeof(close), "</%s>\n", param->m_tagname);
		WRITE_LINE(fp, close, len, print);
		break;
	default:
		return -1;
	}
	return 0;

no_tag:
	sce_paf_printf_dummy();
	return -1;
}

int32_t ScePafResource_D74427E6(common::SharedPtr<File> fp, int32_t tag, int32_t indent,
				int32_t print)
{
	if (indent != 0)
		WRITE_SPACES(fp, (uint32_t)indent * 4, print);

	switch (tag) {
	case 0: {
		char header[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\n";
		char body[] = "<body>\n";

		if (!print) {
			WRITE_LINE(fp, header, sizeof(header) - 1, false);
			WRITE_LINE(fp, body, sizeof(body) - 1, false);
		} else {
			sce_paf_printf_dummy();
			sce_paf_printf_dummy();
		}
		break;
	}
	case 1: {
		char body[] = "</body>\n";

		WRITE_LINE(fp, body, sizeof(body) - 1, print);
		break;
	}
	case 2: {
		char table[] = "<pagetable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	case 3: {
		char table[] = "</pagetable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	case 4: {
		char table[] = "<styletable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	case 5: {
		char table[] = "</styletable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	case 6: {
		char table[] = "<texturetable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	case 7: {
		char table[] = "</texturetable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	case 8: {
		char table[] = "<templatetable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	case 9: {
		char table[] = "</templatetable>\n";

		WRITE_LINE(fp, table, sizeof(table) - 1, print);
		break;
	}
	default:
		return -1;
	}
	return 0;
}

namespace paf {

void ResourceParam::SetIDName(const char *idname)
{
	m_idname = idname;
}

void ResourceParam::SetTagName(const char *tagname)
{
	m_tagname = tagname;
}

bool ResourceParam::GetDirty()
{
	return m_dirty;
}

void ResourceParam::SetDirty(bool flag)
{
	m_dirty = flag;
}

ResourceParam::ResourceParam() : m_idname(""), m_tagname(""), m_dirty(false)
{
	m_param_list.reserve(48);
}

ResourceParam::~ResourceParam()
{
}

}
