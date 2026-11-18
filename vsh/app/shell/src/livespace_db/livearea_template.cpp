/*
 * The LiveArea template's objects: a template, its parts and their liveitems with their
 * elements, and the names of the template's tags and attributes (the SDK's LiveArea
 * specification). The file and every name here are ours.
 */
#include "livearea.h"

/* a liveitem from any time for ever, without an age limit */
LiLiveItem::LiLiveItem()
	: m_from(0), m_until((SceUInt64)-1), m_default(false), m_ageLimit(0), m_uageLimit(255),
	  m_pokesute(LI_POKESUTE_NONE), m_target(NULL)
{
}

LiLiveItem::~LiLiveItem()
{
	while (m_elements.size() != 0) {
		paf::vector<LiItemElement *>::iterator last = m_elements.end();

		last--;

		LiItemElement *element = *last;

		last = m_elements.erase(last);
		delete element;
	}
}

extern "C" int SceLsdb_628CFBE1(LiLiveItem *item, bool whole, bool network)
{
	paf::rtc::Tick now;

	if (item->m_from == 0 && item->m_until == (SceUInt64)-1)
		return 1;

	int result;

	if (network)
		result = SceLsdb_D8DF91EE(&now);
	else
		result = SceLsdb_76C1997A(&now);
	if (result != 0)
		return 0;
	if (whole) {
		if (item->m_from <= item->m_until && item->m_until >= now)
			return 1;
	} else if (item->m_from <= now && item->m_until >= now) {
		return 1;
	}
	return 0;
}

bool LiLiveItem::HasElement(int type)
{
	for (paf::vector<LiItemElement *>::iterator it = m_elements.begin();
	     it != m_elements.end(); ++it) {
		if ((*it)->m_type == type)
			return true;
	}
	return false;
}

LiPart::~LiPart()
{
}

LiBackground::LiBackground() : LiPart(LI_PART_BACKGROUND)
{
}

LiBackground::~LiBackground()
{
}

unsigned int LiBackground::GetMask() const
{
	return LI_MASK_BACKGROUND;
}

LiFunctionZone::LiFunctionZone() : LiPart(LI_PART_FUNCTION_ZONE)
{
}

LiFunctionZone::~LiFunctionZone()
{
}

unsigned int LiFunctionZone::GetMask() const
{
	return LI_MASK_FUNCTION_ZONE;
}

LiFrame::LiFrame()
	: LiPart(LI_PART_FRAME), m_multi(0), m_autoflip(0), m_rev(0), m_isDefault(false)
{
}

LiFrame::~LiFrame()
{
	while (m_items.size() != 0) {
		paf::vector<LiLiveItem *>::iterator last = m_items.end();

		last--;

		LiLiveItem *item = *last;

		last = m_items.erase(last);
		delete item;
	}
}

/* the bit of the frame's id, frame1 to frame10 */
static const struct {
	const char *id;
	unsigned int mask;
} s_liFrameMasks[LI_FRAME_COUNT] = {
	{ "frame1", 0x001 },
	{ "frame2", 0x002 },
	{ "frame3", 0x004 },
	{ "frame4", 0x008 },
	{ "frame5", 0x010 },
	{ "frame6", 0x020 },
	{ "frame7", 0x040 },
	{ "frame8", 0x080 },
	{ "frame9", 0x100 },
	{ "frame10", 0x200 },
};

unsigned int LiFrame::GetMask() const
{
	for (unsigned int i = 0; i < LI_FRAME_COUNT; i++) {
		if (sce_paf_strcmp(m_id.c_str(), s_liFrameMasks[i].id) == 0)
			return s_liFrameMasks[i].mask;
	}
	return 0;
}

extern "C" LiLiveItem *SceLsdb_92D3A2EA(LiFrame *frame, int index)
{
	if (index >= (int)frame->m_items.size())
		return NULL;
	if (index < 0)
		return NULL;
	return frame->m_items[index];
}

LiGate::LiGate() : LiPart(LI_PART_GATE)
{
}

LiGate::~LiGate()
{
}

unsigned int LiGate::GetMask() const
{
	return LI_MASK_GATE;
}

/* white, half transparent */
LiTitle::LiTitle() : LiPart(LI_PART_TITLE)
{
	m_color[0] = 1.0f;
	m_color[1] = 1.0f;
	m_color[2] = 1.0f;
	m_color[3] = 0.5f;
}

LiTitle::~LiTitle()
{
}

unsigned int LiTitle::GetMask() const
{
	return LI_MASK_TITLE;
}

LiTemplate::LiTemplate() : m_contentRev(0), m_formatVersion(0), m_modifiedDate(0)
{
}

LiTemplate::~LiTemplate()
{
	while (m_parts.size() != 0) {
		LiPart *part = m_parts.back();

		m_parts.pop_back();
		delete part;
	}
}

bool LiTemplate::HasPart(int type)
{
	for (paf::list<LiPart *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		if ((*it)->m_type == type)
			return true;
	}
	return false;
}

bool LiTemplate::HasFrame(const paf::string *id)
{
	for (paf::list<LiPart *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		LiPart *part = *it;

		if (part->m_type == LI_PART_FRAME && *id == ((LiFrame *)part)->m_id)
			return true;
	}
	return false;
}

