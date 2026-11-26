/*
 * libSceXml_internal.h: internal C++ implementation classes and bXCe R/W DOM
 * extensions for SceLibXml (vs0:sys/external/libSceXml.suprx).
 */
#ifndef _LIBSCEXML_INTERNAL_H
#define _LIBSCEXML_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <xml/xml.h>
#include <xml/xml_debug.h>
#include <xml/xml_sax_document_handler.h>
#include "../bXCe/bxce_internal.h"

/* Placement operator new (matches 0x8100bd14) */
inline void *operator new(size_t, void *ptr) throw()
{
	return ptr;
}

/* Additional bXCe error code mapped by bXResultToResultType */
#define SCE_BXCE_ERROR_NOT_SUPPORTED			(-2138759164)	/* 0x80852004 */
#define SCE_BXCE_ERROR_DOM_NOT_ALLOWED_NODE_TYPE	(-2138758641)	/* 0x8085220F */

#ifdef __cplusplus
extern "C" {
#endif

/* bXCe extensions unique to libSceXml (bxce_ext.cpp) */
int bxceStrncpySafe(void *dst, SceSize maxLen, const char *src, SceSize len);
int bxceAttrClone(BxceAttr *dst, const BxceAttr *src);
int bxceAttributeListCloneInit(BxceAttributeList *list);
int bxceNodeListInitInvalid(BxceNodeList *list);
int bxceNodeListCloneInvalid(BxceNodeList *list);
int bxceDocumentCollectByTagNameLen(const BxceDocument *doc, BxceNodeId node,
				    const char *name, SceSize nameLen, BxceNodeList *list);
int bxceDocumentGetElementsByTagNameStatus(const BxceDocument *doc, BxceNodeId node,
					   const char *name, SceSize nameLen,
					   BxceNodeList **outList);
void bxceDocumentConvertToDom(BxceDocument *doc, BxceNodeId bxNode, BxceDomNode *parentDom);
void bxceDocumentConvertToBx(BxceDocument *doc, BxceDomNode *domNode, BxceDocument *dstDoc,
			     SceUInt32 elemNameBase, SceUInt32 textBase,
			     SceUInt32 attrNameBase, SceUInt32 *childLenPos, SceUInt32 *writePos);
BxceNodeId bxceDocumentGetLastChild(const BxceDocument *doc, BxceNodeId node);
int bxceDocumentWriteDomAttrs(BxceDocument *doc, BxceDomNode *node,
			      const BxceAttributeList *attrs);
BxceNodeId bxceDocumentGetXmlMeta(const BxceDocument *doc);
int bxceDocumentMergeTables(BxceDocument *dst, const BxceDocument *src);
int bxceDocumentImportParent(BxceDocument *doc, BxceDocument *parentDoc, BxceNodeId parent);
BxceNodeId bxceDocumentSetWritable(BxceDocument *doc);
int bxceDocumentSetText(BxceDocument *doc, BxceNodeId node, const char *text, SceUInt32 length);
int bxceDocumentSetAttribute(BxceDocument *doc, BxceNodeId node, const char *name,
			     const char *value, SceUInt32 valueLen);
int bxceDocumentSetAttrValue(BxceDocument *doc, BxceNodeId node, const char *name,
			     const char *value, SceUInt32 valueLen);
int bxceDocumentSetAttributeList(BxceDocument *doc, BxceNodeId node,
				 const BxceAttributeList *attrs);
int bxceDocumentRemoveAttribute(BxceDocument *doc, BxceNodeId node, const char *name);
int bxceDocumentRemoveAttributes(BxceDocument *doc, BxceNodeId node);
bool bxceDocumentHasChildNodes(const BxceDocument *doc, BxceNodeId node);
bool bxceDocumentHasAttributes(const BxceDocument *doc, BxceNodeId node);
BxceNodeList *bxceDocumentGetAttributes(const BxceDocument *doc, BxceNodeId node);

#ifdef __cplusplus
}
#endif

namespace sce
{
namespace Xml
{

/* Exported result translation and debug-level functions */
int bXResultToResultType(int bxResult);
int setMemManagerDebugLevel(int level);
int getMemManagerDebugLevel(void);

/* Initializer implementation: stores a copy of InitParameter (24 bytes) */
class InitializerImpl
{
public:
	InitializerImpl() : param() {}

	static InitializerImpl *get(const Initializer *init)
	{
		return init != NULL ? init->impl : NULL;
	}

	static void set(Initializer *init, InitializerImpl *implPtr)
	{
		init->impl = implPtr;
	}

	InitParameter param;
};

/* Base class for objects holding a pointer to the active Initializer (8 bytes) */
class InitializerUser
{
public:
	InitializerUser(const Initializer *init) : initializer(init) {}
	virtual ~InitializerUser() {}

