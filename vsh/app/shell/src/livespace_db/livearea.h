/*
 * The LiveArea templates (sce_sys/livearea/contents/template.xml): the parsed document, its
 * parts (the livearea tag's children) and each frame's liveitems with their elements. The SDK's
 * LiveArea specification names the tags and attributes; nothing names the classes, members or
 * files, which are ours, after the tags (Li: Sony's prefix of the LiveArea daemon,
 * SceLsdbLiDaemon).
 */
#ifndef LIVEAREA_H
#define LIVEAREA_H

#include <std/string>
#include <std/vector>
#include <std/list>
#include <common/shared_ptr.h>
#include <graphics/text/attributed_string.h>
#include <scetypes.h>
#include <system_param.h>
#include <datetime/datetime.h>
#include "xml_tree.h"

/* the parts of a template, and the bit each one's GetMask gives (a frame's: its frameN) */
#define LI_PART_BACKGROUND	1	/* livearea-background */
#define LI_PART_FUNCTION_ZONE	2	/* system-function-zone */
#define LI_PART_GATE		3
#define LI_PART_FRAME		4
#define LI_PART_TITLE		5

#define LI_MASK_BACKGROUND	0x10000
#define LI_MASK_FUNCTION_ZONE	0x20000
#define LI_MASK_GATE		0x40000
#define LI_MASK_TITLE		0x80000
#define LI_MASK_STYLE		0x100000	/* a new style: the stored frames dropped */

/* the frames, frame1 to frame10 */
#define LI_FRAME_COUNT		10

/* the liveitems a frame's tbl_livearea_frame row holds */
#define LI_ROW_ITEM_COUNT	10

/* the elements of a liveitem; an origin names the frame or one of them */
#define LI_ITEM_FRAME		2
#define LI_ITEM_TARGET		3
#define LI_ITEM_BACKGROUND	4
#define LI_ITEM_IMAGE		5
#define LI_ITEM_TEXT		6
#define LI_ITEM_KIND_COUNT	7

/* the alignments: align's and valign's names, the outside ones only for an image and a text */
#define LI_ALIGN_CENTER		0
#define LI_ALIGN_OUTSIDE_MAX	4
#define LI_ALIGN_INSIDE_MAX	2
#define LI_ALIGN_INVALID	6
#define LI_ALIGN_COUNT		5

/* an on or off attribute's values */
#define LI_ON_OFF_INVALID	3

/* a liveitem's pokesute tag: none, on or off */
#define LI_POKESUTE_NONE	1
#define LI_POKESUTE_ON		2
#define LI_POKESUTE_OFF		3

/* a part of a template */
class LiPart {
public:
	LiPart(int type) : m_type(type)
	{
	}

	virtual ~LiPart();

	virtual unsigned int GetMask() const = 0;

	int m_type;			/* LI_PART_* */
};

/* livearea-background: its image */
class LiBackground : public LiPart {
public:
	LiBackground();
	virtual ~LiBackground();
	virtual unsigned int GetMask() const;

	paf::string m_image;
};

/* a function of the system-function-zone: its func, and its target */
struct LiFunction {
	LiFunction()		/* func left to its user */
	{
	}

	int func;
	paf::string target;
};

/* system-function-zone: its functions */
class LiFunctionZone : public LiPart {
public:
	LiFunctionZone();
	virtual ~LiFunctionZone();

	/* its function of func, NULL without one */
	LiFunction *GetFunction(int func)
	{
		for (paf::vector<LiFunction>::iterator it = m_functions.begin();
		     it != m_functions.end(); ++it) {
			if (it->func == func)
				return &*it;
		}
		return NULL;
	}
	virtual unsigned int GetMask() const;

	paf::vector<LiFunction> m_functions;
};

/* gate: its startup-image */
class LiGate : public LiPart {
public:
	LiGate();
	virtual ~LiGate();
	virtual unsigned int GetMask() const;

	paf::string m_startupImage;
};

/* title: its color (red, green, blue and alpha) */
class LiTitle : public LiPart {
public:
	LiTitle();
	virtual ~LiTitle();
	virtual unsigned int GetMask() const;

	float m_color[4];
};

/* an element of a liveitem */
class LiItemElement {
public:
	LiItemElement(int type) : m_type(type)
	{
	}

	virtual ~LiItemElement();

	int m_type;			/* LI_ITEM_* */
};

/* a liveitem's target: its URI, and the application version it asks for */
class LiTarget : public LiItemElement {
public:
	LiTarget();
	virtual ~LiTarget();

	paf::string m_uri;
	paf::string m_reqAppVer;
};

