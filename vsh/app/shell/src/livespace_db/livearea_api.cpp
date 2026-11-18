/*
 * A liveitem written back as the XML of its template (the LiveArea daemon keeps each frame's
 * liveitems so in its database), read from that XML again, and a frame read from a template's
 * XML.
 */
#include <std/string>

/* Sony's build calls the wide string's destructor out of line here (LiWriteLiveitem's texts) */
namespace paf {
template<> inline __attribute__((noinline)) wstring::~basic_string()
{
	if (ptr != &empty_string)
		alloc.deallocate(ptr, count + 1);
	ptr = (wchar_t *)-1;
	count = (size_type)-1;
}
}

#include <std/stdio.h>
#include <std/stdlib.h>
#include <common/char_encoding.h>
#include <rtc.h>
#include "livespace_db_internal.h"

/* the attributes of an element placed in its frame that are not their defaults */
int LiWritePlacement(XmlElement *element, LiPlacedElement *placed)
{
	if (placed->m_align < LI_ALIGN_COUNT)
		element->SetAttribute(g_liAttrAlign.c_str(), LiAlignName(placed->m_align)->c_str());
	if (placed->m_valign < LI_ALIGN_COUNT)
		element->SetAttribute(g_liAttrValign.c_str(),
				      LiValignName(placed->m_valign)->c_str());
	if (placed->m_width > 0) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", placed->m_width);
		element->SetAttribute(g_liAttrWidth.c_str(), value);
	}
	if (placed->m_height > 0) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", placed->m_height);
		element->SetAttribute(g_liAttrHeight.c_str(), value);
	}
	if (placed->m_hasX) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", placed->m_x);
		element->SetAttribute(g_liAttrX.c_str(), value);
	}
	if (placed->m_hasY) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", placed->m_y);
		element->SetAttribute(g_liAttrY.c_str(), value);
	}
	return 1;
}

/* a time attribute of the liveitem tag: name="RFC 3339 time"; 0 when the time is not one */
static inline __attribute__((always_inline)) int LiWriteTime(paf::string *out,
							     const paf::string *name,
							     const paf::rtc::Tick *tick)
{
	char value[64];

	*out += " ";
	out->append(*name);
	*out += "=\"";
	value[0] = '\0';
	if (paf::rtc::FormatRFC3339(value, tick, 0) != 0)
		return 0;
	*out += value;
	*out += "\"";
	return 1;
}

