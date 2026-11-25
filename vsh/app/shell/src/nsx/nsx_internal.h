/*
 * nsx_internal.h: internal types and helpers for SceShell NSX (WhatsNew & PushInfo)
 * 3.65-CEX: 0x8122ACC0 - 0x812327F8
 */

#ifndef SHELL_NSX_INTERNAL_H
#define SHELL_NSX_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/list>
#include <std/map>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/httpfile.h>
#include <rtc/rtc.h>
#include <thread/thread.h>

enum {
	NSX_TARGET_PRODUCT	= 0,
	NSX_TARGET_CATEGORY	= 1,
	NSX_TARGET_URI		= 2,
	NSX_TARGET_NONE		= 3,
};

static const int kNsxErrorInvalidState = -2146397183;	/* 0x80109401 */
static const int kNsxErrorInvalidArg = -2146397182;	/* 0x80109402 */
static const int kNsxErrorNoMemory = -2146397181;	/* 0x80109403 */
static const int kNsxErrorCanceled = -2146397180;	/* 0x80109404 */
static const int kNsxErrorFileOpen = -2146397179;	/* 0x80109405 */
static const int kNsxErrorParseFailed = -2146397178;	/* 0x80109406 */
static const int kNsxErrorHttpFailed = -2146397177;	/* 0x80109407 */
static const int kNsxErrorNoNetworkTime = -2146397176;	/* 0x80109408 */
static const int kNsxErrorNotExpired = -2146397175;	/* 0x80109409 */

/* Shared XML DOM node used by shell bXCe SAX wrapper (0x81389E78..0x8138AB3C) */
struct NsxXmlNode {
	paf::string name;
	paf::map<paf::string, paf::string> attrs;
	paf::string text;
	paf::list<NsxXmlNode> children;
};

struct NsxXmlSaxParser {
	void *impl;
};

extern NsxXmlSaxParser *shellXmlSaxCreate(NsxXmlSaxParser *parser, NsxXmlNode *rootNode,
					  bool resolveEntity, bool resolveAttrEntity);
extern NsxXmlSaxParser *shellXmlSaxDestroy(NsxXmlSaxParser *parser);
extern int shellXmlSaxParse(NsxXmlSaxParser *parser, const char *chunk, SceSize size, bool isFinal);
extern const paf::string *shellXmlNodeGetAttr(const NsxXmlNode *node, const char *attrName);
extern int shellXmlNodeSerialize(paf::string *outXml, const NsxXmlNode *node, SceUInt32 indent);

/* Parsed <mtrl> element (88 bytes) */
struct NsxMaterialItem {
	NsxMaterialItem()
		: materialId(0),
		  targetType(NSX_TARGET_NONE),
		  lastModifiedTick(0),
		  validFromTick(0),
		  validUntilTick(0),
		  minAge(0),
		  maxAge(255)
	{
	}

	int materialId;
	paf::string description;
	paf::string iconUrl;
	int targetType;
	paf::string targetUri;
	paf::rtc::Tick lastModifiedTick;
	paf::rtc::Tick validFromTick;
	paf::rtc::Tick validUntilTick;
	SceInt16 minAge;
	SceInt16 maxAge;
	paf::map<paf::string, paf::string> annotations;
};

/* Base SAX parser for NSX XML documents (0x81230E14..0x812320B0) */
class NsxXmlParserBase {
public:
	NsxXmlParserBase(const paf::rtc::Tick *referenceTick);
	virtual ~NsxXmlParserBase();

	virtual bool ValidateRootNode(const NsxXmlNode *rootNode);
	virtual bool ValidateMaterial(const NsxMaterialItem *material);
	virtual bool ValidateWindow(const NsxMaterialItem *material);

	int ParseChunk(const char *buffer, SceSize size, bool isFinal,
		       paf::vector<NsxMaterialItem *> *outMaterials);

protected:
	bool CheckMinSystemVersion(const NsxXmlNode *rootNode);
	bool ParseMaterialChildren(const NsxXmlNode *materialNode, NsxMaterialItem *outMaterial);
	bool ParseMaterialNode(const NsxXmlNode *materialNode,
			       paf::vector<NsxMaterialItem *> *outMaterials);
	bool ParseSpaceMaterials(const NsxXmlNode *rootNode, bool isFinal,
				 paf::vector<NsxMaterialItem *> *outMaterials);

	bool m_awaitingRoot;
	paf::rtc::Tick m_referenceTick;
	int m_userLanguage;
	paf::string m_userCountry;
	NsxXmlSaxParser m_saxParser;
	NsxXmlNode m_rootNode;
};

/* Shared XML tag names (0x815AA460..0x815AA568) */
extern const char *const kNsxTagRoot;
extern const char *const kNsxAttrMinSysVer;
extern const char *const kNsxTagSpace;
extern const char *const kNsxAttrId;
extern const char *const kNsxTagMaterial;
extern const char *const kNsxAttrAnnotation;
extern const char *const kNsxAttrValidFrom;
extern const char *const kNsxAttrValidUntil;
extern const char *const kNsxAttrLastModified;
extern const char *const kNsxTagDesc;
extern const char *const kNsxTagUrl;
extern const char *const kNsxTagTarget;
extern const char *const kNsxAttrTargetType;
extern const char *const kNsxTagCountry;
extern const char *const kNsxAttrMinAge;
extern const char *const kNsxAttrMaxAge;
extern const char *const kNsxTagLang;
extern const char *const kNsxAnnoOperator;
extern const char *const kNsxAnnoEmergency;
extern const char *const kNsxAnnoPicks;
extern const char *const kNsxAnnoEnabledValue;

/* Shared NSX helpers (nsx_xml_http.cpp) */
int nsxHttpOpenIfModified(paf::HttpFile *httpFile, const char *url, const SceUInt64 *ifModifiedTick);
SceUInt64 nsxHttpGetLastModifiedTick(const paf::HttpFile *httpFile);
paf::rtc::Tick nsxGetNetworkTick(void);
int nsxGetAnnotationInt(const paf::map<paf::string, paf::string> &annotations,
			const paf::string &key, int defaultValue);
paf::string nsxGetAnnotationString(const paf::map<paf::string, paf::string> &annotations,
				   const paf::string &key, const paf::string &defaultValue);
void nsxFreeMaterialList(paf::vector<NsxMaterialItem *> *materials);

/* PushInfo job control (nsx_clock_updater.cpp / nsx_push_info.cpp) */
int nsxPushInfoRegisterCron(void);
int nsxPushInfoEnqueueJob(void);
int nsxClockUpdaterRegisterCron(void);
int nsxPushInfoRunCheck(void);

/* WhatsNew accessor wrapper (nsx_whats_new.cpp, 0x8122CD08..0x8122CD90) */
class NsxWhatsNewAccessorImpl;

struct NsxWhatsNewAccessor {
	NsxWhatsNewAccessor() : impl(NULL) {}
	~NsxWhatsNewAccessor();
	int Init(void (*callback)(void *), void *callbackArg);
	int Terminate();
	int LoadMaterials(paf::vector<NsxMaterialItem *> *outMaterials,
			  paf::rtc::Tick *outNextRefreshTick);
	int RequestUpdate(bool forceRefresh);

	NsxWhatsNewAccessorImpl *impl;
};

/* WhatsNew material annotation queries (nsx_whats_new.cpp) */
int nsxMaterialGetOperatorId(const NsxMaterialItem *material);
bool nsxMaterialIsEmergency(const NsxMaterialItem *material);
bool nsxMaterialIsPicks(const NsxMaterialItem *material);

#endif
