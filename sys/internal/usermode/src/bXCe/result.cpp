/*
 * ScebXCe's results: the values the C functions return (0x80852xxx) as the ones the C++
 * interface returns (0x80851xxx, 0x1000 below), and the name of each of those.
 */
#include "bxce_internal.h"

/* an internal result and the public one 0x1000 below it */
#define BXCE_PUBLIC_RESULT(internal) \
	case (internal): \
		return (int)((internal) - 0x1000)

int bxceConvertResult(int result)
{
	switch ((SceUInt32)result) {
	case 0:
		return 0;
	case (SceUInt32)SCE_BXCE_INFO_GENERIC_MESSAGE:
		return 0x00851002;		/* MultipleInitialized */
	BXCE_PUBLIC_RESULT(0x80852000);
	BXCE_PUBLIC_RESULT(0x80852001);
	BXCE_PUBLIC_RESULT(0x80852002);
	BXCE_PUBLIC_RESULT(0x80852003);
	BXCE_PUBLIC_RESULT(0x80852004);
	BXCE_PUBLIC_RESULT(0x80852005);
	BXCE_PUBLIC_RESULT(0x80852006);
	BXCE_PUBLIC_RESULT(0x80852007);
	BXCE_PUBLIC_RESULT(0x80852008);
	BXCE_PUBLIC_RESULT(0x80852009);
	BXCE_PUBLIC_RESULT(0x8085200A);
	BXCE_PUBLIC_RESULT(0x8085200B);
	BXCE_PUBLIC_RESULT(0x8085200C);
	BXCE_PUBLIC_RESULT(0x8085200D);
	BXCE_PUBLIC_RESULT(0x8085200E);
	BXCE_PUBLIC_RESULT(0x8085200F);
	BXCE_PUBLIC_RESULT(0x80852010);
	BXCE_PUBLIC_RESULT(0x80852011);
	BXCE_PUBLIC_RESULT(0x80852012);
	BXCE_PUBLIC_RESULT(0x80852013);
	BXCE_PUBLIC_RESULT(0x80852014);
	BXCE_PUBLIC_RESULT(0x80852015);
	BXCE_PUBLIC_RESULT(0x80852016);
	BXCE_PUBLIC_RESULT(0x80852017);
	BXCE_PUBLIC_RESULT(0x80852018);
	BXCE_PUBLIC_RESULT(0x80852019);
	BXCE_PUBLIC_RESULT(0x8085201A);
	BXCE_PUBLIC_RESULT(0x8085201B);
	BXCE_PUBLIC_RESULT(0x8085201C);
	BXCE_PUBLIC_RESULT(0x8085201D);
	BXCE_PUBLIC_RESULT(0x8085201E);
	BXCE_PUBLIC_RESULT(0x8085201F);
	BXCE_PUBLIC_RESULT(0x80852020);
	BXCE_PUBLIC_RESULT(0x80852021);
	BXCE_PUBLIC_RESULT(0x80852022);
	BXCE_PUBLIC_RESULT(0x80852023);
	BXCE_PUBLIC_RESULT(0x80852024);
	BXCE_PUBLIC_RESULT(0x80852025);
	BXCE_PUBLIC_RESULT(0x80852026);
	BXCE_PUBLIC_RESULT(0x80852027);
	BXCE_PUBLIC_RESULT(0x80852028);
	BXCE_PUBLIC_RESULT(0x80852029);
	BXCE_PUBLIC_RESULT(0x8085202A);
	case (SceUInt32)SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS:
		return 0x00851001;		/* XML_ParseInProgress */
	BXCE_PUBLIC_RESULT(0x80852101);
	BXCE_PUBLIC_RESULT(0x80852102);
	BXCE_PUBLIC_RESULT(0x80852103);
	BXCE_PUBLIC_RESULT(0x80852200);
	BXCE_PUBLIC_RESULT(0x80852201);
	BXCE_PUBLIC_RESULT(0x80852202);
	BXCE_PUBLIC_RESULT(0x80852203);
	BXCE_PUBLIC_RESULT(0x80852204);
	BXCE_PUBLIC_RESULT(0x80852205);
	BXCE_PUBLIC_RESULT(0x80852206);
	BXCE_PUBLIC_RESULT(0x80852207);
	BXCE_PUBLIC_RESULT(0x80852208);
	BXCE_PUBLIC_RESULT(0x80852209);
	BXCE_PUBLIC_RESULT(0x8085220A);
	BXCE_PUBLIC_RESULT(0x8085220B);
	BXCE_PUBLIC_RESULT(0x8085220C);
	BXCE_PUBLIC_RESULT(0x8085220D);
	BXCE_PUBLIC_RESULT(0x8085220E);
	BXCE_PUBLIC_RESULT(0x8085220F);
	BXCE_PUBLIC_RESULT(0x80852210);
	BXCE_PUBLIC_RESULT(0x80852300);
	BXCE_PUBLIC_RESULT(0x80852500);
	BXCE_PUBLIC_RESULT(0x80852501);
	BXCE_PUBLIC_RESULT(0x80852502);
	BXCE_PUBLIC_RESULT(0x80852503);
	BXCE_PUBLIC_RESULT(0x80852600);
	BXCE_PUBLIC_RESULT(0x80852601);
	BXCE_PUBLIC_RESULT(0x80852602);
	default:
		return 0x00851000;
	}
}

