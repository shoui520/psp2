/*
 * The SDK's xml/xml_attribute.h: sce::Xml::Attr interface (Sony's names, SDK 3.570.011).
 */
#ifndef _SCE_XML_ATTRIBUTE_H
#define _SCE_XML_ATTRIBUTE_H

#include <xml/xml_result.h>
#include <xml/xml_types.h>

#ifdef __cplusplus
namespace sce
{
namespace Xml
{

class Initializer;
class AttrImpl;

class Attr
{
public:
	Attr();
	Attr(const Attr &src);
	~Attr();

	Attr &operator=(const Attr &src);

	int initialize(const Initializer *init);
	int terminate();

	String getName() const;
	String getValue() const;

	int setName(const String *str);
	int setValue(const String *str);

	bool isAvailable() const;

private:
	friend class AttrImpl;
	AttrImpl *impl;
};

} /* namespace Xml */
} /* namespace sce */
#endif

#endif /* _SCE_XML_ATTRIBUTE_H */
