/*
 * result.cpp: bXCe-to-SceLibXml result translation and result name table
 * (0x810033B0-0x81003623, 0x8100372C-0x810039B3).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

int bXResultToResultType(int bxResult)
{
	switch (bxResult) {
	case 0:
		return SCE_XML_SUCCESS;
	case SCE_BXCE_INFO_GENERIC_MESSAGE:
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;
	case SCE_BXCE_ERROR_INVALID_CHARACTER:
		return SCE_XML_ERROR_XML_INVALID_CHAR;
	case SCE_BXCE_ERROR_INVALID_DECIMAL:
		return SCE_XML_ERROR_XML_INVALID_DEC_VALUE;
	case SCE_BXCE_ERROR_INVALID_HEXADECIMAL:
		return SCE_XML_ERROR_XML_INVALID_HEX_VALUE;

	case (int)0x80852000u:
		return SCE_XML_ERROR_NO_MEMORY;
	case (int)0x80852001u:
		return SCE_XML_ERROR_NOT_INITIALIZED;
	case (int)0x80852002u:
		return SCE_XML_ERROR_INVALID_ARGUMENT;
	case (int)0x80852004u:
		return SCE_XML_ERROR_NOT_SUPPORTED;
	case (int)0x80852006u:
		return SCE_XML_ERROR_INITIALIZE_FAILED;
	case (int)0x80852007u:
		return SCE_XML_ERROR_INVALID_BIN_XML;
	case (int)0x80852008u:
		return SCE_XML_ERROR_PARSER_BUSY;
	case (int)0x8085200Au:
		return SCE_XML_ERROR_XML_UNEXPEXTED_EO_F;
	case (int)0x8085200Bu:
		return SCE_XML_ERROR_XML_SYNTAX_ERROR;
	case (int)0x8085200Du:
		return SCE_XML_ERROR_XML_END_TAG_MISMATCH;
	case (int)0x80852011u:
		return SCE_XML_ERROR_XML_INVALID_CHAR;
	case (int)0x80852013u:
		return SCE_XML_ERROR_XML_CLOSING_ANGLE_BRACKET_CHAR_NOT_FOUND;
	case (int)0x80852014u:
		return SCE_XML_ERROR_XML_EQUALITY_CHAR_NOT_FOUND;
	case (int)0x80852015u:
		return SCE_XML_ERROR_XML_SEMI_COLON_CHAR_NOT_FOUND;
	case (int)0x80852016u:
		return SCE_XML_ERROR_XML_END_OF_COMMENT_NOT_FOUND;
	case (int)0x80852017u:
		return SCE_XML_ERROR_XML_END_OF_CD_SECT_NOT_FOUND;
	case (int)0x80852019u:
		return SCE_XML_ERROR_XML_QUOTE_CHAR_NOT_FOUND;
	case (int)0x80852020u:
		return SCE_XML_ERROR_XML_UNKNOWN_ENCODING;
	case (int)0x80852024u:
		return SCE_XML_ERROR_XML_HANDLER_NOT_SET;
	case (int)0x80852025u:
		return SCE_XML_ERROR_XML_INVALID_PI;
	case (int)0x80852026u:
		return SCE_XML_ERROR_XML_INVALID_DOCUMENT_ELEMENT;
	case (int)0x80852027u:
		return SCE_XML_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND;
	case (int)0x80852029u:
		return SCE_XML_ERROR_XML_END_OF_DTD_NOT_FOUND;
	case (int)0x8085202Au:
		return SCE_XML_ERROR_XML_DUPLICATE_ATTR_NAME;

	case (int)0x80852100u:
		return SCE_XML_INFO_XML_PARSE_IN_PROGRESS;
	case (int)0x80852101u:
		return SCE_XML_INFO_XML_PARSE_ABORTED;

	case (int)0x80852205u:
		return SCE_XML_ERROR_DOM_NODE_NOT_FOUND;
	case (int)0x80852209u:
		return SCE_XML_ERROR_DOM_READ_ONLY_ERROR;
	case (int)0x8085220Bu:
		return SCE_XML_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR;
	case (int)0x8085220Cu:
		return SCE_XML_ERROR_DOM_MAX_UNIQUE_ELEMENT_ERROR;
	case (int)0x8085220Du:
		return SCE_XML_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR;
	case (int)0x8085220Eu:
		return SCE_XML_ERROR_DOM_INVALID_ENITITY;
	case (int)0x8085220Fu:
		return SCE_XML_ERROR_DOM_INVALID_NODE_TYPE;
	case (int)0x80852210u:
		return SCE_XML_ERROR_DOM_ERROR;

	default:
		return SCE_XML_ERROR_GENERIC_ERROR;
	}
}

namespace Util
{

const char *strResult(int result)
{
	switch (result) {
	case SCE_XML_SUCCESS:
		return "Success";
	case SCE_XML_INFO_GENERIC_MESSAGE:
		return "GenericMessage";
	case SCE_XML_INFO_XML_PARSE_IN_PROGRESS:
		return "XmlParseInProgress";
	case SCE_XML_INFO_XML_PARSE_ABORTED:
		return "XmlParseAborted";
	case SCE_XML_INFO_MULTIPLE_INITIALIZED:
		return "MultipleInitialized";

	case SCE_XML_ERROR_GENERIC_ERROR:
		return "GenericError";
	case SCE_XML_ERROR_NO_MEMORY:
		return "NoMemory";
	case SCE_XML_ERROR_NOT_INITIALIZED:
		return "NotInitialized";
	case SCE_XML_ERROR_INVALID_ARGUMENT:
		return "InvalidArgument";
	case SCE_XML_ERROR_NOT_SUPPORTED:
		return "NotSupported";
	case SCE_XML_ERROR_INITIALIZE_FAILED:
		return "InitializeFailed";
	case SCE_XML_ERROR_INVALID_BIN_XML:
		return "InvalidBinXml";
	case SCE_XML_ERROR_PARSER_BUSY:
		return "ParserBusy";
	case SCE_XML_ERROR_XML_UNEXPEXTED_EO_F:
		return "XmlUnexpectedEoF";
	case SCE_XML_ERROR_XML_SYNTAX_ERROR:
		return "XmlSyntaxError";
	case SCE_XML_ERROR_XML_END_TAG_MISMATCH:
		return "XmlEndTagMismatch";
	case SCE_XML_ERROR_XML_INVALID_CHAR:
		return "XmlInvalidChar";
	case SCE_XML_ERROR_XML_INVALID_DEC_VALUE:
		return "XmlInvalidDecValue";
	case SCE_XML_ERROR_XML_INVALID_HEX_VALUE:
		return "XmlInvalidHexValue";
	case SCE_XML_ERROR_XML_CLOSING_ANGLE_BRACKET_CHAR_NOT_FOUND:
		return "XmlClosingAngleBracketCharNotFound";
	case SCE_XML_ERROR_XML_EQUALITY_CHAR_NOT_FOUND:
		return "XmlEqualityCharNotFound";
	case SCE_XML_ERROR_XML_SEMI_COLON_CHAR_NOT_FOUND:
		return "XmlSemiColonCharNotFound";
	case SCE_XML_ERROR_XML_QUOTE_CHAR_NOT_FOUND:
		return "XmlQuoteCharNotFound";
	case SCE_XML_ERROR_XML_END_OF_COMMENT_NOT_FOUND:
		return "XmlEndOfCommentNotFound";
	case SCE_XML_ERROR_XML_END_OF_CD_SECT_NOT_FOUND:
		return "XmlEndOfCDSectNotFound";
	case SCE_XML_ERROR_XML_END_OF_DTD_NOT_FOUND:
		return "XmlEndOfDtdNotFound";
	case SCE_XML_ERROR_XML_UNKNOWN_ENCODING:
		return "XmlUnknownEncoding";
	case SCE_XML_ERROR_XML_HANDLER_NOT_SET:
		return "XmlHandlerNotSet";
	case SCE_XML_ERROR_XML_INVALID_PI:
		return "XmlInvalidPi";
	case SCE_XML_ERROR_XML_INVALID_DOCUMENT_ELEMENT:
		return "XmlInvalidDocumentElement";
	case SCE_XML_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND:
		return "XmlDocumentElementNotFound";
	case SCE_XML_ERROR_XML_DUPLICATE_ATTR_NAME:
		return "XmlDuplicateAttrName";

	case SCE_XML_ERROR_DOM_ERROR:
		return "DomError";
	case SCE_XML_ERROR_DOM_NODE_NOT_FOUND:
		return "DomNodeNotFound";
	case SCE_XML_ERROR_DOM_READ_ONLY_ERROR:
		return "DomReadOnlyError";
	case SCE_XML_ERROR_DOM_MAX_UNIQUE_ELEMENT_ERROR:
		return "DomMaxUniqueElementError";
	case SCE_XML_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR:
		return "DomMaxUniqueAttrError";
	case SCE_XML_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR:
		return "DomMaxNumOfAttrError";
	case SCE_XML_ERROR_DOM_MAX_SIZE_OF_ELEMENT_NAME_ERROR:
		return "DomMaxSizeOfElementNameError";
	case SCE_XML_ERROR_DOM_MAX_SIZE_OF_ATTR_NAME_ERROR:
		return "DomMaxSizeOfAttrNameError";
	case SCE_XML_ERROR_DOM_MAX_SIZE_OF_ATTR_VALUE_ERROR:
		return "DomMaxSizeOfAttrValueError";
	case SCE_XML_ERROR_DOM_INVALID_ENITITY:
		return "DomInvalidEnitity";
	case SCE_XML_ERROR_DOM_INVALID_NODE_TYPE:
		return "DomInvalidNodeType";

	default:
		return "Unknown error.";
	}
}

} /* namespace Util */

} /* namespace Xml */
} /* namespace sce */