/* "NN.NN" */
static inline bool LiIsDigit(char ch)
{
	return ch >= '0' && ch <= '9';
}

bool LiIsFormatVersion(const paf::string *version)
{
	if (version->size() != 5)
		return false;

	const char *str = version->c_str();

	if (LiIsDigit(str[0]) && LiIsDigit(str[1]) && str[2] == '.' && LiIsDigit(str[3]) &&
	    LiIsDigit(str[4]) &&
	    (str[0] - '0') * 1000 + (str[1] - '0') * 100 + (str[3] - '0') * 10 +
	    (str[4] - '0') >= 100)
		return true;
	return false;
}

extern "C" int SceLsdb_19BA5A0A(const paf::string *version)
{
	if (version->size() != 5)
		return -1;

	const char *str = version->c_str();

	if (LiIsDigit(str[0]) && LiIsDigit(str[1]) && str[2] == '.' && LiIsDigit(str[3]) &&
	    LiIsDigit(str[4]))
		return (str[0] - '0') * 1000 + (str[1] - '0') * 100 + (str[3] - '0') * 10 +
			(str[4] - '0');
	return -1;
}

/* 01.00 to 01.04 */
bool LiIsSupportedFormatVersion(const paf::string *version)
{
	int number = SceLsdb_19BA5A0A(version);

	return number == 100 || number == 101 || number == 102 || number == 103 ||
	       number == 104;
}

LiItemElement::~LiItemElement()
{
}

LiTarget::LiTarget() : LiItemElement(LI_ITEM_TARGET)
{
}

LiTarget::~LiTarget()
{
}

/* a req-appver: "NN.NN" */
extern "C" int SceLsdb_87172F61(const paf::string *version)
{
	if (version->size() != 5)
		return -1;

	const char *str = version->c_str();

	if (LiIsDigit(str[0]) && LiIsDigit(str[1]) && str[2] == '.' && LiIsDigit(str[3]) &&
	    LiIsDigit(str[4]))
		return (str[0] - '0') * 1000 + (str[1] - '0') * 100 + (str[3] - '0') * 10 +
			(str[4] - '0');
	return -1;
}

LiItemBackground::LiItemBackground() : LiPlacedElement(LI_ITEM_BACKGROUND)
{
}

LiItemBackground::~LiItemBackground()
{
}

LiImage::LiImage() : LiPlacedElement(LI_ITEM_IMAGE), m_origin(LI_ITEM_BACKGROUND)
{
}

LiImage::~LiImage()
{
}

LiText::LiText()
	: LiPlacedElement(LI_ITEM_TEXT), m_textAlign(0), m_textValign(0), m_lineAlign(1),
	  m_origin(LI_ITEM_BACKGROUND), m_lineSpace(0), m_lineBreak(true), m_wordWrap(true),
	  m_ellipsis(false), m_wordScroll(false), m_preBr(true), m_marginTop(0),
	  m_marginBottom(0), m_marginLeft(0), m_marginRight(0), m_titleIndex(-1)
{
}

LiText::~LiText()
{
}

int LiAlignFromName(const paf::string *name, bool outside)
{
	int max = LI_ALIGN_INSIDE_MAX;

	if (outside)
		max = LI_ALIGN_OUTSIDE_MAX;

	int align = 0;

	do {
		if (name->c_str()[0] == g_liAlignNames[align].c_str()[0] &&
		    sce_paf_strcmp(name->c_str(), g_liAlignNames[align].c_str()) == 0)
			return align;
		align++;
	} while (align <= max);
	return LI_ALIGN_INVALID;
}

int LiValignFromName(const paf::string *name, bool outside)
{
	int max = LI_ALIGN_INSIDE_MAX;

	if (outside)
		max = LI_ALIGN_OUTSIDE_MAX;

	int valign = 0;

	do {
		if (name->c_str()[0] == g_liValignNames[valign].c_str()[0] &&
		    sce_paf_strcmp(name->c_str(), g_liValignNames[valign].c_str()) == 0)
			return valign;
		valign++;
	} while (valign <= max);
	return LI_ALIGN_INVALID;
}

const paf::string *LiAlignName(int align)
{
	return &g_liAlignNames[align];
}

const paf::string *LiValignName(int valign)
{
	return &g_liValignNames[valign];
}

/* the element named, LI_ITEM_KIND_COUNT + 1 for none (Sony's search reads one name past the
   table: the first of g_liOnOff, never an origin) */
int LiOriginFromName(const paf::string *name)
{
	for (int origin = 0; origin <= LI_ITEM_KIND_COUNT; origin++) {
		if (g_liOriginNames[origin].size() != 0 &&
		    name->c_str()[0] == g_liOriginNames[origin].c_str()[0] &&
		    sce_paf_strcmp(name->c_str(), g_liOriginNames[origin].c_str()) == 0)
			return origin;
	}
	return LI_ITEM_KIND_COUNT + 1;
}

const paf::string *LiOriginName(int origin)
{
	return &g_liOriginNames[origin];
}