/* the name of a public result, "!! undefined !!" for any other value */
const char *ScebXCe_998552A5(int result)
{
	switch (result) {
	case 0:
		return "Success";
	case 0x00851001:
		return "XML_ParseInProgress";
	case 0x00851002:
		return "MultipleInitialized";
	case (int)0x80851000:
		return "NoMemory";
	case (int)0x80851001:
		return "NotInitialized";
	case (int)0x80851002:
		return "InvalidArgument";
	case (int)0x80851003:
		return "UnkownError";
	case (int)0x80851004:
		return "NotSupported";
	case (int)0x80851005:
		return "InternalError";
	case (int)0x80851006:
		return "InitFailed";
	case (int)0x80851007:
		return "InvalidbX";
	case (int)0x80851008:
		return "ParserBusy";
	case (int)0x8085100A:
		return "XML_UnexpectedEOF";
	case (int)0x8085100B:
		return "XML_SyntaxError";
	case (int)0x8085100C:
		return "XML_TagNotClosed";
	case (int)0x8085100D:
		return "XML_EndTagMismatch";
	case (int)0x8085100E:
		return "XML_UnexpectedEndTag";
	case (int)0x8085100F:
		return "XML_UnknownToken";
	case (int)0x80851010:
		return "XML_UnexpectedToken";
	case (int)0x80851011:
		return "XML_UnexpectedChar";
	case (int)0x80851012:
		return "XML_ExpectedClosingSquareBracket";
	case (int)0x80851013:
		return "XML_ExpectedClosingAngleBracket";
	case (int)0x80851014:
		return "XML_ExpectedEqualityOperator";
	case (int)0x80851015:
		return "XML_ExpectedSemiColon";
	case (int)0x80851016:
		return "XML_EndOfCommentNotFound";
	case (int)0x80851017:
		return "XML_EndOfCDSectNotFound";
	case (int)0x80851018:
		return "XML_UnexpectedSpace";
	case (int)0x80851019:
		return "XML_ExpectedQuote";
	case (int)0x8085101A:
		return "XML_ExpectedSpace";
	case (int)0x8085101B:
		return "XML_ExpectedElementName";
	case (int)0x8085101C:
		return "XML_ExpectedAttributeName";
	case (int)0x8085101D:
		return "XML_ExpectedPositiveInteger";
	case (int)0x8085101E:
		return "XML_ExpectedAttributeValue";
	case (int)0x8085101F:
		return "XML_UnexpectedCharacter";
	case (int)0x80851020:
		return "XML_UnknownEncoding";
	case (int)0x80851021:
		return "XML_ExpectedDocumentElement";
	case (int)0x80851022:
		return "XML_InvalidContextId";
	case (int)0x80851023:
		return "XML_ProblemsOpeningStream";
	case (int)0x80851024:
		return "XML_NULL_Handler";
	case (int)0x80851025:
		return "XML_InvalidPI";
	case (int)0x80851026:
		return "XML_InvalidDocumentElement";
	case (int)0x80851027:
		return "XML_DocumentElementNotFound";
	case (int)0x80851028:
		return "XML_EndOfDeclNotFound";
	case (int)0x80851029:
		return "XML_EndOfDTDNotFound";
	case (int)0x8085102A:
		return "result_XML_DuplicateAttributeName";
	case (int)0x80851101:
		return "XML_ParseAborted";
	case (int)0x80851102:
		return "XML_ParseSuspended";
	case (int)0x80851103:
		return "XML_InvalidContext";
	case (int)0x80851200:
		return "DOM_HierarchyRequestError";
	case (int)0x80851201:
		return "DOM_IndexSizeError";
	case (int)0x80851202:
		return "DOM_InvalidAccessError";
	case (int)0x80851203:
		return "DOM_InvalidStateError";
	case (int)0x80851204:
		return "DOM_NoModificationAllowedError";
	case (int)0x80851205:
		return "DOM_NotFoundError";
	case (int)0x80851206:
		return "DOM_NotSupportedError";
	case (int)0x80851207:
		return "DOM_NoReferenceGiven";
	case (int)0x80851208:
		return "DOM_WrongDocumentError";
	case (int)0x80851209:
		return "DOM_ReadOnlyError";
	case (int)0x8085120A:
		return "DOM_EmptyElementError";
	case (int)0x8085120B:
		return "DOM_MAXNumOfAttrError";
	case (int)0x8085120C:
		return "DOM_MAXUniqueElementError";
	case (int)0x8085120D:
		return "DOM_MAXUniqueAttrError";
	case (int)0x8085120E:
		return "DOM_EnitityError";
	case (int)0x8085120F:
		return "DOM_NotAllowedNodeType";
	case (int)0x80851210:
		return "ExceedLimit";
	case (int)0x80851300:
		return "XPath_ExpressionError";
	case (int)0x80851500:
		return "Utils_InvalidXMLChar";
	case (int)0x80851501:
		return "Utils_InvalidXMLName";
	case (int)0x80851502:
		return "Utils_InvalidAsciiDigit";
	case (int)0x80851503:
		return "Utils_InvalidHexValue";
	case (int)0x80851600:
		return "Utils_Serializer_BufferTooSmall";
	case (int)0x80851601:
		return "Utils_Serializer_Continue";
	case (int)0x80851602:
		return "Utils_Serializer_Final";
	default:
		return "!! undefined !!";
	}
}
