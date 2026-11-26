/*
 * The SDK's xml/xml_debug.h: sce::Xml::Dom::DocumentDebug interface
 * (Sony's names, SDK 3.570.011).
 */
#ifndef _SCE_XML_DEBUG_H
#define _SCE_XML_DEBUG_H

#include <xml/xml_types.h>

#ifdef __cplusplus
namespace sce
{
namespace Xml
{
namespace Dom
{

class Document;

class DocumentDebug
{
	DocumentDebug(Document *doc);
	~DocumentDebug();

	size_t getStructSize() const;
	size_t getCharTableSize() const;
	size_t getAttrTableSize() const;
	size_t getElementTableSize() const;

private:
	DocumentDebug(const DocumentDebug &);
	DocumentDebug &operator=(const DocumentDebug &);
};

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
#endif

#endif /* _SCE_XML_DEBUG_H */
