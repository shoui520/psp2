/*
 * The SDK's xml/xml_result.h: SceLibXml result enum and error/info constants
 * (Sony's names and values, SDK 3.570.011).
 */
#ifndef _SCE_XML_RESULT_H
#define _SCE_XML_RESULT_H

#include <sceerror.h>

#define SCE_XML_PARSER_ERROR_OFFSET	SCE_ERROR_MAKE_ERROR(SCE_ERROR_FACILITY_XML, 0)
#define SCE_XML_PARSER_INFO_OFFSET	(SCE_XML_PARSER_ERROR_OFFSET & ~0x80000000)

#ifdef __cplusplus
namespace sce
{
namespace Xml
{

enum result_t
{
	resultSuccess = 0,
	resultGenericError = SCE_XML_PARSER_ERROR_OFFSET,
	resultNoMemory,
	resultNotInitialized,
	resultInvalidArgument,
	resultNotSupported,
	resultInitializeFailed,
	resultInvalidBinXml,
	resultParserBusy,
	resultXmlUnexpextedEoF,
	resultXmlSyntaxError,
	resultXmlEndTagMismatch,
	resultXmlInvalidChar,
	resultXmlInvalidDecValue,
	resultXmlInvalidHexValue,
	resultXmlClosingAngleBracketCharNotFound,
	resultXmlEqualityCharNotFound,
	resultXmlSemiColonCharNotFound,
	resultXmlQuoteCharNotFound,
	resultXmlEndOfCommentNotFound,
	resultXmlEndOfCDSectNotFound,
	resultXmlEndOfDtdNotFound,
	resultXmlUnknownEncoding,
	resultXmlHandlerNotSet,
	resultXmlInvalidPi,
	resultXmlInvalidDocumentElement,
	resultXmlDocumentElementNotFound,
	resultXmlDuplicateAttrName,

	resultDomError = SCE_XML_PARSER_ERROR_OFFSET + 0x200,
	resultDomNodeNotFound,
	resultDomReadOnlyError,
	resultDomMaxUniqueElementError,
	resultDomMaxUniqueAttrError,
	resultDomMaxNumOfAttrError,
	resultDomMaxSizeOfElementNameError,
	resultDomMaxSizeOfAttrNameError,
	resultDomMaxSizeOfAttrValueError,
	resultDomInvalidEnitity,
	resultDomInvalidNodeType,

	resultGenericMessage = SCE_XML_PARSER_INFO_OFFSET,
	resultXmlParseInProgress,
	resultXmlParseAborted,
	resultMultipleInitialized
};

} /* namespace Xml */
} /* namespace sce */
#endif

