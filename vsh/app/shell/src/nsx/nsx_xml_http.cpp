/*
 * nsx_xml_http.cpp: NSX XML parser base class, HTTP conditional GET, and annotation helpers
 * 3.65-CEX: 0x8122CC58, 0x81230E14 - 0x812327F8
 */

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <std/list>
#include <std/map>
#include <std/vector>
#include <common/string_util.h>
#include <file/httpfile.h>
#include <rtc/rtc.h>
#include "nsx_internal.h"

struct SceKernelSystemSwVersion {
	SceSize size;
	char versionString[28];
	SceUInt32 version;
	SceUInt32 reserved;
};

extern "C" {
extern int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *swInfo);
extern int SceLsdb_60B535FD(int source, int language);
extern paf::string SceLsdb_88FDC55C(int source);
extern int SceLsdb_D8DF91EE(paf::rtc::Tick *tick);
}

const char *const kNsxTagRoot = "nsx";
const char *const kNsxAttrMinSysVer = "min-sys-ver";
const char *const kNsxTagSpace = "spc";
const char *const kNsxAttrId = "id";
const char *const kNsxTagMaterial = "mtrl";
const char *const kNsxAttrAnnotation = "anno";
const char *const kNsxAttrValidFrom = "from";
const char *const kNsxAttrValidUntil = "until";
const char *const kNsxAttrLastModified = "lastm";
const char *const kNsxTagDesc = "desc";
const char *const kNsxTagUrl = "url";
const char *const kNsxTagTarget = "target";
const char *const kNsxAttrTargetType = "type";
const char *const kNsxTagCountry = "cntry";
const char *const kNsxAttrMinAge = "agelmt";
const char *const kNsxAttrMaxAge = "uagelmt";
const char *const kNsxTagLang = "lang";
const char *const kNsxAnnoOperator = "operator";
const char *const kNsxAnnoEmergency = "emergency";
const char *const kNsxAnnoPicks = "picks";
const char *const kNsxAnnoEnabledValue = "1";

/* 0x8122CC58 */
void nsxFreeMaterialList(paf::vector<NsxMaterialItem *> *materials)
{
	for (size_t index = 0; index < materials->size(); ++index) {
		delete (*materials)[index];
	}
	materials->clear();
}

/* 0x81231A58 */
static void parseAnnotationPairs(const paf::string &annoText,
				 paf::map<paf::string, paf::string> *outMap)
{
	paf::list<paf::string> pairs;
	paf::common::string_util::tokenize(pairs, annoText, 0, '&');
	for (paf::list<paf::string>::iterator iter = pairs.begin(); iter != pairs.end(); ++iter) {
		paf::list<paf::string> keyVal;
		paf::common::string_util::tokenize(keyVal, *iter, 0, '=');
		if (keyVal.size() != 2)
			break;

		paf::list<paf::string>::iterator first = keyVal.begin();
		paf::string key = *first;
		++first;
		paf::string value = *first;
		(*outMap)[key] = value;
	}
}

/* 0x81230E14 / 0x81230EBC */
NsxXmlParserBase::NsxXmlParserBase(const paf::rtc::Tick *referenceTick)
	: m_awaitingRoot(true),
	  m_referenceTick(*referenceTick),
	  m_userLanguage(SceLsdb_60B535FD(1, -1)),
	  m_userCountry(SceLsdb_88FDC55C(1))
{
	shellXmlSaxCreate(&m_saxParser, &m_rootNode, true, true);
}

/* 0x81230EC4 / 0x812317B8 / 0x812317C8 */
NsxXmlParserBase::~NsxXmlParserBase()
{
	shellXmlSaxDestroy(&m_saxParser);
}