	const InitParameter *getInitParam() const;

	const Initializer *initializer;
};

/* Variable-length allocator-backed buffer exported by SceLibXml (20 bytes) */
class VarAllocBuffer : public InitializerUser
{
public:
	VarAllocBuffer(const Initializer *init);
	virtual ~VarAllocBuffer();

	int clear();
	int terminate();
	int reserve(size_t size);
	int copy(const SceUInt8 *src, size_t size, bool addNull);
	int copyStr(const char *str, size_t size);
	int copyStr(const String &str);

	SceUInt8 *buffer;
	size_t usedSize;
	size_t capacity;
};

/* Traits for CoreWrapper<CoreT, Traits> */
struct AttrCoreTraits {
	static int init(BxceAttr *core) { return bxceAttrInitialize(core); }
	static int clone(BxceAttr *dst, const BxceAttr *src) { return bxceAttrClone(dst, src); }
	static void term(BxceAttr *core) { bxceAttrTerminate(core); }
};

struct DocumentCoreTraits {
	static int init(BxceDocument *core) { return ScebXCe_FAB50608(core, 1); }
	static void term(BxceDocument *core) { ScebXCe_B0891B13(core); }
};

struct DocumentBuilderCoreTraits {
	static int init(BxceDocumentBuilder *core) { return ScebXCe_11D1FC0D(core); }
	static void term(BxceDocumentBuilder *core) { ScebXCe_8FA315F1(core); }
};

struct NodeListCoreTraits {
	static int init(BxceNodeList *core) { return bxceNodeListInitInvalid(core); }
	static int clone(BxceNodeList *dst, const BxceNodeList *) { return bxceNodeListCloneInvalid(dst); }
	static void term(BxceNodeList *core) { ScebXCe_1F59F0C4(core); }
};

/* Generic ownership wrapper over a bXCe core struct (20 bytes) */
template <typename CoreT, typename Traits>
class CoreWrapper : public InitializerUser
{
public:
	CoreWrapper(const Initializer *init)
		: InitializerUser(init), core(NULL), status(0), isReference(false) {}

	virtual ~CoreWrapper()
	{
		if (core != NULL) {
			if (!isReference) {
				Traits::term(core);
				(void)getInitParam();
				if (core != NULL)
					ScebXCe_EC560647(core);
				isReference = false;
			}
			core = NULL;
		}
	}

	__attribute__((noinline)) int initializeCore();
	__attribute__((noinline)) int terminateCore();
	__attribute__((noinline)) bool copyCore(const CoreWrapper<CoreT, Traits> &src);
	__attribute__((noinline)) int cloneCore(const CoreWrapper<CoreT, Traits> &src);

	int initializeCoreInline()
	{
		if (core == NULL) {
			(void)getInitParam();
			core = (CoreT *)ScebXCe_B8DDE741(sizeof(CoreT));
		}
		int bxRet = Traits::init(core);
		return bXResultToResultType(bxRet);
	}

	int terminateCoreInline()
	{
		if (core == NULL)
			return SCE_XML_ERROR_NOT_INITIALIZED;
		if (!isReference) {
			Traits::term(core);
			(void)getInitParam();
			if (core != NULL)
				ScebXCe_EC560647(core);
			isReference = false;
		}
		core = NULL;
		return SCE_XML_SUCCESS;
	}

	void attachOwnedCore(CoreT *newCore)
	{
		if (core != NULL && !isReference) {
			Traits::term(core);
			(void)getInitParam();
			if (core != NULL)
				ScebXCe_EC560647(core);
			isReference = false;
		}
		core = newCore;
	}

	void attachReferenceCore(CoreT *refCore)
	{
		if (core != NULL && !isReference) {
			Traits::term(core);
			(void)getInitParam();
			if (core != NULL)
				ScebXCe_EC560647(core);
		}
		core = refCore;
		isReference = true;
	}

	__attribute__((noinline)) bool isCoreAvailable() const;