#define SCE_XML_SUCCESS                                                       0 /* 0x00000000 */
#define SCE_XML_ERROR_GENERIC_ERROR                                 -2138767360 /* 0x80850000 */
#define SCE_XML_ERROR_NO_MEMORY                                     -2138767359 /* 0x80850001 */
#define SCE_XML_ERROR_NOT_INITIALIZED                               -2138767358 /* 0x80850002 */
#define SCE_XML_ERROR_INVALID_ARGUMENT                              -2138767357 /* 0x80850003 */
#define SCE_XML_ERROR_NOT_SUPPORTED                                 -2138767356 /* 0x80850004 */
#define SCE_XML_ERROR_INITIALIZE_FAILED                             -2138767355 /* 0x80850005 */
#define SCE_XML_ERROR_INVALID_BIN_XML                               -2138767354 /* 0x80850006 */
#define SCE_XML_ERROR_PARSER_BUSY                                   -2138767353 /* 0x80850007 */
#define SCE_XML_ERROR_XML_UNEXPEXTED_EO_F                           -2138767352 /* 0x80850008 */
#define SCE_XML_ERROR_XML_SYNTAX_ERROR                              -2138767351 /* 0x80850009 */
#define SCE_XML_ERROR_XML_END_TAG_MISMATCH                          -2138767350 /* 0x8085000a */
#define SCE_XML_ERROR_XML_INVALID_CHAR                              -2138767349 /* 0x8085000b */
#define SCE_XML_ERROR_XML_INVALID_DEC_VALUE                         -2138767348 /* 0x8085000c */
#define SCE_XML_ERROR_XML_INVALID_HEX_VALUE                         -2138767347 /* 0x8085000d */
#define SCE_XML_ERROR_XML_CLOSING_ANGLE_BRACKET_CHAR_NOT_FOUND      -2138767346 /* 0x8085000e */
#define SCE_XML_ERROR_XML_EQUALITY_CHAR_NOT_FOUND                   -2138767345 /* 0x8085000f */
#define SCE_XML_ERROR_XML_SEMI_COLON_CHAR_NOT_FOUND                 -2138767344 /* 0x80850010 */
#define SCE_XML_ERROR_XML_QUOTE_CHAR_NOT_FOUND                      -2138767343 /* 0x80850011 */
#define SCE_XML_ERROR_XML_END_OF_COMMENT_NOT_FOUND                  -2138767342 /* 0x80850012 */
#define SCE_XML_ERROR_XML_END_OF_CD_SECT_NOT_FOUND                  -2138767341 /* 0x80850013 */
#define SCE_XML_ERROR_XML_END_OF_DTD_NOT_FOUND                      -2138767340 /* 0x80850014 */
#define SCE_XML_ERROR_XML_UNKNOWN_ENCODING                          -2138767339 /* 0x80850015 */
#define SCE_XML_ERROR_XML_HANDLER_NOT_SET                           -2138767338 /* 0x80850016 */
#define SCE_XML_ERROR_XML_INVALID_PI                                -2138767337 /* 0x80850017 */
#define SCE_XML_ERROR_XML_INVALID_DOCUMENT_ELEMENT                  -2138767336 /* 0x80850018 */
#define SCE_XML_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND                -2138767335 /* 0x80850019 */
#define SCE_XML_ERROR_XML_DUPLICATE_ATTR_NAME                       -2138767334 /* 0x8085001a */
#define SCE_XML_ERROR_DOM_ERROR                                     -2138766848 /* 0x80850200 */
#define SCE_XML_ERROR_DOM_NODE_NOT_FOUND                            -2138766847 /* 0x80850201 */
#define SCE_XML_ERROR_DOM_READ_ONLY_ERROR                           -2138766846 /* 0x80850202 */
#define SCE_XML_ERROR_DOM_MAX_UNIQUE_ELEMENT_ERROR                  -2138766845 /* 0x80850203 */
#define SCE_XML_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR                     -2138766844 /* 0x80850204 */
#define SCE_XML_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR                     -2138766843 /* 0x80850205 */
#define SCE_XML_ERROR_DOM_MAX_SIZE_OF_ELEMENT_NAME_ERROR            -2138766842 /* 0x80850206 */
#define SCE_XML_ERROR_DOM_MAX_SIZE_OF_ATTR_NAME_ERROR               -2138766841 /* 0x80850207 */
#define SCE_XML_ERROR_DOM_MAX_SIZE_OF_ATTR_VALUE_ERROR              -2138766840 /* 0x80850208 */
#define SCE_XML_ERROR_DOM_INVALID_ENITITY                           -2138766839 /* 0x80850209 */
#define SCE_XML_ERROR_DOM_INVALID_NODE_TYPE                         -2138766838 /* 0x8085020a */
#define SCE_XML_INFO_GENERIC_MESSAGE                                    8716288 /* 0x00850000 */
#define SCE_XML_INFO_XML_PARSE_IN_PROGRESS                              8716289 /* 0x00850001 */
#define SCE_XML_INFO_XML_PARSE_ABORTED                                  8716290 /* 0x00850002 */
#define SCE_XML_INFO_MULTIPLE_INITIALIZED                               8716291 /* 0x00850003 */

#endif /* _SCE_XML_RESULT_H */