/* 0x812317D0 */
bool NsxXmlParserBase::ParseMaterialChildren(const NsxXmlNode *materialNode,
					     NsxMaterialItem *outMaterial)
{
	bool countryMatched = false;
	bool countryExactMatched = false;
	bool languageMatched = false;

	for (paf::list<NsxXmlNode>::iterator iter =
		     const_cast<paf::list<NsxXmlNode> &>(materialNode->children).begin();
	     iter != const_cast<paf::list<NsxXmlNode> &>(materialNode->children).end();
	     ++iter) {
		const NsxXmlNode &child = *iter;
		if (child.name == kNsxTagDesc && outMaterial->description.empty()) {
			outMaterial->description = child.text;
		} else if (child.name == kNsxTagUrl && outMaterial->iconUrl.empty()) {
			outMaterial->iconUrl = child.text;
		} else if (child.name == kNsxTagTarget && outMaterial->targetType == NSX_TARGET_NONE) {
			const paf::string *typeAttr = shellXmlNodeGetAttr(&child, kNsxAttrTargetType);
			int targetType = NSX_TARGET_NONE;
			if (*typeAttr == "p")
				targetType = NSX_TARGET_PRODUCT;
			else if (*typeAttr == "c")
				targetType = NSX_TARGET_CATEGORY;
			else if (*typeAttr == "u")
				targetType = NSX_TARGET_URI;

			outMaterial->targetType = targetType;
			outMaterial->targetUri = child.text;
		} else if (child.name == kNsxTagCountry) {
			if (!countryExactMatched) {
				if (child.text == m_userCountry)
					countryExactMatched = true;
				if (countryExactMatched || child.text == "all") {
					const paf::string *minAgeAttr =
						shellXmlNodeGetAttr(&child, kNsxAttrMinAge);
					const paf::string *maxAgeAttr =
						shellXmlNodeGetAttr(&child, kNsxAttrMaxAge);
					outMaterial->minAge =
						minAgeAttr->empty() ? 0 : (SceInt16)sce_paf_atoi(minAgeAttr->c_str());
					outMaterial->maxAge =
						maxAgeAttr->empty() ? 255 : (SceInt16)sce_paf_atoi(maxAgeAttr->c_str());
					countryMatched = true;
				}
			}
		} else if (child.name == kNsxTagLang && !languageMatched) {
			if (child.text == "all" || m_userLanguage == sce_paf_atoi(child.text.c_str()))
				languageMatched = true;
		}
	}

	return countryMatched && languageMatched;
}

/* 0x81231C3C */
bool NsxXmlParserBase::ParseMaterialNode(const NsxXmlNode *materialNode,
					 paf::vector<NsxMaterialItem *> *outMaterials)
{
	NsxMaterialItem *material = new NsxMaterialItem();
	if (material == NULL)
		return false;

	const paf::string *idAttr = shellXmlNodeGetAttr(materialNode, kNsxAttrId);
	material->materialId = sce_paf_atoi(idAttr->c_str());

	const paf::string *fromAttr = shellXmlNodeGetAttr(materialNode, kNsxAttrValidFrom);
	paf::rtc::ParseRFC3339(&material->validFromTick, fromAttr->c_str());

	const paf::string *untilAttr = shellXmlNodeGetAttr(materialNode, kNsxAttrValidUntil);
	paf::rtc::ParseRFC3339(&material->validUntilTick, untilAttr->c_str());

	const paf::string *lastmAttr = shellXmlNodeGetAttr(materialNode, kNsxAttrLastModified);
	paf::rtc::ParseRFC3339(&material->lastModifiedTick, lastmAttr->c_str());

	const paf::string *annoAttr = shellXmlNodeGetAttr(materialNode, kNsxAttrAnnotation);
	parseAnnotationPairs(*annoAttr, &material->annotations);

	if (!ParseMaterialChildren(materialNode, material) ||
	    !ValidateMaterial(material) ||
	    !ValidateWindow(material)) {
		delete material;
		return false;
	}

	outMaterials->push_back(material);
	return true;
}

/* 0x81231E16 */
bool NsxXmlParserBase::ParseSpaceMaterials(const NsxXmlNode *rootNode, bool isFinal,
					   paf::vector<NsxMaterialItem *> *outMaterials)
{
	paf::list<NsxXmlNode> &rootChildren =
		const_cast<paf::list<NsxXmlNode> &>(rootNode->children);
	NsxXmlNode *spaceNode = NULL;
	for (paf::list<NsxXmlNode>::iterator iter = rootChildren.begin();
	     iter != rootChildren.end(); ++iter) {
		spaceNode = &(*iter);
		if (spaceNode->name == kNsxTagSpace)
			break;
	}

	if (spaceNode == NULL)
		return true;

	paf::list<NsxXmlNode>::iterator childIter = spaceNode->children.begin();
	while (childIter != spaceNode->children.end()) {
		if (!isFinal && spaceNode->children.size() == 1)
			break;

		if (childIter->name == kNsxTagMaterial)
			ParseMaterialNode(&(*childIter), outMaterials);
		childIter = spaceNode->children.erase(childIter);
	}
	return true;
}