	CoreT *core;
	int status;
	bool isReference;
};

template <typename CoreT, typename Traits>
__attribute__((noinline)) int CoreWrapper<CoreT, Traits>::initializeCore()
{
	if (core == NULL) {
		(void)getInitParam();
		core = (CoreT *)ScebXCe_B8DDE741(sizeof(CoreT));
	}
	int bxRet = Traits::init(core);
	return bXResultToResultType(bxRet);
}

template <typename CoreT, typename Traits>
__attribute__((noinline)) int CoreWrapper<CoreT, Traits>::terminateCore()
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (!isReference) {
		Traits::term(core);
		(void)getInitParam();
		if (core != NULL)
			ScebXCe_EC560647(core);
		isReference = false;
	}
	core = NULL;
	return SCE_XML_SUCCESS;
}

template <typename CoreT, typename Traits>
__attribute__((noinline)) bool CoreWrapper<CoreT, Traits>::copyCore(
	const CoreWrapper<CoreT, Traits> &src)
{
	if (core != NULL) {
		if (!isReference) {
			Traits::term(core);
			(void)getInitParam();
			if (core != NULL)
				ScebXCe_EC560647(core);
			isReference = false;
		}
		core = NULL;
	}
	core = src.core;
	isReference = true;
	return true;
}

template <typename CoreT, typename Traits>
__attribute__((noinline)) int CoreWrapper<CoreT, Traits>::cloneCore(
	const CoreWrapper<CoreT, Traits> &src)
{
	if (core != NULL) {
		if (!isReference) {
			Traits::term(core);
			(void)getInitParam();
			if (core != NULL)
				ScebXCe_EC560647(core);
			isReference = false;
		}
		core = NULL;
	}
	(void)getInitParam();
	core = (CoreT *)ScebXCe_B8DDE741(sizeof(CoreT));
	if (core == NULL)
		return SCE_XML_ERROR_NO_MEMORY;
	int bxRet = Traits::clone(core, src.core);
	return bXResultToResultType(bxRet);
}

template <typename CoreT, typename Traits>
__attribute__((noinline)) bool CoreWrapper<CoreT, Traits>::isCoreAvailable() const
{
	return core != NULL;
}

/* AttrImpl (20 bytes) */
class AttrImpl : public CoreWrapper<BxceAttr, AttrCoreTraits>
{
public:
	AttrImpl(const Initializer *init);
	virtual ~AttrImpl();

	String getName() const;
	String getValue() const;
	int setName(const String *str);
	int setValue(const String *str);
};

/* AttributeListImpl (16 bytes) */
class AttributeListImpl : public InitializerUser
{
public:
	AttributeListImpl(const Initializer *init)
		: InitializerUser(init), list(NULL), isReference(false) {}
	virtual ~AttributeListImpl();

	static __attribute__((noinline)) AttributeListImpl *create(const Initializer *init);
	static AttributeListImpl *get(const AttributeList *list) { return list->impl; }

	int initialize(const Initializer *init);
	__attribute__((noinline)) int terminate();
	int copyFrom(const AttributeListImpl *src);
	int attachReference(BxceAttributeList *refList);

	Attr getAttribute(const String *attName) const;
	Attr getAttribute(index_t index) const;
	size_t getLength() const;
	int addAttribute(const String *name, const String *value);
	void clear();
	bool isAvailable() const;

	BxceAttributeList *list;
	bool isReference;
};

namespace Dom
{

static inline NodeId makeNodeId(const BxceDocument *doc, BxceNodeId bxNode)
{
	if (bxNode == 0)
		return SCE_XML_INVALID_NODE_ID;
	return ((SceUInt64)(uintptr_t)doc << 32) | (SceUInt64)bxNode;
}

static inline BxceDocument *nodeIdDocument(NodeId id)
{
	return (BxceDocument *)((uintptr_t)(id >> 32) & ~(uintptr_t)3u);
}

static inline BxceNodeId nodeIdLocal(NodeId id)
{
	return (BxceNodeId)(id & 0xFFFFFFFFu);
}

/* NodeListImpl (24 bytes) */
class NodeListImpl : public CoreWrapper<BxceNodeList, NodeListCoreTraits>
{
public:
	NodeListImpl(const Initializer *init);
	virtual ~NodeListImpl();

	int terminate();
	void clear();
	size_t getLength() const;
	NodeId insertFirst(NodeId newNode);
	NodeId insertLast(NodeId newNode);
	NodeId removeItem(NodeId node);
	NodeId item(index_t index) const;
	NodeId findItem(const String *name) const;
	NodeId findItem(NodeId node) const;

	const BxceDocument *doc;
};

/* DocumentImpl (28 bytes) */
class DocumentImpl : public CoreWrapper<BxceDocument, DocumentCoreTraits>
{
public:
	DocumentImpl(const Initializer *init);
	virtual ~DocumentImpl();

	int terminate();
	void setOwnerDocument(Document *owner);