/* a liveitem's text: its layout's attributes, and a str (or the text-title) per style run */
static inline __attribute__((always_inline)) int LiWriteText(paf::string *out, LiText *text)
{
	XmlElement element;

	if (!LiWritePlacement(&element, text))
		return 0;
	element.name = g_liTagText;
	if (text->m_textAlign <= LI_ALIGN_INSIDE_MAX)
		element.SetAttribute(g_liAttrTextAlign.c_str(),
				     LiAlignName(text->m_textAlign)->c_str());
	if (text->m_textValign <= LI_ALIGN_INSIDE_MAX)
		element.SetAttribute(g_liAttrTextValign.c_str(),
				     LiValignName(text->m_textValign)->c_str());
	if (text->m_lineAlign <= LI_ALIGN_INSIDE_MAX)
		element.SetAttribute(g_liAttrLineAlign.c_str(),
				     LiAlignName(text->m_lineAlign)->c_str());
	if (text->m_origin < LI_ITEM_KIND_COUNT)
		element.SetAttribute(g_liAttrTextOrigin.c_str(),
				     LiOriginName(text->m_origin)->c_str());
	if (text->m_lineSpace > 0) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", text->m_lineSpace);
		element.SetAttribute(g_liAttrLineSpace.c_str(), value);
	}
	element.SetAttribute(g_liAttrLineBreak.c_str(), LiOnOffName(text->m_lineBreak)->c_str());
	element.SetAttribute(g_liAttrWordWrap.c_str(), LiOnOffName(text->m_wordWrap)->c_str());
	element.SetAttribute(g_liAttrEllipsis.c_str(), LiOnOffName(text->m_ellipsis)->c_str());
	element.SetAttribute(g_liAttrWordScroll.c_str(), LiOnOffName(text->m_wordScroll)->c_str());
	element.SetAttribute(g_liAttrPreBr.c_str(), LiOnOffName(text->m_preBr)->c_str());
	if (text->m_marginTop > 0) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", text->m_marginTop);
		element.SetAttribute(g_liAttrMarginTop.c_str(), value);
	}
	if (text->m_marginBottom > 0) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", text->m_marginBottom);
		element.SetAttribute(g_liAttrMarginBottom.c_str(), value);
	}
	if (text->m_marginLeft > 0) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", text->m_marginLeft);
		element.SetAttribute(g_liAttrMarginLeft.c_str(), value);
	}
	if (text->m_marginRight > 0) {
		char value[64];

		sce_paf_snprintf(value, sizeof(value), "%d", text->m_marginRight);
		element.SetAttribute(g_liAttrMarginRight.c_str(), value);
	}

	paf::wstring str;
	paf::_list_iterator<paf::graph::TextStyleRun *> run;
	paf::_list_iterator<paf::graph::TextStyleRun *> last;

	if (text->m_string.GetString(str) != 0)
		return 0;
	if (text->m_string.GetStyleRuns(run, last) != 0)
		return 0;

	unsigned int pos = 0;
	int index = 0;

	for (; run != last; ++run, ++index) {
		paf::graph::TextStyleRun *styleRun = *run;
		paf::math::v4 color;
		float point;
		bool bold;
		bool oblique;
		bool underline;
		bool shadow;
		bool emboss;
		char colorValue[64];
		char pointValue[64];

		if (styleRun->style.GetStyleAttribute(paf::graph::TextStyleAttribute_Color,
						      color) != 0 ||
		    styleRun->style.GetStyleAttribute(paf::graph::TextStyleAttribute_Point,
						      point) != 0 ||
		    styleRun->style.GetStyleAttribute(paf::graph::TextStyleAttribute_Bold,
						      bold) != 0 ||
		    styleRun->style.GetStyleAttribute(paf::graph::TextStyleAttribute_Oblique,
						      oblique) != 0 ||
		    styleRun->style.GetStyleAttribute(paf::graph::TextStyleAttribute_Underline,
						      underline) != 0 ||
		    styleRun->style.GetStyleAttribute(paf::graph::TextStyleAttribute_Shadow,
						      shadow) != 0 ||
		    styleRun->style.GetStyleAttribute(paf::graph::TextStyleAttribute_Emboss,
						      emboss) != 0)
			return 0;

		XmlElement child;

		if (index == text->m_titleIndex)
			child.name = g_liTagTextTitle;
		else
			child.name = g_liTagStr;
		if (index != text->m_titleIndex) {
			paf::wstring part(str, pos, styleRun->len);

			paf::common::Utf16ToUtf8(part, &child.text);
		}
		color = color * 255.0f;
		sce_paf_snprintf(colorValue, sizeof(colorValue), "#%02x%02x%02x",
				 (unsigned int)color.extract_x(), (unsigned int)color.extract_y(),
				 (unsigned int)color.extract_z());
		child.SetAttribute(g_liAttrStrColor.c_str(), colorValue);
		sce_paf_snprintf(pointValue, sizeof(pointValue), "%f", (double)point);
		child.SetAttribute(g_liAttrStrSize.c_str(), pointValue);
		child.SetAttribute(g_liAttrStrBold.c_str(), LiOnOffName(bold)->c_str());
		child.SetAttribute(g_liAttrStrOblique.c_str(), LiOnOffName(oblique)->c_str());
		child.SetAttribute(g_liAttrStrUnderline.c_str(), LiOnOffName(underline)->c_str());
		child.SetAttribute(g_liAttrStrShadow.c_str(), LiOnOffName(shadow)->c_str());
		child.SetAttribute(g_liAttrStrEmboss.c_str(), LiOnOffName(emboss)->c_str());
		element.children.push_back(child);
		pos += styleRun->len;
	}
	return XmlWriteElement(out, &element, 1) == 0;
}

/*
 * A liveitem as its template's XML, appended to out; 0 when an element cannot be written
 * (system, whether it is a system application's, is not read)
 */
