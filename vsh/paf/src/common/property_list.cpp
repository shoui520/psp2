/*
 * paf::common::property_list: an element inherits a type when it is handed
 * that type's own m_type_name; a missing string reads as the empty one.
 */
#include <common/property_list.h>

namespace paf {
namespace common {
namespace property_list {

const char PropertyElement::m_type_name[] = "property_element";

bool PropertyElement::IsInherit(const char *type_name) const
{
	return type_name == m_type_name;
}

const char *PropertyElement::GetTypeName() const
{
	return m_type_name;
}

const char *PropertyElement::TypeName()
{
	return m_type_name;
}

const char StringElement::m_type_name[] = "string_element";

bool StringElement::IsInherit(const char *type_name) const
{
	if (type_name == m_type_name)
		return true;
	return PropertyElement::IsInherit(type_name);
}

const char *StringElement::GetTypeName() const
{
	return m_type_name;
}

const char *StringElement::TypeName()
{
	return m_type_name;
}

const string &StringElement::GetString(SharedPtr<PropertyElement> element)
{
	StringElement *str = NULL;

	if (element.get() != NULL && element->IsInherit(m_type_name))
		str = (StringElement *)element.get();
	if (str != NULL)
		return str->m_string;
	return s_empty_string;
}

const char WStringElement::m_type_name[] = "wstring_element";

bool WStringElement::IsInherit(const char *type_name) const
{
	if (type_name == m_type_name)
		return true;
	return PropertyElement::IsInherit(type_name);
}

const char *WStringElement::GetTypeName() const
{
	return m_type_name;
}

const char *WStringElement::TypeName()
{
	return m_type_name;
}

const wstring &WStringElement::GetString(SharedPtr<PropertyElement> element)
{
	WStringElement *str = NULL;

	if (element.get() != NULL && element->IsInherit(m_type_name))
		str = (WStringElement *)element.get();
	if (str != NULL)
		return str->m_string;
	return s_empty_string;
}

void PropertyList::SetElement(const IDParam &id, SharedPtr<PropertyElement> element)
{
	m_element_map[id] = element;
}

void PropertyList::SetElement(uint32_t index, SharedPtr<PropertyElement> element)
{
	if (index >= m_element_list.size())
		m_element_list.resize(index + 1);
	m_element_list[index] = element;
}

void PropertyList::SetString(const IDParam &id, const string &str)
{
	SetElement(id, SharedPtr<PropertyElement>(new StringElement(str)));
}

void PropertyList::SetString(uint32_t index, const string &str)
{
	SetElement(index, SharedPtr<PropertyElement>(new StringElement(str)));
}

void PropertyList::SetWString(const IDParam &id, const wstring &str)
{
	SetElement(id, SharedPtr<PropertyElement>(new WStringElement(str)));
}

void PropertyList::SetWString(uint32_t index, const wstring &str)
{
	SetElement(index, SharedPtr<PropertyElement>(new WStringElement(str)));
}

void PropertyList::SetValue(const IDParam &id, PropertyValue value)
{
	m_value_map[id] = value;
}

void PropertyList::SetValue(uint32_t index, PropertyValue value)
{
	if (index >= m_value_list.size())
		m_value_list.resize(index + 1);
	m_value_list[index] = value;
}

SharedPtr<PropertyElement> PropertyList::GetElement(const IDParam &id) const
{
	map<IDParam, SharedPtr<PropertyElement> >::iterator it = m_element_map.find(id);

	if (it == m_element_map.end())
		return SharedPtr<PropertyElement>();
	return it->second;
}

/* Gives an element only for an index past the end. */
SharedPtr<PropertyElement> PropertyList::GetElement(uint32_t index) const
{
	if (m_element_list.size() >= index)
		return SharedPtr<PropertyElement>();
	return m_element_list[index];
}

const string &PropertyList::GetString(const IDParam &id) const
{
	return StringElement::GetString(GetElement(id));
}

const string &PropertyList::GetString(uint32_t index) const
{
	return StringElement::GetString(GetElement(index));
}

const wstring &PropertyList::GetWString(const IDParam &id) const
{
	return WStringElement::GetString(GetElement(id));
}

const wstring &PropertyList::GetWString(uint32_t index) const
{
	return WStringElement::GetString(GetElement(index));
}

PropertyValue PropertyList::GetValue(const IDParam &id) const
{
	map<IDParam, PropertyValue>::iterator it = m_value_map.find(id);

	if (it == m_value_map.end())
		return PropertyValue();
	return it->second;
}

PropertyValue PropertyList::GetValue(uint32_t index) const
{
	if (index < m_value_list.size())
		return m_value_list[index];
	return PropertyValue();
}

bool PropertyList::HasElement(const IDParam &id) const
{
	return m_element_map.find(id) != m_element_map.end();
}

bool PropertyList::HasValue(const IDParam &id) const
{
	return m_value_map.find(id) != m_value_map.end();
}

/* a word of the file's data ahead of the empty strings, nothing in this module reads it (name
   after the NID) */
extern "C" {
uint32_t ScePafCommon_55A149D9;
}

string StringElement::s_empty_string;
wstring WStringElement::s_empty_string;

}
}
}