/* 0x81231EF4 */
int NsxXmlParserBase::ParseChunk(const char *buffer, SceSize size, bool isFinal,
				 paf::vector<NsxMaterialItem *> *outMaterials)
{
	if (shellXmlSaxParse(&m_saxParser, buffer, size, isFinal) != 0)
		return kNsxErrorParseFailed;

	if (m_awaitingRoot) {
		m_awaitingRoot = false;
		if (!ValidateRootNode(&m_rootNode))
			return kNsxErrorParseFailed;
	}

	if (!ParseSpaceMaterials(&m_rootNode, isFinal, outMaterials))
		return kNsxErrorParseFailed;
	return 0;
}

/* 0x81231F5C */
bool NsxXmlParserBase::ValidateRootNode(const NsxXmlNode *rootNode)
{
	return rootNode->name == kNsxTagRoot;
}

/* 0x81231F86 */
bool NsxXmlParserBase::ValidateMaterial(const NsxMaterialItem *material)
{
	if (material->materialId == 0 || material->iconUrl.empty())
		return false;
	if (material->validFromTick == 0 || material->validUntilTick == 0 ||
	    material->lastModifiedTick == 0) {
		return false;
	}
	if (material->targetType != NSX_TARGET_NONE && material->targetUri.empty())
		return false;
	return true;
}

/* 0x81231FFA */
bool NsxXmlParserBase::ValidateWindow(const NsxMaterialItem *material)
{
	return material->validFromTick <= m_referenceTick &&
	       m_referenceTick < material->validUntilTick;
}

/* 0x81232022 */
bool NsxXmlParserBase::CheckMinSystemVersion(const NsxXmlNode *rootNode)
{
	const paf::string *minVerAttr = shellXmlNodeGetAttr(rootNode, kNsxAttrMinSysVer);
	if (minVerAttr->empty())
		return true;

	SceKernelSystemSwVersion swInfo;
	sce_paf_memset(&swInfo, 0, sizeof(swInfo));
	swInfo.size = sizeof(swInfo);
	if (sceKernelGetSystemSwVersion(&swInfo) != 0)
		return false;

	return sce_paf_atof(minVerAttr->c_str()) <= sce_paf_atof(swInfo.versionString);
}

/* 0x81232480 */
int nsxHttpOpenIfModified(paf::HttpFile *httpFile, const char *url, const SceUInt64 *ifModifiedTick)
{
	paf::HttpFile::OpenArg openArg;
	int ret = openArg.ParseUrl(url);
	if (ret != 0)
		return ret;

	if (*ifModifiedTick != 0) {
		SceRtcTick utcTick;
		utcTick.tick = *ifModifiedTick;
		char dateHeader[64];
		if (sceRtcFormatRFC2822(dateHeader, &utcTick, 0) == 0)
			openArg.AddRequestHeader("If-Modified-Since", dateHeader);
	}

	return httpFile->Open(&openArg);
}

/* 0x812326B2 */
SceUInt64 nsxHttpGetLastModifiedTick(const paf::HttpFile *httpFile)
{
	paf::string lastModHeader;
	SceUInt64 resultTick = 0;
	if (httpFile->GetResponseHeader("Last-Modified", lastModHeader) == 0 &&
	    !lastModHeader.empty()) {
		SceRtcTick utcTick;
		utcTick.tick = 0;
		if (sceRtcParseDateTime(&utcTick, lastModHeader.c_str()) == 0)
			resultTick = utcTick.tick;
	}
	return resultTick;
}

/* 0x81232728 */
paf::rtc::Tick nsxGetNetworkTick(void)
{
	paf::rtc::Tick tick = 0;
	if (SceLsdb_D8DF91EE(&tick) != 0)
		return 0;
	return tick;
}

/* 0x81232766 */
int nsxGetAnnotationInt(const paf::map<paf::string, paf::string> &annotations,
			const paf::string &key, int defaultValue)
{
	paf::map<paf::string, paf::string> &mutableMap =
		const_cast<paf::map<paf::string, paf::string> &>(annotations);
	paf::map<paf::string, paf::string>::iterator iter = mutableMap.find(key);
	if (iter != mutableMap.end())
		return sce_paf_atoi(iter->second.c_str());
	return defaultValue;
}

/* 0x812327A8 */
paf::string nsxGetAnnotationString(const paf::map<paf::string, paf::string> &annotations,
				   const paf::string &key, const paf::string &defaultValue)
{
	paf::map<paf::string, paf::string> &mutableMap =
		const_cast<paf::map<paf::string, paf::string> &>(annotations);
	paf::map<paf::string, paf::string>::iterator iter = mutableMap.find(key);
	if (iter != mutableMap.end())
		return iter->second;
	return defaultValue;
}