	NodeId getDocRoot() const;
	NodeId getRoot() const;
	NodeId getFirstChild(NodeId node) const;
	NodeId getSibling(NodeId node) const;
	String getNodeName(NodeId node) const;
	String getText(NodeId node) const;
	NodeId getFirstAttr(NodeId node) const;
	NodeId getNextAttr(NodeId node) const;
	String getAttrName(NodeId node) const;
	String getAttrValue(NodeId node) const;
	NodeId getParent(NodeId node) const;
	String getSkippedText(NodeId node) const;
	EntityType getEntityType(NodeId node) const;
	String getEntity(NodeId node) const;
	MetaNodeId getXmlMeta() const;
	NodeType getNodeType(NodeId node) const;

	int getElementsByTagName(NodeId node, const String *name, NodeList *nodelist) const;
	int getStatus() const;
	void resetStatus();
	bool isReadOnly() const;
	NodeId setWritable();

	NodeId addElementChild(NodeId parent, const String *name,
			       const AttributeList *list, const String *text);
	NodeId createElement(const String *name, const AttributeList *list, const String *text);
	NodeId insertNode(NodeId parent, NodeId ref, NodeId child);
	NodeId createTextNode(const String *text);
	int recurseDelete(NodeId node);
	NodeId removeChild(NodeId child, NodeId parent);
	int importParent(const Document *parentDoc, NodeId parent);
	NodeId importNode(NodeId nparent, NodeId ref, const Document *doc, NodeId node);

	int setText(NodeId node, const String *text);
	int setAttrValue(NodeId node, const String *name, const String *value);
	int setAttribute(NodeId node, const String *name, const String *value);
	int setAttributeList(NodeId node, const AttributeList *list);
	int removeAttribute(NodeId node, const String *name);
	int removeAttributes(NodeId node);

	int getChildNodes(NodeId node, NodeList *nodelist) const;
	NodeId getLastChild(NodeId node) const;
	bool hasChildNodes(NodeId node) const;
	bool hasAttributes(NodeId node) const;
	String getAttribute(NodeId node, const String *name) const;
	int getAttributes(NodeId node, NodeList *nodelist) const;
	int serialize(const SerializeParameter *param, XmlText *outputString);

	String serialized;
};

/* DocumentBuilderImpl (48 bytes): virtual inheritance of Builder and Document wrappers */
class DocumentBuilderImpl
	: public virtual CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>,
	  public virtual CoreWrapper<BxceDocument, DocumentCoreTraits>
{
public:
	DocumentBuilderImpl(const Initializer *init);
	virtual ~DocumentBuilderImpl();

	int initialize();
	int terminate();
	int parse(const XmlText *text, bool isFinal);
	Document getDocument();
	void setSkipIgnorableWhiteSpace(bool isSkipped);
	void setResolveEntity(bool isResolved);
	int setSkipIgnorableText(bool isSkipped);

	Document document;
};

/* Inline storage layout inside sce::Xml::Dom::Node::impl[64] */
class NodeImpl
{
public:
	NodeId id;
	BxceDocument *doc;
	BxceNodeId node;
	const InitParameter *initParam;
	NodeList *childNodes;
	NodeList *attributes;
};

} /* namespace Dom */

namespace Sax
{

/* Adapter bridging bXCe's BxceDocumentHandler to sce::Xml::Sax::DocumentHandler (16 bytes) */
class DocumentHandlerAdapter : public BxceDocumentHandler, public InitializerUser
{
public:
	DocumentHandlerAdapter(const Initializer *init, DocumentHandler *docHandler)
		: InitializerUser(init), handler(docHandler) {}
	virtual ~DocumentHandlerAdapter() {}

	virtual int startDocument(void *userData);
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int characters(void *userData, const char *ch, int length);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int endDocument(void *userData);
	virtual int skippedText(void *userData, int tokenType, const char *text, int length);
	virtual int entityData(void *userData, int entityType, const char *name, int length);
	virtual void fatalError(void *userData, int errCode);

	DocumentHandler *handler;
};

/* SAX Parser implementation (16 bytes) */
class SaxParserCore : public InitializerUser
{
public:
	SaxParserCore(const Initializer *init);
	virtual ~SaxParserCore();

	int initialize();
	int terminate();
	int setDocumentHandler(DocumentHandler *docHandler);
	void setUserData(void *data);
	int parse(const XmlText *text, bool isFinal);
	int reset();
	void setSkipIgnorableWhiteSpace(bool isSkipped);
	void setResolveEntity(bool isResolved);

	BxceSaxParser *sax;
	DocumentHandlerAdapter *handlerAdapter;
};

/* Handle stored in sce::Xml::Sax::Parser::impl (8 bytes) */
class ParserImpl
{
public:
	ParserImpl() : initializer(NULL), core(NULL) {}

	const Initializer *initializer;
	SaxParserCore *core;
};

} /* namespace Sax */

} /* namespace Xml */
} /* namespace sce */

#endif /* _LIBSCEXML_INTERNAL_H */
