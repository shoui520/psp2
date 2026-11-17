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
#include <graphics/text/attributed_string.h>
#include <scetypes.h>
#include <datetime/datetime.h>

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

/* the frames, frame1 to frame10 */
#define LI_FRAME_COUNT		10

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
	int func;
	paf::string target;
};

/* system-function-zone: its functions */
class LiFunctionZone : public LiPart {
public:
	LiFunctionZone();
	virtual ~LiFunctionZone();
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

/* where an element sits in its frame: its alignments, its size and its position */
struct LiPlacement {
	LiPlacement()
		: align(LI_ALIGN_CENTER), valign(LI_ALIGN_CENTER), width(0), height(0), x(0), y(0),
		  hasX(false), hasY(false)
	{
	}

	int align;
	int valign;
	int width;
	int height;
	int x;
	int y;
	bool hasX;
	bool hasY;
};

/* a liveitem's background: its image */
class LiItemBackground : public LiItemElement {
public:
	LiItemBackground();
	virtual ~LiItemBackground();

	LiPlacement m_place;
	paf::string m_image;
};

/* a liveitem's image: the image, and the element it is placed from */
class LiImage : public LiItemElement {
public:
	LiImage();
	virtual ~LiImage();

	LiPlacement m_place;
	paf::string m_image;
	int m_origin;			/* LI_ITEM_* */
};

/* a liveitem's text: its layout and its strings */
class LiText : public LiItemElement {
public:
	LiText();
	virtual ~LiText();

	LiPlacement m_place;
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
	int m_strCount;			/* -1 until a str */
	paf::graph::AttributedString m_string;
};

/* a frame's liveitem: when it shows, where, and its elements */
class LiLiveItem {
public:
	LiLiveItem();
	~LiLiveItem();

	bool HasElement(int type);

	SceUInt64 m_from;		/* 0: from any time */
	SceUInt64 m_until;		/* -1: for ever */
	bool m_default;
	short m_country;
	short m_language;
	short m_ageLimit;
	int m_reserved;
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
	bool m_hasRev;
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

	paf::string m_style;
	paf::string m_formatVer;
	SceInt64 m_contentRev;
	int m_reserved;
	paf::list<LiPart *> m_parts;
	SceInt64 m_reserved2;
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

extern "C" {

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
