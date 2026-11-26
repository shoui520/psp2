/*
 * teleport_xml.cpp: XML response parsing and memory allocator adapter for
 * SceTeleportClient (0x8100377C-0x810049AD).
 */
#include "teleport_internal.h"

static SceVoid *(*s_xmlAllocFunc)(SceUInt32 size) = NULL;
static SceVoid (*s_xmlFreeFunc)(void *ptr) = NULL;

static int parseDecimalText(const char *str, SceSize maxLen)
{
	int value = 0;
	int negative = 0;
	for (SceSize i = 0; i <= sceClibStrnlen(str, maxLen); ++i) {
		int ch = (int)(signed char)str[i];
		if (ch < '0' || ch > '9') {
			if (str[0] != '-' || i != 0)
				break;
			negative = 1;
		} else {
			value = value * 10 + (ch - '0');
		}
	}
	if (negative == 1)
		value = -value;
	return value;
}

static bool readChildElementText(sce::Xml::Dom::Document *doc, sce::Xml::Dom::NodeId parentNode,
				 const char *tagName, char *outBuf, SceSize outBufSize)
{
	sce::Xml::Dom::NodeList childList;
	sce::Xml::String tag(tagName);
	if (doc->getElementsByTagName(parentNode, &tag, &childList) == SCE_XML_SUCCESS &&
	    childList.getLength() != 0) {
		sce::Xml::Dom::NodeId elemNode = childList.item(0);
		sce::Xml::Dom::NodeId textNode = doc->getFirstChild(elemNode);
		sce::Xml::String textVal = doc->getText(textNode);
		sceClibSnprintf(outBuf, outBufSize, "%s", textVal.buffer);
		return true;
	}
	return false;
}

void *TeleportXmlMemAllocator::allocate(size_t size, void *userData)
{
	(void)userData;
	return s_xmlAllocFunc((SceUInt32)size);
}

void TeleportXmlMemAllocator::deallocate(void *ptr, void *userData)
{
	(void)userData;
	s_xmlFreeFunc(ptr);
}

TeleportXmlParser::TeleportXmlParser(const SceTeleportClientInitializeInfo *info)
{
	s_xmlAllocFunc = info->_malloc;
	s_xmlFreeFunc = info->_free;
}

TeleportXmlParser::~TeleportXmlParser()
{
}

int TeleportXmlParser::parseErrorResponse(sce::Xml::Dom::Document *doc,
					  sce::Xml::Dom::NodeId rootNode,
					  SceTeleportLaunchResponse *res,
					  SceUInt32 *outErrCode)
{
	{
		sce::Xml::Dom::NodeList appList;
		sce::Xml::String appTag("app");
		if (doc->getElementsByTagName(rootNode, &appTag, &appList) == SCE_XML_SUCCESS) {
			(void)appList.getLength();
			char optBuf[32];
			memset(optBuf, 0, sizeof(optBuf));
			sce::Xml::Dom::NodeId appNode = appList.item(0);
			readChildElementText(doc, appNode, "option", optBuf, sizeof(optBuf));
		}
	}

	sce::Xml::String errTag("error");
	sce::Xml::Dom::NodeList errList;
	int status = doc->getElementsByTagName(rootNode, &errTag, &errList);
	if (status == SCE_XML_SUCCESS) {
		(void)errList.getLength();
		char textBuf[128];
		memset(textBuf, 0, sizeof(textBuf));

		sce::Xml::Dom::NodeId errNode = errList.item(0);
		readChildElementText(doc, errNode, "hint", textBuf, sizeof(textBuf));
		sceClibSnprintf(asChar(res->appErrorHint), 128, "%s", textBuf);

		errNode = errList.item(0);
		if (!readChildElementText(doc, errNode, "code", textBuf, sizeof(textBuf)))
			status = 1;

		char upperCode[12];
		memset(upperCode, 0, 11);
		for (unsigned int i = 0; i < 11 && textBuf[i] != '\0'; ++i)
			upperCode[i] = (char)sceClibToupper((int)(signed char)textBuf[i]);

		SceUInt32 parsedCode = (SceUInt32)SCE_TELEPORT_ERROR_XML_PARSE;
		for (SceSize i = 0; i <= sceClibStrnlen(textBuf, 10); ++i) {
			int ch = (int)(signed char)upperCode[i];
			if (ch >= '0' && ch <= '9') {
				parsedCode = parsedCode * 16 + (SceUInt32)(ch - '0');
			} else if (ch >= 'A' && ch <= 'F') {
				parsedCode = parsedCode * 16 + (SceUInt32)(ch - 'A' + 10);
			} else if (ch != 'x' && ch != 'X') {
				break;
			}
		}
		*outErrCode = parsedCode;
		res->appPort = 0;
	}
	return status;
}

