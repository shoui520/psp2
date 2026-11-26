/*
 * The SDK's xml/xml_attribute_list.h: sce::Xml::AttributeList interface
 * (Sony's names, SDK 3.570.011).
 */
#ifndef _SCE_XML_ATTRIBUTE_LIST_H
#define _SCE_XML_ATTRIBUTE_LIST_H

#include <xml/xml_attribute.h>
#include <xml/xml_types.h>

#ifdef __cplusplus
namespace sce
{
namespace Xml
{

class Attr;
class Initializer;
class AttributeListImpl;

class AttributeList
{
public:
	AttributeList();
	AttributeList(const AttributeList &src);
	~AttributeList();

	AttributeList &operator=(const AttributeList &src);

	int initialize(const Initializer *init);
	int terminate();

	Attr getAttribute(const String *attName) const;
	Attr getAttribute(index_t index) const;
	size_t getLength() const;

	int addAttribute(const String *name, const String *value);
	void clear();

	bool isAvailable() const;

protected:
	friend class AttributeListImpl;
	AttributeListImpl *impl;
};

} /* namespace Xml */
} /* namespace sce */
#endif

#endif /* _SCE_XML_ATTRIBUTE_LIST_H */