/* an element placed in its frame: its alignments, its size and its position */
class LiPlacedElement : public LiItemElement {
public:
	LiPlacedElement(int type)
		: LiItemElement(type), m_align(LI_ALIGN_CENTER), m_valign(LI_ALIGN_CENTER),
		  m_width(0), m_height(0), m_x(0), m_y(0), m_hasX(false), m_hasY(false)
	{
	}

	int m_align;
	int m_valign;
	int m_width;
	int m_height;
	int m_x;
	int m_y;
	bool m_hasX;
	bool m_hasY;
};

/* a liveitem's background: its image */
class LiItemBackground : public LiPlacedElement {
public:
	LiItemBackground();
	virtual ~LiItemBackground();

	paf::string m_image;
};

/* a liveitem's image: the image, and the element it is placed from */
class LiImage : public LiPlacedElement {
public:
	LiImage();
	virtual ~LiImage();

	paf::string m_image;
	int m_origin;			/* LI_ITEM_* */
};

/* a liveitem's text: its layout and its strings */
class LiText : public LiPlacedElement {
public:
	LiText();
	virtual ~LiText();

	int m_textAlign;
	int m_textValign;
	int m_lineAlign;
	int m_origin;			/* LI_ITEM_* */
	int m_lineSpace;
	bool m_lineBreak;
	bool m_wordWrap;
	bool m_ellipsis;
	bool m_wordScroll;
	bool m_preBr;
	int m_marginTop;
	int m_marginBottom;
	int m_marginLeft;
	int m_marginRight;
	int m_titleIndex;		/* the title's str, -1 without one */
	paf::graph::AttributedString m_string;
};

/* a frame's liveitem: when it shows, where, and its elements */
class LiLiveItem {
public:
	LiLiveItem();
	~LiLiveItem();

	bool HasElement(int type);

	/* its element of type, NULL without one */
	LiItemElement *GetElement(int type)
	{
		for (paf::vector<LiItemElement *>::iterator it = m_elements.begin();
		     it != m_elements.end(); ++it) {
			if ((*it)->m_type == type)
				return *it;
		}
		return NULL;
	}

	SceUInt64 m_from;		/* 0: from any time */
	SceUInt64 m_until;		/* -1: for ever */
	bool m_default;
	short m_ageLimit;		/* its age-limit, 0 without one */
	short m_uageLimit;		/* its uage-limit, 255 without one */
	short m_pokesute;		/* LI_POKESUTE_* */
	LiTarget *m_target;
	paf::vector<LiItemElement *> m_elements;
};

/* frame: its id, its flip, revision and user data, and its liveitems */
class LiFrame : public LiPart {
public:
	LiFrame();
	virtual ~LiFrame();
	virtual unsigned int GetMask() const;

	paf::string m_id;
	int m_multi;
	int m_autoflip;
	SceInt64 m_rev;
	bool m_isDefault;		/* its type is 0: the default frame */
	paf::string m_userdata;
	paf::vector<LiLiveItem *> m_items;
};

/* a template: the livearea tag's attributes, and its parts */
class LiTemplate {
public:
	LiTemplate();
	~LiTemplate();

	bool HasPart(int type);
	bool HasFrame(const paf::string *id);

	/* its part of type, NULL without one */
	LiPart *GetPart(int type)
	{
		for (paf::list<LiPart *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
			if ((*it)->m_type == type)
				return *it;
		}
		return NULL;
	}

	paf::string m_style;
	paf::string m_formatVer;
	SceInt64 m_contentRev;
	int m_formatVersion;		/* format-ver's number (LiFormatVersion) */
	paf::list<LiPart *> m_parts;
	SceUInt64 m_modifiedDate;	/* an rtc tick, 0 without one */
};

/* reads a template.xml into a LiTemplate: the file, whether it is a system application's, and
   the liveitems kept (its language, country and age, now, the pokesute) */
class LiParser {
public:
	LiParser(const char *path, bool system, bool useDefault);
	~LiParser();

	int Parse();
	int SetFilter(const int *language, const char *country, int age);
	int ParseFrameOnly(XmlElement *element);
	int SetDefaultPath();

	void Error(const char *format, ...);
	int ParseTitle(XmlElement *element);
	int ParseLiveitem(XmlElement *element, LiFrame *frame, bool isDefault, int *limits,
			  int pokesute);
	int ParseFrame(XmlElement *element);
	int ParseGate(XmlElement *element, bool isDefault);
	int ParseFunctionZone(XmlElement *element);
	int ParseBackground(XmlElement *element, bool isDefault);
	int ParseParts(XmlElement *root, bool last);
	int ParseLivearea(XmlElement *root);