int TeleportXmlParser::parseLaunchResponse(sce::Xml::Dom::Document *doc,
					   sce::Xml::Dom::NodeId rootNode,
					   SceTeleportLaunchResponse *res)
{
	res->appPort = 0;
	sce::Xml::Dom::NodeList appList;
	sce::Xml::String appTag("app");
	doc->getElementsByTagName(rootNode, &appTag, &appList);
	(void)appList.getLength();

	char textBuf[128];
	memset(textBuf, 0, sizeof(textBuf));
	sce::Xml::Dom::NodeId appNode = appList.item(0);

	int status = 0;
	if (!readChildElementText(doc, appNode, "port", textBuf, sizeof(textBuf)))
		status = 1;
	if (status != 0)
		return status;

	res->appPort = (SceUInt16)parseDecimalText(textBuf, 8);
	sceClibMemset(res->appErrorHint, 0, sizeof(res->appErrorHint));
	sceClibMemset(res->reserved, 0, sizeof(res->reserved));

	appNode = appList.item(0);
	if (readChildElementText(doc, appNode, "ext", textBuf, sizeof(textBuf))) {
		if (res->extData != NULL && res->extDataBufSize != 0)
			sceClibSnprintf(asChar(res->extData), res->extDataBufSize, "%s", textBuf);
	}
	return 0;
}

int TeleportXmlParser::parseHttpStatusCode(const char *httpResp, int respLen)
{
	char statusLine[64] = { 0 };
	char codeStr[16];

	unsigned int lineLen = 0;
	for (int i = 0; i < respLen; ++i) {
		char ch = httpResp[i];
		if (ch == '\n')
			break;
		if (ch != '\r')
			statusLine[lineLen] = ch;
		++lineLen;
	}

	memset(codeStr, 0, sizeof(codeStr));
	unsigned int idx = 0;
	while (idx < lineLen && statusLine[idx] != ' ')
		++idx;
	if (idx < lineLen)
		++idx;

	unsigned int outIdx = 0;
	while (idx < lineLen && statusLine[idx] != ' ') {
		if (outIdx < sizeof(codeStr))
			codeStr[outIdx] = statusLine[idx];
		++idx;
		++outIdx;
	}

	return parseDecimalText(codeStr, 8);
}

int TeleportXmlParser::parseVersionString(const char *verStr, unsigned int maxLen, SceUInt32 *outVer)
{
	unsigned int len = 0;
	if (verStr != NULL) {
		while (len < maxLen && verStr[len] != '\0')
			++len;
	}
	if (len > maxLen - 1)
		return SCE_TELEPORT_ERROR_XML_PARSE;

	for (unsigned int i = 0; i < len; ++i) {
		char ch = verStr[i];
		if ((ch < '0' || ch > '9') && ch != '.')
			return SCE_TELEPORT_ERROR_XML_PARSE;
	}

	int bcd = 0;
	int mult = 1;
	for (int i = (int)len - 1; i >= 0; --i) {
		char ch = verStr[i];
		if (ch != '.') {
			bcd += (ch - '0') * mult;
			mult *= 16;
		}
	}
	*outVer = (SceUInt32)(bcd << 16);
	return 0;
}

