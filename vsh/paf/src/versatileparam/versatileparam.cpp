/*
 * paf::VersatileParam: a parameter object of a resource element, whose
 * ResourceParam reads its fields from the element and writes them back.
 */
#include <stddef.h>
#include <stdint.h>
#include <cxml/cxml.h>
#include <file/file.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <versatileparam/resourceparam.h>
#include <versatileparam/versatileparam.h>

namespace paf {

const char VersatileParam::m_param_name[] = "resource_base";

const char *VersatileParam::GetParamName() const
{
	return m_param_name;
}

const char *VersatileParam::ParamName()
{
	return m_param_name;
}

VersatileParam::VersatileParam() : m_serial(NULL), m_plugin(NULL), m_id(s_empty_name, 0)
{
	m_node = new cxml::Element;
}

VersatileParam::~VersatileParam()
{
	Destroy(-1);
	delete m_node;
}

int32_t VersatileParam::Create(int32_t mode)
{
	ResourceParam *serial = m_serial;

	if (mode & 1) {
		delete serial;
		serial = new ResourceParam;
		m_serial = serial;
	}
	if (serial != NULL) {
		serial->SetTagName(GetParamName());
		if (m_serial != NULL)
			m_serial->SetIDName(m_id.m_id.c_str());
	}
	return 0;
}

int32_t VersatileParam::Destroy(int32_t mode)
{
	if (mode & 1) {
		delete m_serial;
		m_serial = NULL;
	}
	return 0;
}

int32_t VersatileParam::OnUpdate()
{
	if (m_serial != NULL) {
		if (m_serial->GetDirty())
			Set();
		m_serial->SetDirty(false);
	}
	return 0;
}

void VersatileParam::OnDraw()
{
}

int32_t VersatileParam::Read(const cxml::Element *elem)
{
	if (m_serial == NULL)
		return -1;

	resource::ResourceObj *rscobj = NULL;

	if (m_plugin != NULL)
		rscobj = m_plugin->m_rscobj;
	if (m_serial->Read(*elem, rscobj, ResourceParam::READTYPE_FROM_PARAMLIST))
		return 0;
	return -1;
}

int32_t VersatileParam::Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent)
{
	if (m_serial == NULL)
		return -1;

	int32_t part;

	switch (type) {
	case 1:
		part = 1;
		break;
	case 2:
		part = 2;
		break;
	default:
		part = 0;
		break;
	}
	if (fp.get() != NULL)
		return ScePafResource_441E989F(m_serial, fp, part, indent, false);
	return ScePafResource_441E989F(m_serial, fp, part, indent, true);
}

}