int LiWriteLiveitem(paf::string *out, LiLiveItem *item, bool system)
{
	*out += "<";
	out->append(g_liTagLiveitem);
	if (item->m_from != g_liFromAny) {
		if (!LiWriteTime(out, &g_liAttrFrom, &item->m_from))
			return 0;
	}
	if (item->m_until != (SceUInt64)-1) {
		if (!LiWriteTime(out, &g_liAttrUntil, &item->m_until))
			return 0;
	}
	*out += ">\n";
	if (item->m_ageLimit != 0) {
		XmlElement element;
		char value[64];

		element.name = g_liTagAgeLimit;
		sce_paf_snprintf(value, sizeof(value), "%lld", (SceInt64)item->m_ageLimit);
		element.text = paf::string(value);
		if (XmlWriteElement(out, &element, 1) != 0)
			return 0;
	}
	if (item->m_uageLimit != 255) {
		XmlElement element;
		char value[64];

		element.name = g_liTagUageLimit;
		sce_paf_snprintf(value, sizeof(value), "%lld", (SceInt64)item->m_uageLimit);
		element.text = paf::string(value);
		if (XmlWriteElement(out, &element, 1) != 0)
			return 0;
	}
	if (item->m_pokesute == LI_POKESUTE_ON || item->m_pokesute == LI_POKESUTE_OFF) {
		XmlElement element;

		element.name = g_liTagPokesute;
		if (item->m_pokesute == LI_POKESUTE_ON)
			element.text = "on";
		else
			element.text = "off";
		if (XmlWriteElement(out, &element, 1) != 0)
			return 0;
	}
	for (paf::vector<LiItemElement *>::iterator it = item->m_elements.begin();
	     it != item->m_elements.end(); ++it) {
		LiItemElement *itemElement = *it;

		switch (itemElement->m_type) {
		case LI_ITEM_TARGET: {
			LiTarget *target = (LiTarget *)itemElement;
			XmlElement element;

			element.name = g_liTagTarget;
			element.text = target->m_uri;
			if (target->m_reqAppVer.size() != 0)
				element.SetAttribute(g_liAttrReqAppVer.c_str(),
						     target->m_reqAppVer.c_str());
			if (XmlWriteElement(out, &element, 1) != 0)
				return 0;
			break;
		}
		case LI_ITEM_BACKGROUND: {
			LiItemBackground *background = (LiItemBackground *)itemElement;
			XmlElement element;

			if (!LiWritePlacement(&element, background))
				return 0;
			element.name = g_liTagItemBackground;
			element.text = background->m_image;
			if (XmlWriteElement(out, &element, 1) != 0)
				return 0;
			break;
		}
		case LI_ITEM_IMAGE: {
			LiImage *image = (LiImage *)itemElement;
			XmlElement element;

			if (!LiWritePlacement(&element, image))
				return 0;
			element.name = g_liTagImage;
			element.text = image->m_image;
			if (image->m_origin < LI_ITEM_KIND_COUNT)
				element.SetAttribute(g_liAttrImageOrigin.c_str(),
						     LiOriginName(image->m_origin)->c_str());
			if (XmlWriteElement(out, &element, 1) != 0)
				return 0;
			break;
		}
		case LI_ITEM_TEXT:
			if (!LiWriteText(out, (LiText *)itemElement))
				return 0;
			break;
		}
	}
	*out += "</";
	out->append(g_liTagLiveitem);
	*out += ">\n";
	return 1;
}

/*
 * A liveitem read from its XML, when its filter tags keep it for the age (below 0: any), the
 * model (any: anyModel) and the pokesute
 */
bool LiReadLiveitem(LiLiveItem *item, const char *xml, bool anyModel, int age,
		    int formatVersion, int pokesute)
{
	XmlElement root;

	XmlParseDocument(&root, xml);

	int limits[2] = { 0, 255 };
	int shown = 0;

	if (age >= 0) {
		if (!LiFilterLiveitem(NULL, &root.children, NULL, NULL, age, limits))
			return false;
		if (!anyModel && !LiCheckModel(NULL, &root.children))
			return false;
		if (!LiCheckPokesute(NULL, &root.children, formatVersion, pokesute, &shown))
			return false;
	}
	return LiParseLiveitemElement(item, &root, limits, shown, NULL);
}

extern "C" {

/*
 * The frame of a template's XML (size bytes of it), its liveitems kept for the language,
 * country and age (NULL, NULL and below 0: any); 1 with the frame in *frame, which the caller
 * then owns
 */
int SceLsdb_5BDC6351(LiFrame **frame, const char *xml, int size, const int *language,
		     const char *country, int age, bool system)
{
	LiParser parser(NULL, system, false);
	XmlElement root;

	{
		XmlTreeParser xmlParser(&root, true, true);

		if (xmlParser.Parse("<?xml version=\"1.0\" encoding=\"utf-8\"?>", 38, false) < 0)
			return 0;
		if (xmlParser.Parse(xml, size, true) < 0)
			return 0;
	}
	parser.SetFilter(language, country, age);
	if (!parser.ParseFrameOnly(&root))
		return 0;

	paf::list<LiPart *> *parts = &parser.m_template->m_parts;

	for (paf::list<LiPart *>::iterator part = parts->begin(); part != parts->end(); ++part) {
		if ((*part)->m_type == LI_PART_FRAME) {
			*frame = (LiFrame *)*part;
			parts->erase(part);
			return 1;
		}
	}
	return 0;
}

}