int TeleportXmlParser::parseRemoteAppList(sce::Xml::Dom::Document *doc,
					  sce::Xml::Dom::NodeId rootNode,
					  TeleportRemoteAppList *outList)
{
	const char appTagStr[4] = "app";
	SceUInt16 appCount = 0;
	{
		sce::Xml::Dom::NodeList countList;
		sce::Xml::String appTag(appTagStr);
		doc->getElementsByTagName(rootNode, &appTag, &countList);
		for (size_t i = 0; i < countList.getLength(); ++i) {
			sce::Xml::Dom::NodeId node = countList.item(i);
			sce::Xml::String nodeName = doc->getNodeName(node);
			if (sceClibStrcmp(nodeName.buffer, appTagStr) == 0)
				++appCount;
		}
	}

	sce::Xml::Dom::NodeList appNodes;
	sce::Xml::String appTag("app");
	int status = doc->getElementsByTagName(rootNode, &appTag, &appNodes);

	outList->items = (SceTeleportRemoteAppInfo *)s_xmlAllocFunc(
		(SceUInt32)(sizeof(SceTeleportRemoteAppInfo) * appCount));
	outList->totalCount = appCount;

	int parsedIdx = 0;
	char textBuf[516];
	char nameBuf[256];
	char uriBuf[128];

	for (size_t i = 0; i < appNodes.getLength(); ++i) {
		sce::Xml::Dom::NodeId node = appNodes.item(i);
		sce::Xml::String nodeName = doc->getNodeName(node);
		if (sceClibStrcmp(nodeName.buffer, appTagStr) != 0)
			continue;

		sce::Xml::Dom::NodeId appNode = appNodes.item(i);
		readChildElementText(doc, appNode, "name", textBuf, 512);
		sceClibSnprintf(nameBuf, sizeof(nameBuf), "%s", textBuf);

		appNode = appNodes.item(i);
		readChildElementText(doc, appNode, "uri", textBuf, 512);
		sceClibSnprintf(uriBuf, sizeof(uriBuf), "%s", textBuf);

		appNode = appNodes.item(i);
		readChildElementText(doc, appNode, "version", textBuf, 512);

		SceUInt32 appVer = 0;
		SceSize verLen = sceClibStrnlen(textBuf, 1024);
		status = parseVersionString(textBuf, (unsigned int)(verLen + 1), &appVer);
		if (status < 0)
			break;

		SceTeleportRemoteAppInfo *dst = &outList->items[parsedIdx];
		sceClibMemset(dst, 0, sizeof(*dst));
		sceClibSnprintf(asChar(dst->name), sizeof(dst->name), "%s", nameBuf);
		sceClibSnprintf(asChar(dst->appId), sizeof(dst->appId), "%s", uriBuf);
		dst->appVersion = appVer;
		++parsedIdx;
		outList->parsedCount = parsedIdx;
	}
	return status;
}

int TeleportXmlParser::parseDeviceDescription(sce::Xml::Dom::Document *doc,
					      sce::Xml::Dom::NodeId rootNode,
					      SceTeleportDeviceDescription *outDesc)
{
	char textBuf[516];
	int ret;
	{
		sce::Xml::Dom::NodeList devList;
		sce::Xml::String devTag("device");
		doc->getElementsByTagName(rootNode, &devTag, &devList);
		(void)devList.getLength();

		sce::Xml::Dom::NodeId devNode = devList.item(0);
		readChildElementText(doc, devNode, "name", textBuf, 512);
		sceClibSnprintf(asChar(outDesc->name), 128, "%s", textBuf);

		devNode = devList.item(0);
		readChildElementText(doc, devNode, "version", textBuf, 512);
		SceSize verLen = sceClibStrnlen(textBuf, 64);
		ret = parseVersionString(textBuf, (unsigned int)(verLen + 1), &outDesc->systemVersion);
	}

	if (ret >= 0) {
		sce::Xml::Dom::NodeList protoList;
		sce::Xml::String protoTag("protocol");
		doc->getElementsByTagName(rootNode, &protoTag, &protoList);
		(void)protoList.getLength();

		char protoBuf[512];
		memset(protoBuf, 0, sizeof(protoBuf));

		sce::Xml::Dom::NodeId protoNode = protoList.item(0);
		readChildElementText(doc, protoNode, "auth_salt", protoBuf, sizeof(protoBuf));
		sceClibSnprintf(asChar(outDesc->innerData), sizeof(outDesc->innerData), "%s", protoBuf);

		protoNode = protoList.item(0);
		readChildElementText(doc, protoNode, "version", protoBuf, sizeof(protoBuf));
		SceSize verLen = sceClibStrnlen(protoBuf, 64);
		ret = parseVersionString(protoBuf, (unsigned int)(verLen + 1),
					 &outDesc->protocolVersion);
	}
	return ret;
}

