/*
 * The SDK's xml/xml_sax_document_handler.h: sce::Xml::Sax::DocumentHandler interface
 * (Sony's names, SDK 3.570.011).
 */
#ifndef _SCE_XML_SAX_DOCUMENT_HANDLER_H
#define _SCE_XML_SAX_DOCUMENT_HANDLER_H

#include <xml/xml_types.h>

#if defined(__SNC__)
#pragma diag_push
#pragma diag_suppress=828
#endif

#ifdef __cplusplus
namespace sce
{
namespace Xml
{
namespace Sax
{

class DocumentHandler
{
public:
	DocumentHandler() {}
	virtual ~DocumentHandler() {}

	virtual int startDocument(void *userData)
	{
		(void)userData;
		return 0;
	}

	virtual int startElement(void *userData, const String *name, const AttributeList *attrList)
	{
		(void)userData;
		(void)name;
		(void)attrList;
		return 0;
	}

	virtual int characters(void *userData, const String *ch)
	{
		(void)userData;
		(void)ch;
		return 0;
	}

	virtual int endElement(void *userData, const String *name)
	{
		(void)userData;
		(void)name;
		return 0;
	}

	virtual int endDocument(void *userData)
	{
		(void)userData;
		return 0;
	}

	virtual int skippedText(void *userData, TokenType tokenType, const String *text)
	{
		(void)userData;
		(void)tokenType;
		(void)text;
		return 0;
	}

	virtual int entityData(void *userData, EntityType entityType, const String *name)
	{
		(void)userData;
		(void)entityType;
		(void)name;
		return 0;
	}

	virtual void fatalError(void *userData, int errCode)
	{
		(void)userData;
		(void)errCode;
	}
};

} /* namespace Sax */
} /* namespace Xml */
} /* namespace sce */
#endif

#if defined(__SNC__)
#pragma diag_pop
#endif

#endif /* _SCE_XML_SAX_DOCUMENT_HANDLER_H */