/* 0 off, 1 on */
int LiOnOffFromName(const paf::string *name)
{
	for (unsigned int on = 0; on < 2; on++) {
		if (name->c_str()[0] == g_liOnOff[on].c_str()[0] &&
		    sce_paf_strcmp(name->c_str(), g_liOnOff[on].c_str()) == 0)
			return on;
	}
	return LI_ON_OFF_INVALID;
}

const paf::string *LiOnOffName(bool on)
{
	if (on)
		return &g_liOnOff[1];
	return &g_liOnOff[0];
}

paf::string g_liOnOff[2] = { "off", "on" };
paf::string g_liTagLivearea("livearea");
paf::string g_liTagBackground("livearea-background");
paf::string g_liTagFunctionZone("system-function-zone");
paf::string g_liTagFrame("frame");
paf::string g_liTagGate("gate");
paf::string g_liTagTitle("title");
paf::string g_liTagLiveitem("liveitem");
paf::string g_liAttrStyle("style");
paf::string g_liAttrFormatVer("format-ver");
paf::string g_liAttrContentRev("content-rev");
paf::string g_liTagBackgroundImage("image");
paf::string g_liAttrBackgroundDefault("default");
paf::string g_liAttrBackgroundPath("path");
paf::string g_liTagFunc("func");
paf::string g_liAttrFuncTarget("target");
paf::string g_liFuncNames[9] = {
	"patch", "upgrade", "search", "voucher-code", "online-manual", "manual", "reload",
	"import-savedata", "online-storage"
};
paf::string g_liAttrFrameId("id");
paf::string g_liAttrFrameMulti("multi");
paf::string g_liAttrFrameAutoflip("autoflip");
paf::string g_liAttrFrameRev("rev");
paf::string g_liAttrFrameUserdata("userdata");
paf::string g_liMultiNames[2] = { "o", "r" };
paf::string g_liTagStartupImage("startup-image");
paf::string g_liAttrGateDefault("default");
paf::string g_liAttrGatePath("path");
paf::string g_liAttrTitleColor("color");
paf::string g_liAttrFrom("from");
paf::string g_liAttrUntil("until");
paf::string g_liAttrItemDefault("default");
/* a liveitem's from when it has none (from any time) */
paf::rtc::Tick g_liFromAny;
paf::string g_liTagCntry("cntry");
paf::string g_liTagExcludeCntry("exclude-cntry");
paf::string g_liTagLang("lang");
paf::string g_liTagExcludeLang("exclude-lang");
paf::string g_liTagAgeLimit("age-limit");
paf::string g_liTagUageLimit("uage-limit");
paf::string g_liTagModel("model");
paf::string g_liTagPokesute("pokesute");
paf::string g_liAttrCntryLang("lang");
paf::string g_liAttrCntryAgeLimit("age-limit");
paf::string g_liAttrCntryUageLimit("uage-limit");
paf::string g_liAttrExcludeCntryLang("lang");
paf::string g_liTagStr("str");
paf::string g_liAttrStrColor("color");
paf::string g_liAttrStrSize("size");
paf::string g_liAttrStrBold("bold");
paf::string g_liAttrStrOblique("oblique");
paf::string g_liAttrStrUnderline("underline");
paf::string g_liAttrStrShadow("shadow");
paf::string g_liAttrStrEmboss("emboss");
paf::string g_liTagTextTitle("title");
paf::string g_liAlignNames[LI_ALIGN_COUNT] = {
	"center", "left", "right", "outside-left", "outside-right"
};
paf::string g_liValignNames[LI_ALIGN_COUNT] = {
	"center", "top", "bottom", "outside-top", "outside-bottom"
};
paf::string g_liTagItemBackground("background");
paf::string g_liTagTarget("target");
paf::string g_liTagImage("image");
paf::string g_liTagText("text");
paf::string g_liOriginNames[LI_ITEM_KIND_COUNT] = {
	"", "", g_liTagFrame, "", g_liTagItemBackground, g_liTagImage, g_liTagText
};
paf::string g_liAttrAlign("align");
paf::string g_liAttrValign("valign");
paf::string g_liAttrWidth("width");
paf::string g_liAttrHeight("height");
paf::string g_liAttrX("x");
paf::string g_liAttrY("y");
paf::string g_liAttrReqAppVer("req-appver");
paf::string g_liAttrImageOrigin("origin");
paf::string g_liAttrTextAlign("text-align");
paf::string g_liAttrTextValign("text-valign");
paf::string g_liAttrLineAlign("line-align");
paf::string g_liAttrTextOrigin("origin");
paf::string g_liAttrLineSpace("line-space");
paf::string g_liAttrLineBreak("line-break");
paf::string g_liAttrWordWrap("word-wrap");
paf::string g_liAttrEllipsis("ellipsis");
paf::string g_liAttrWordScroll("word-scroll");
paf::string g_liAttrPreBr("pre-br");
paf::string g_liAttrMarginTop("margin-top");
paf::string g_liAttrMarginBottom("margin-bottom");
paf::string g_liAttrMarginLeft("margin-left");
paf::string g_liAttrMarginRight("margin-right");