	paf::string m_path;
	bool m_system;		/* a system application's: its function zone and model */
	bool m_useDefault;		/* the default template when the file cannot be read */
	bool m_isDefault;		/* the default template read */
	paf::common::SharedPtr<LiTemplate> m_template;
	const int *m_language;		/* NULL: any */
	const char *m_country;		/* NULL: any */
	int m_age;			/* -1: any */
	bool m_checkTime;		/* only the liveitems shown now */
	bool m_keepAll;			/* every liveitem, unfiltered */
	bool m_supportedFormatOnly;	/* only the format-vers this firmware supports */
	bool m_networkTime;		/* now by the network's time */
	int m_pokesute;			/* 0: any, else the LI_POKESUTE_* kept */
	int m_languageValue;
	paf::string m_countryValue;
};

/* a format-ver's number ("01.03": 103), -1 when it is not one */
int LiFormatVersion(const paf::string *version);
bool LiIsFormatVersion(const paf::string *version);
bool LiIsSupportedFormatVersion(const paf::string *version);

int LiAlignFromName(const paf::string *name, bool outside);
int LiValignFromName(const paf::string *name, bool outside);
const paf::string *LiAlignName(int align);
const paf::string *LiValignName(int valign);
int LiOriginFromName(const paf::string *name);
const paf::string *LiOriginName(int origin);
int LiOnOffFromName(const paf::string *name);
const paf::string *LiOnOffName(bool on);

/* the tags' and attributes' names */
extern paf::string g_liOnOff[2];
extern paf::string g_liTagLivearea;
extern paf::string g_liTagBackground;
extern paf::string g_liTagFunctionZone;
extern paf::string g_liTagFrame;
extern paf::string g_liTagGate;
extern paf::string g_liTagTitle;
extern paf::string g_liTagLiveitem;
extern paf::string g_liAttrStyle;
extern paf::string g_liAttrFormatVer;
extern paf::string g_liAttrContentRev;
extern paf::string g_liTagBackgroundImage;
extern paf::string g_liAttrBackgroundDefault;
extern paf::string g_liAttrBackgroundPath;
extern paf::string g_liTagFunc;
extern paf::string g_liAttrFuncTarget;
extern paf::string g_liFuncNames[9];
extern paf::string g_liAttrFrameId;
extern paf::string g_liAttrFrameMulti;
extern paf::string g_liAttrFrameAutoflip;
extern paf::string g_liAttrFrameRev;
extern paf::string g_liAttrFrameUserdata;
extern paf::string g_liMultiNames[2];
extern paf::string g_liTagStartupImage;
extern paf::string g_liAttrGateDefault;
extern paf::string g_liAttrGatePath;
extern paf::string g_liAttrTitleColor;
extern paf::string g_liAttrFrom;
extern paf::string g_liAttrUntil;
extern paf::string g_liAttrItemDefault;
extern paf::string g_liTagCntry;
extern paf::string g_liTagExcludeCntry;
extern paf::string g_liTagLang;
extern paf::string g_liTagExcludeLang;
extern paf::string g_liTagAgeLimit;
extern paf::string g_liTagUageLimit;
extern paf::string g_liTagModel;
extern paf::string g_liTagPokesute;
extern paf::string g_liAttrCntryLang;
extern paf::string g_liAttrCntryAgeLimit;
extern paf::string g_liAttrCntryUageLimit;
extern paf::string g_liAttrExcludeCntryLang;
extern paf::string g_liTagStr;
extern paf::string g_liAttrStrColor;
extern paf::string g_liAttrStrSize;
extern paf::string g_liAttrStrBold;
extern paf::string g_liAttrStrOblique;
extern paf::string g_liAttrStrUnderline;
extern paf::string g_liAttrStrShadow;
extern paf::string g_liAttrStrEmboss;
extern paf::string g_liTagTextTitle;
extern paf::string g_liAlignNames[LI_ALIGN_COUNT];
extern paf::string g_liValignNames[LI_ALIGN_COUNT];
extern paf::string g_liTagItemBackground;
extern paf::string g_liTagTarget;
extern paf::string g_liTagImage;
extern paf::string g_liTagText;
extern paf::string g_liOriginNames[LI_ITEM_KIND_COUNT];
extern paf::string g_liAttrAlign;
extern paf::string g_liAttrValign;
extern paf::string g_liAttrWidth;
extern paf::string g_liAttrHeight;
extern paf::string g_liAttrX;
extern paf::string g_liAttrY;
extern paf::string g_liAttrReqAppVer;
extern paf::string g_liAttrImageOrigin;
extern paf::string g_liAttrTextAlign;
extern paf::string g_liAttrTextValign;
extern paf::string g_liAttrLineAlign;
extern paf::string g_liAttrTextOrigin;
extern paf::string g_liAttrLineSpace;
extern paf::string g_liAttrLineBreak;
extern paf::string g_liAttrWordWrap;
extern paf::string g_liAttrEllipsis;
extern paf::string g_liAttrWordScroll;
extern paf::string g_liAttrPreBr;
extern paf::string g_liAttrMarginTop;
extern paf::string g_liAttrMarginBottom;
extern paf::string g_liAttrMarginLeft;
extern paf::string g_liAttrMarginRight;

