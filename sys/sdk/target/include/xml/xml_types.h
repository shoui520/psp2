/*
 * The SDK's xml/xml_types.h: SceLibXml core types, allocator, initializer, and parameters
 * (Sony's names and layout, SDK 3.570.011).
 */
#ifndef _SCE_XML_TYPES_H
#define _SCE_XML_TYPES_H

#include <scetypes.h>
#include <xml/xml_result.h>
#include <xml/xml_defs.h>

#ifdef __cplusplus
namespace sce
{
namespace Xml
{

typedef SceUInt32 index_t;

#define SCE_XML_INVALID_INDEX	((index_t)-1)
const index_t invalidIndex = SCE_XML_INVALID_INDEX;

#ifndef WIN32
typedef SceUInt32 size_t;
#endif

#define SCE_XML_INVALID_SIZE	((size_t)-1)
const size_t invalidSize = SCE_XML_INVALID_SIZE;

class SimpleData
{
public:
	SimpleData();
	SimpleData(const char *data, size_t size);

	const char *buffer;
	size_t size;
};

typedef SimpleData BinaryData;

class String : public SimpleData
{
public:
	String();
	String(const String &src);
	String &operator=(const String &src);
	String(const char *str, size_t size);
	String(const char *str);

	bool isAvailable() const
	{
		return (this && buffer && size);
	}
};

typedef String XmlText;

typedef enum TokenType
{
	tokenUnknown = -1,
	tokenInit = 0,
	tokenStartTag,
	tokenEndTag,
	tokenCharData,
	tokenDtd,
	tokenDtdEnd,
	tokenPi,
	tokenPiEnd,
	tokenCdata,
	tokenCdataEnd,
	tokenComment,
	tokenCommentEnd,
	tokenUnexpected,
	tokenEscapedText,
	tokenXmlEmpty,
	tokenXmlEntity,
	tokenXmlEntityEnd
} TokenType;

typedef enum EntityType
{
	entityUnknown = 0,
	entityCharReference,
	entityBuiltInAmp,
	entityBuiltInQuot,
	entityBuiltInLt,
	entityBuiltInGt,
	entityBuiltInApos
} EntityType;

class MemAllocator
{
public:
	MemAllocator();
	virtual ~MemAllocator();

	virtual void *allocate(size_t size, void *userData) = 0;
	virtual void deallocate(void *ptr, void *userData) = 0;
};

class InitParameter
{
public:
	InitParameter()
		: allocator(0), userData(0) {}

	InitParameter(MemAllocator *al, void *ud)
		: allocator(al), userData(ud) {}

	MemAllocator *allocator;
	void *userData;
	int reserve[4];
};

class InitializerImpl;

class Initializer
{
public:
	Initializer();
	~Initializer();

	int initialize(const InitParameter *initParam);
	int terminate();

private:
	Initializer(const Initializer &);
	Initializer &operator=(Initializer &);
	friend class InitializerImpl;
	InitializerImpl *impl;
};

class SerializeParameter
{
public:
	SerializeParameter();

	int serializeOption;
	int reserve[4];
};

namespace Dom
{

typedef SceUInt64 NodeId;
typedef NodeId MetaNodeId;

#define SCE_XML_INVALID_NODE_ID	(0ULL)
const NodeId invalidNodeId = SCE_XML_INVALID_NODE_ID;

} /* namespace Dom */

} /* namespace Xml */
} /* namespace sce */

#include <xml/xml_attribute_list.h>
#include <xml/xml_node_types.h>
#include <xml/xml_node.h>
#include <xml/xml_node_list.h>
#endif

#endif /* _SCE_XML_TYPES_H */
