/*
 * paf::common::property_list: elements and values kept by index and by ID. An
 * element is a reference-counted object that says which types it inherits by
 * the addresses of their type names; a value is eight plain bytes.
 */
#ifndef PAF_COMMON_PROPERTY_LIST_H
#define PAF_COMMON_PROPERTY_LIST_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <common/shared_ptr.h>
#include <versatileparam/idparam.h>

namespace paf {
namespace common {
namespace property_list {

class PropertyElement {
public:
	virtual bool IsInherit(const char *type_name) const;
	virtual const char *GetTypeName() const;

	virtual ~PropertyElement()
	{
	}

	static const char *TypeName();

	static const char m_type_name[];
};

class StringElement : public PropertyElement {
public:
	StringElement(const string &str) : m_string(str)
	{
	}

	virtual bool IsInherit(const char *type_name) const;
	virtual const char *GetTypeName() const;

	virtual ~StringElement()
	{
	}

	static const char *TypeName();
	static const string &GetString(SharedPtr<PropertyElement> element);

	static const char m_type_name[];
	static string s_empty_string;

	string m_string;
};

class WStringElement : public PropertyElement {
public:
	WStringElement(const wstring &str) : m_string(str)
	{
	}

	virtual bool IsInherit(const char *type_name) const;
	virtual const char *GetTypeName() const;

	virtual ~WStringElement()
	{
	}

	static const char *TypeName();
	static const wstring &GetString(SharedPtr<PropertyElement> element);

	static const char m_type_name[];
	static wstring s_empty_string;

	wstring m_string;
};

class PropertyValue {
public:
	PropertyValue() : m_value(0)
	{
	}

	PropertyValue(const PropertyValue &src) : m_value(src.m_value)
	{
	}

	union {
		int64_t m_value;
		void *m_pointer;
	};
};

class PropertyList {
public:
	void SetElement(const IDParam &id, SharedPtr<PropertyElement> element);
	void SetElement(uint32_t index, SharedPtr<PropertyElement> element);
	void SetString(const IDParam &id, const string &str);
	void SetString(uint32_t index, const string &str);
	void SetWString(const IDParam &id, const wstring &str);
	void SetWString(uint32_t index, const wstring &str);
	void SetValue(const IDParam &id, PropertyValue value);
	void SetValue(uint32_t index, PropertyValue value);

	SharedPtr<PropertyElement> GetElement(const IDParam &id) const;
	SharedPtr<PropertyElement> GetElement(uint32_t index) const;
	const string &GetString(const IDParam &id) const;
	const string &GetString(uint32_t index) const;
	const wstring &GetWString(const IDParam &id) const;
	const wstring &GetWString(uint32_t index) const;
	PropertyValue GetValue(const IDParam &id) const;
	PropertyValue GetValue(uint32_t index) const;

	bool HasElement(const IDParam &id) const;
	bool HasValue(const IDParam &id) const;

	vector<SharedPtr<PropertyElement> > m_element_list;
	map<IDParam, SharedPtr<PropertyElement> > m_element_map;
	vector<PropertyValue> m_value_list;
	map<IDParam, PropertyValue> m_value_map;
};

}
}
}

#endif