/* livearea_parser.cpp: the path and whitespace strings */
extern paf::string g_liDotDotSlash;
extern paf::string g_liSlash;
extern paf::string g_liCrLf;
extern paf::string g_liLf;
extern paf::string g_liTab;
extern paf::string g_liSpace;

void LiParserError(LiParser *parser, const char *format, ...);
bool LiIsFilterTag(const paf::string *name);
void LiRemoveNewlines(paf::string *str);
void LiRemoveWhitespace(paf::string *str);
int LiParsePlacement(const paf::string *tag, const XmlElement::AttrMap::iterator *attr,
		     LiPlacedElement *element, LiParser *parser, bool outside);
LiText *LiParseText(XmlElement *element, LiLiveItem *item, LiParser *parser);
int LiCheckPath(LiParser *parser, paf::string *path);
LiImage *LiParseImage(XmlElement *element, LiLiveItem *item, LiParser *parser);
LiItemBackground *LiParseItemBackground(XmlElement *element, LiLiveItem *item, LiParser *parser);
LiTarget *LiParseTarget(XmlElement *element, LiLiveItem *item, LiParser *parser);
int LiParseItemElements(LiLiveItem *item, XmlElement *element, const int *limits, int pokesute,
			LiParser *parser);
int LiIsFrameAllowed(const paf::string *style, const paf::string *id);
int LiParseLiveitemElement(LiLiveItem *item, XmlElement *element, const int *limits,
			   int pokesute, LiParser *parser);

/* livearea_filter.cpp: makes and frees the time exports' object (with the network tick exports) */
void LsdbTimeInit(void);
void LsdbTimeTerm(void);

/* a language code's system language (the LiveArea's language table), -1 for another code */
int LsdbLanguageFromCode(const paf::string *code);

/* a liveitem's country, and its age limits there when its country is the system's */
struct LiAgeLimits {
	int ageLimit;
	int uageLimit;
	bool hasAgeLimit;
	bool hasUageLimit;
};

int LiIsExcludedCountry(const paf::string *country, const char *systemCountry,
			XmlElement::AttrMap *attrs, const int *language);
int LiCheckCountry(const paf::string *country, const char *systemCountry,
		   XmlElement::AttrMap *attrs, const int *language, LiAgeLimits *limits);

#define LI_LANGUAGE_COUNT	(SCE_SYSTEM_PARAM_LANG_TURKISH + 1)

extern paf::string g_liAll;
extern paf::string g_liLanguages[LI_LANGUAGE_COUNT];

/* a liveitem's filter tags: its country, language and age (limits: its age-limit and
   uage-limit, or NULL), its model, and its pokesute (result: the LI_POKESUTE_* it shows in) */
int LiFilterLiveitem(LiParser *parser, paf::list<XmlElement> *children, const int *language,
		     const char *country, int age, int *limits);
int LiCheckModel(LiParser *parser, paf::list<XmlElement> *children);
int LiCheckPokesute(LiParser *parser, paf::list<XmlElement> *children, int formatVersion,
		    int pokesute, int *result);

/* livearea_api.cpp: a liveitem as its template's XML, and read from it */
extern paf::rtc::Tick g_liFromAny;
int LiWritePlacement(XmlElement *element, LiPlacedElement *placed);
int LiWriteLiveitem(paf::string *out, LiLiveItem *item, bool system);
bool LiReadLiveitem(LiLiveItem *item, const char *xml, bool anyModel, int age,
		    int formatVersion, int pokesute);

extern "C" {

int SceLsdb_5BDC6351(LiFrame **frame, const char *xml, int size, const int *language,
		     const char *country, int age, bool system);

/* a liveitem's time: from and until round now (whole: until only); 1 when it shows */
int SceLsdb_628CFBE1(LiLiveItem *item, bool whole, bool network);
LiLiveItem *SceLsdb_92D3A2EA(LiFrame *frame, int index);
int SceLsdb_19BA5A0A(const paf::string *version);
int SceLsdb_87172F61(const paf::string *version);

/* the network time (D8DF91EE); 76C1997A: paf's when the debug network time is not set */
int SceLsdb_D8DF91EE(paf::rtc::Tick *tick);
int SceLsdb_76C1997A(paf::rtc::Tick *tick);

}

#endif