/*
 * The LiveArea daemon's requests. Most take the caller's object first, which the daemon does
 * not read.
 */
extern "C" {

int SceLsdb_B51740A6(void *client, paf::common::SharedPtr<LiTemplate> *tmpl,
		     const paf::string *titleId, bool keepDefault)
{
	return LiDaemon::GetInstance()->GetTemplate(tmpl, titleId, keepDefault);
}

int SceLsdb_DA5DC77C(void *client, paf::vector<paf::string> *excluded, paf::string *titleId)
{
	return LiDaemon::GetInstance()->GetUr0TitleToRemove(excluded, titleId);
}

int SceLsdb_BA1D0DB8(void *client, const paf::string *titleId, const paf::string *frameId,
		     paf::string *userdata, SceUID pid)
{
	return LiDaemon::GetInstance()->GetUserdata(titleId, frameId, userdata, pid);
}

void SceLsdb_29B275BA(const paf::string *titleId, const char *path)
{
	LiDaemon::GetInstance()->ResetContentsPath(titleId, path);
}

/* the title's LiveArea read again from its contents, in the system language */
int SceLsdb_EF880EFE(const paf::string *titleId, paf::string *path)
{
	return LiDaemon::GetInstance()->UpdateTitles(*titleId, -1, path);
}

int SceLsdb_6C74B301(void *client, const paf::string *titleId, const char *dir,
		     unsigned int *mask, const SceUInt64 *modifiedDate, paf::string *storedPath,
		     SceUID pid)
{
	return LiDaemon::GetInstance()->UpdateFromDir(titleId, dir, mask, modifiedDate,
						      storedPath, pid);
}

int SceLsdb_EFD528B1(void *client, const paf::string *titleId,
		     paf::common::SharedPtr<LiTemplate> *tmpl, const char *dir, bool keepStyle,
		     unsigned int *mask, paf::string *storedPath, SceUID pid)
{
	return LiDaemon::GetInstance()->UpdateTemplate(titleId, tmpl, dir, keepStyle, mask,
						       storedPath, pid);
}

int SceLsdb_95EF7C51(void *client, const paf::string *titleId, int func)
{
	return LiDaemon::GetInstance()->HasFunction(titleId, func);
}

int SceLsdb_4B64A790(void *client, const paf::string *titleId, LiPcc *pcc)
{
	return LiDaemon::GetInstance()->GetPcc(titleId, pcc);
}

int SceLsdb_79A6D877(void *client, const paf::string *titleId, bool function0,
		     const LiPcc *pcc, int pccFlags)
{
	return LiDaemon::GetInstance()->SetPcc(titleId, function0, pcc, pccFlags);
}

int SceLsdb_2EBB75D1(void *client, const paf::string *titleId, const LiFunction *function)
{
	return LiDaemon::GetInstance()->SetFunction(titleId, function);
}

int SceLsdb_ED2C11FC(void *client, const paf::string *titleId, int func)
{
	return LiDaemon::GetInstance()->RemoveFunction(titleId, func);
}

int SceLsdb_D22CFA8C(void *client, const paf::string *titleId, SceUInt64 *date)
{
	return LiDaemon::GetInstance()->GetModifiedDate(titleId, date);
}

int SceLsdb_105428BF(void *client, const paf::string *titleId, const SceUInt64 *date)
{
	return LiDaemon::GetInstance()->SetModifiedDate(titleId, date);
}

/* a title's LiveArea registered from dir, in the system language */
int SceLsdb_C875C57A(const paf::string *titleId, const char *dir)
{
	return LiDaemon::GetInstance()->Register(titleId, dir, -1);
}

int SceLsdb_C87397C0(const paf::string *titleId)
{
	return LiDaemon::GetInstance()->Delete(titleId);
}

/* a frame's revision and the path it was read from, for any process */
int SceLsdb_F9B38E6B(const paf::string *titleId, const char *frameId, SceInt64 *rev,
		     paf::string *orgPath)
{
	return LiDaemon::GetInstance()->GetRevision(titleId, frameId, rev, orgPath, 0);
}

}
