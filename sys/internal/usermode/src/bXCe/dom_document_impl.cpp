/*
 * ScebXCe's DocumentImpl: the C++ DOM document over the library's document, its own or
 * another object's.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Dom {

DocumentImpl::DocumentImpl(int reserved) : document(NULL), isReference(false)
{
	document = (BxceDocument *)ScebXCe_B8DDE741(sizeof(BxceDocument));
}

/* copies the start of the source object itself into the new document, as Sony's code does */
DocumentImpl::DocumentImpl(const DocumentImpl &src) : document(NULL), isReference(false)
{
	document = (BxceDocument *)ScebXCe_B8DDE741(sizeof(BxceDocument));
	if (document == NULL)
		return;
	bxceMemcpy(document, &src, sizeof(BxceDocument));
}

DocumentImpl::DocumentImpl(const DocumentImpl *src)
	: document(src != NULL ? src->document : NULL), isReference(true)
{
}

DocumentImpl::DocumentImpl(BxceDocument *doc) : document(doc), isReference(true)
{
}

DocumentImpl::~DocumentImpl()
{
	if (document != NULL && !isReference) {
		ScebXCe_B0891B13(document);
		if (document != NULL)
			ScebXCe_EC560647(document);
	}
	isReference = false;
	document = NULL;
}

/* the arguments are not used */
int DocumentImpl::initialize(const void *init, bool isDom)
{
	if (document == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_FAB50608(document, 1));
}

int DocumentImpl::terminate()
{
	if (document == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	ScebXCe_B0891B13(document);
	return bxceConvertResult(0);
}

BxceNodeId DocumentImpl::getDocRoot() const
{
	if (document == NULL)
		return 0;
	return ScebXCe_75E1B095(document);
}

BxceNodeId DocumentImpl::getRoot() const
{
	if (document == NULL)
		return 0;
	return ScebXCe_CED6BECF(document);
}

BxceNodeId DocumentImpl::getFirstChild(BxceNodeId node) const
{
	if (document == NULL)
		return 0;
	return ScebXCe_FEA6E4A1(document, node);
}

BxceNodeId DocumentImpl::getSibling(BxceNodeId node) const
{
	if (document == NULL)
		return 0;
	return ScebXCe_6FC9DC3F(document, node);
}

const char *DocumentImpl::getNodeName(BxceNodeId node) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_1189A2B1(document, node);
}

const char *DocumentImpl::getText(BxceNodeId node) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_33825EFF(document, node);
}

BxceNodeId DocumentImpl::getFirstAttr(BxceNodeId node) const
{
	if (document == NULL)
		return 0;
	return ScebXCe_B034C595(document, node);
}

const char *DocumentImpl::getAttrName(BxceNodeId node) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_B4989A58(document, node);
}

const char *DocumentImpl::getAttrValue(BxceNodeId attr) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_84383D8E(document, attr);
}

BxceNodeId DocumentImpl::getNextAttr(BxceNodeId node) const
{
	if (document == NULL)
		return 0;
	return ScebXCe_134F491C(document, node);
}

BxceNodeId DocumentImpl::getParent(BxceNodeId node) const
{
	if (document == NULL)
		return 0;
	return bxceDocumentGetParent(document, node);
}

const char *DocumentImpl::getSkippedText(BxceNodeId node) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_2FAD3DFA(document, node);
}

int DocumentImpl::getEntityType(BxceNodeId node) const
{
	if (document == NULL)
		return 0;
	return ScebXCe_35CDE77C(document, node);
}

const char *DocumentImpl::getEntityName(BxceNodeId node) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_1189A2B1(document, node);
}

/* -1 without a document or a node */
int DocumentImpl::getNodeType(BxceNodeId node) const
{
	if (document == NULL || node == 0)
		return -1;
	return ScebXCe_FA18A039(document, node);
}

/* the matching elements as a new NodeList over the library's list */
NodeList *DocumentImpl::getElementsByTagName(BxceNodeId node,
								     const char *name) const
{
	BxceNodeList *list;

	if (document == NULL)
		return NULL;
	list = ScebXCe_1FA24DC0(document, node, name);
	if (list == NULL)
		return NULL;
	return new NodeList(list);
}

BxceNodeId DocumentImpl::addElementChild(BxceNodeId parent,
								   const char *name,
								   const BxceAttributeList *attrs,
								   const char *text,
								   SceUInt32 textLength)
{
	if (document == NULL)
		return 0;
	return ScebXCe_AC0E4F67(document, parent, name, attrs, text, textLength);
}

BxceNodeId DocumentImpl::insertNode(BxceNodeId parent, BxceNodeId after,
							      BxceNodeId child)
{
	if (document == NULL)
		return 0;
	return ScebXCe_3BF2F11B(document, parent, after, child);
}

BxceNodeId DocumentImpl::createTextNode(const char *text,
								  SceUInt32 length)
{
	if (document == NULL)
		return 0;
	return ScebXCe_AEB2CE13(document, text, length);
}

BxceNodeId DocumentImpl::removeChild(BxceNodeId child, BxceNodeId parent)
{
	if (document == NULL)
		return 0;
	return ScebXCe_568CE24D(document, child, parent);
}

BxceNodeId DocumentImpl::importNode(BxceNodeId parent, BxceNodeId after,
							      const BxceDocument *src,
							      BxceNodeId node)
{
	if (document == NULL)
		return 0;
	return ScebXCe_62C76804(document, parent, after, src, node);
}

/* not implemented */
int DocumentImpl::setAttributeList(BxceNodeId node, const char *name,
							     const char *value, SceUInt32 length)
{
	return 0;
}

BxceNodeList *DocumentImpl::getChildNodes(BxceNodeId node) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_2B9B57AE(document, node);
}

const char *DocumentImpl::getAttribute(BxceNodeId node,
								 const char *name) const
{
	if (document == NULL)
		return NULL;
	return ScebXCe_753D9382(document, node, name);
}

} /* namespace Dom */
} /* namespace bXCe */