int TeleportXmlParser::parseXml(const char *httpResp, int respLen, int mode,
				void *outData, SceUInt32 *outErrCode)
{
	TeleportXmlMemAllocator memAlloc;
	memAlloc.allocFunc = s_xmlAllocFunc;
	memAlloc.freeFunc = s_xmlFreeFunc;

	sce::Xml::InitParameter initParam;
	initParam.allocator = NULL;
	initParam.userData = NULL;

	sce::Xml::Initializer xmlInit;
	sce::Xml::XmlText xmlText;
	int ret = 0;

	const char *xmlStart = sceClibStrstr(httpResp, "<?xml ");
	if (xmlStart != NULL) {
		initParam.allocator = &memAlloc;
		initParam.userData = (void *)0xDEADBEEFu;
		ret = xmlInit.initialize(&initParam);
		if (ret >= 0) {
			void *builderMem = s_xmlAllocFunc(sizeof(sce::Xml::Dom::DocumentBuilder));
			if (builderMem != NULL) {
				sce::Xml::Dom::DocumentBuilder *builder =
					new (builderMem) sce::Xml::Dom::DocumentBuilder();
				ret = builder->initialize(&xmlInit);
				if (ret >= 0) {
					builder->setSkipIgnorableText(false);
					builder->setSkipIgnorableWhiteSpace(true);
					builder->setResolveEntity(true);

					xmlText.buffer = xmlStart;
					xmlText.size = (size_t)(respLen - (int)(xmlStart - httpResp) - 1);
					ret = builder->parse(&xmlText, true);
					if (ret >= 0) {
						if (mode == TELEPORT_XML_MODE_DEVICE_DESC) {
							sce::Xml::Dom::Document doc = builder->getDocument();
							sce::Xml::Dom::NodeId root = doc.getRoot();
							SceTeleportDeviceDescription parsed;
							if (parseDeviceDescription(&doc, root, &parsed) != 0) {
								ret = SCE_TELEPORT_ERROR_XML_PARSE;
							} else {
								SceTeleportDeviceDescription *dst =
									(SceTeleportDeviceDescription *)outData;
								sceClibSnprintf(asChar(dst->name), sizeof(dst->name),
										"%s", parsed.name);
								sceClibSnprintf(asChar(dst->innerData), sizeof(dst->innerData),
										"%s", parsed.innerData);
								dst->systemVersion = parsed.systemVersion;
								dst->protocolVersion = parsed.protocolVersion;
							}
						} else if (mode == TELEPORT_XML_MODE_APP_NUM) {
							sce::Xml::Dom::Document doc = builder->getDocument();
							sce::Xml::SerializeParameter serParam;
							sce::Xml::XmlText serOut;
							doc.serialize(&serParam, &serOut);
							sce::Xml::Dom::NodeId root = doc.getRoot();
							const char appTagStr[4] = "app";
							sce::Xml::Dom::NodeList appList;
							sce::Xml::String appTag(appTagStr);
							int findRet = doc.getElementsByTagName(root, &appTag, &appList);
							SceUInt16 count = 0;
							for (size_t i = 0; i < appList.getLength(); ++i) {
								sce::Xml::Dom::NodeId node = appList.item(i);
								sce::Xml::String nodeName = doc.getNodeName(node);
								if (sceClibStrcmp(nodeName.buffer, appTagStr) == 0)
									++count;
							}
							if (findRet != 0)
								ret = SCE_TELEPORT_ERROR_XML_PARSE;
							else
								((SceTeleportRemoteAppInfoNum *)outData)->num = count;
						} else if (mode == TELEPORT_XML_MODE_APP_LIST) {
							sce::Xml::Dom::Document doc = builder->getDocument();
							sce::Xml::Dom::NodeId root = doc.getRoot();
							if (parseRemoteAppList(&doc, root,
									       (TeleportRemoteAppList *)outData) != 0)
								ret = SCE_TELEPORT_ERROR_XML_PARSE;
						} else if (mode == TELEPORT_XML_MODE_LAUNCH_RESP) {
							sce::Xml::Dom::Document doc = builder->getDocument();
							int httpStatus = parseHttpStatusCode(httpResp, respLen);
							if (httpStatus == 200 || httpStatus == 201) {
								sce::Xml::Dom::NodeId root = doc.getRoot();
								if (parseLaunchResponse(&doc, root,
											(SceTeleportLaunchResponse *)outData) != 0)
									ret = SCE_TELEPORT_ERROR_XML_PARSE;
							} else if (httpStatus == 403) {
								SceUInt32 errCode = 0;
								sce::Xml::Dom::NodeId root = doc.getRoot();
								if (parseErrorResponse(&doc, root,
										       (SceTeleportLaunchResponse *)outData,
										       &errCode) != 0)
									ret = SCE_TELEPORT_ERROR_XML_PARSE;
								if (outErrCode != NULL)
									*outErrCode = errCode;
							} else {
								ret = SCE_TELEPORT_ERROR_XML_PARSE;
							}
						}
					}
				}
				builder->~DocumentBuilder();
				s_xmlFreeFunc(builder);
			}
		}
	}

	return ret;
}
