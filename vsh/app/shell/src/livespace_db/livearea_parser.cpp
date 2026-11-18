/*
 * template.xml read into a LiTemplate: the file read a part at a time into an XmlElement tree,
 * each of the livearea tag's parts parsed as its element is complete, and each frame's
 * liveitems kept or left out by their country, language, age, model, pokesute and time.
 */
#include <stdarg.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/algorithm>
#include <buffer/buffer.h>
#include <file/localfile.h>
#include <common/char_encoding.h>
#include <common/string_util.h>
#include <graphics/text/text_style.h>
#include <math/ngp/v4.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include "livearea.h"

/* the default template, shown when an application's cannot be read */
#define LI_DEFAULT_TEMPLATE \
	"vs0:data/internal/livearea/default/sce_sys/livearea/contents/template.xml"

/* a template.xml's largest size, and the part of it read at a time */
#define LI_TEMPLATE_SIZE_MAX	0x8000
#define LI_READ_SIZE		0x2000

/* the most characters of a text's strs, of a userdata and of a path's names */
#define LI_TEXT_LENGTH_MAX	0x200
#define LI_USERDATA_LENGTH_MAX	1023
#define LI_FILENAME_LENGTH_MAX	32
#define LI_DIRNAME_LENGTH_MAX	16

/* a frame's most liveitems */
#define LI_LIVEITEM_MAX		5

/* the liveitem elements in the order they are drawn */
struct LiItemElementLess {
	bool operator()(const LiItemElement *left, const LiItemElement *right) const
	{
		return left->m_type < right->m_type;
	}
};

LiParser::LiParser(const char *path, bool system, bool useDefault)
	: m_path(path), m_system(system), m_useDefault(useDefault), m_isDefault(false),
	  m_language(NULL), m_country(NULL), m_age(-1), m_checkTime(false), m_keepAll(false),
	  m_supportedFormatOnly(false), m_networkTime(false), m_pokesute(0)
{
}

LiParser::~LiParser()
{
}

void LiParser::Error(const char *format, ...)
{
#if !defined(FW_TYPE_CEX)	/* a retail system prints no errors */
	char message[2048];
	va_list args;

	va_start(args, format);
	sce_paf_vsnprintf(message, sizeof(message), format, args);
	if (m_system)
		sceClibPrintf("[livearea xml]error xml %s\n", message);
	else
		sceClibPrintf("[livearea xml]error xml:%s %s\n", m_path.c_str(), message);
	va_end(args);
#endif
}

/* "#RRGGBBAA" */
int LiParser::ParseTitle(XmlElement *element)
{
	if (!m_system)
		return 0;

	LiTemplate *tmpl = m_template.get();

	if (tmpl->HasPart(LI_PART_TITLE)) {
		Error("tag:%s not only one!\n", g_liTagTitle.c_str());
		return 0;
	}

	LiTitle *title = new LiTitle;

	if (title == NULL)
		return 0;

	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (g_liAttrTitleColor == it->first) {
			const char *value = it->second.c_str();

			if (value[0] == '#' && it->second.size() == 9) {
				const char *digit = value + 1;
				int i = 1;

				while (*digit >= '0' && *digit <= 'f') {
					i++;
					digit++;
					if (i >= 9) {
						char red[3] = { it->second.c_str()[1],
								it->second.c_str()[2], 0 };
						char green[3] = { it->second.c_str()[3],
								it->second.c_str()[4], 0 };
						char blue[3] = { it->second.c_str()[5],
								it->second.c_str()[6], 0 };
						char alpha[3] = { it->second.c_str()[7],
								it->second.c_str()[8], 0 };

						float r = (float)sce_paf_strtol(red, NULL, 16)
							/ 255.0f;
						float g = (float)sce_paf_strtol(green, NULL, 16)
							/ 255.0f;
						float b = (float)sce_paf_strtol(blue, NULL, 16)
							/ 255.0f;
						float a = (float)sce_paf_strtol(alpha, NULL, 16)
							/ 255.0f;

						title->m_color[0] = r;
						title->m_color[1] = g;
						title->m_color[2] = b;
						title->m_color[3] = a;
						goto next;
					}
				}
			}
			Error("tag[%s] attr[%s] invalid %s\n", g_liTagTitle.c_str(),
			      g_liAttrTitleColor.c_str(), it->second.c_str());
		}
next:
		;
	}
	tmpl->m_parts.push_back(title);
	return 1;
}

/* Error for a parser that may be NULL (the liveitems' elements) */
void LiParserError(LiParser *parser, const char *format, ...)
{
#if !defined(FW_TYPE_CEX)	/* a retail system prints no errors */
	char message[2048];
	va_list args;

	va_start(args, format);
	sce_paf_vsnprintf(message, sizeof(message), format, args);
	if (parser != NULL) {
		if (parser->m_system)
			sceClibPrintf("[livearea xml]error xml %s\n", message);
		else
			sceClibPrintf("[livearea xml]error xml:%s %s\n", parser->m_path.c_str(),
				      message);
	} else {
		sceClibPrintf("[livearea xml]error %s\n", message);
	}
	va_end(args);
#endif
}

/* a liveitem's tags that choose where it is shown */
bool LiIsFilterTag(const paf::string *name)
{
	return *name == g_liTagCntry || *name == g_liTagExcludeCntry || *name == g_liTagLang ||
	       *name == g_liTagExcludeLang || *name == g_liTagAgeLimit ||
	       *name == g_liTagUageLimit || *name == g_liTagModel || *name == g_liTagPokesute;
}

void LiRemoveNewlines(paf::string *str)
{
	paf::string::size_type pos;

	while ((pos = str->find(g_liCrLf)) != paf::string::npos)
		str->erase(pos, g_liCrLf.size());
	while ((pos = str->find(g_liLf)) != paf::string::npos)
		str->erase(pos, g_liLf.size());
}

/* an element's align, valign, width, height, x or y; 0 for another attribute */
int LiParsePlacement(const paf::string *tag, const XmlElement::AttrMap::iterator *attr,
		     LiPlacedElement *element, LiParser *parser, bool outside)
{
	XmlElement::AttrMap::iterator it = *attr;

	if (it->first == g_liAttrAlign) {
		int align = LiAlignFromName(&it->second, outside);

		if (align != LI_ALIGN_INVALID)
			element->m_align = align;
		else
			LiParserError(parser, "tag[%s] attr[%s] invalid string:%s\n", tag->c_str(),
				      it->first.c_str(), it->second.c_str());
	} else if (it->first == g_liAttrValign) {
		int valign = LiValignFromName(&it->second, outside);

		if (valign != LI_ALIGN_INVALID)
			element->m_valign = valign;
		else
			LiParserError(parser, "tag[%s] attr[%s] invalid string:%s\n", tag->c_str(),
				      it->first.c_str(), it->second.c_str());
	} else if (it->first == g_liAttrWidth) {
		char *end;
		int width = sce_paf_strtol(it->second.c_str(), &end, 10);

		if (*end != '\0')
			LiParserError(parser, "tag[%s] attr[%s] invalid string:%s\n", tag->c_str(),
				      it->first.c_str(), it->second.c_str());
		else
			element->m_width = width;
	} else if (it->first == g_liAttrHeight) {
		char *end;
		int height = sce_paf_strtol(it->second.c_str(), &end, 10);

		if (*end != '\0')
			LiParserError(parser, "tag[%s] attr[%s] invalid string:%s\n", tag->c_str(),
				      it->first.c_str(), it->second.c_str());
		else
			element->m_height = height;
	} else if (it->first == g_liAttrX) {
		char *end;
		int x = sce_paf_strtol(it->second.c_str(), &end, 10);

		if (*end != '\0') {
			LiParserError(parser, "tag[%s] attr[%s] invalid string:%s\n", tag->c_str(),
				      it->first.c_str(), it->second.c_str());
		} else {
			element->m_x = x;
			element->m_hasX = true;
		}
	} else if (it->first == g_liAttrY) {
		char *end;
		int y = sce_paf_strtol(it->second.c_str(), &end, 10);

		if (*end != '\0') {
			LiParserError(parser, "tag[%s] attr[%s] invalid string:%s\n", tag->c_str(),
				      it->first.c_str(), it->second.c_str());
		} else {
			element->m_y = y;
			element->m_hasY = true;
		}
	} else {
		return 0;
	}
	return 1;
}

/* an on or off attribute of a text; LI_ON_OFF_INVALID reported */
#define LI_TEXT_ON_OFF(attr, field)							\
	do {										\
		int on = LiOnOffFromName(&it->second);					\
											\
		if (on == LI_ON_OFF_INVALID)						\
			LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",	\
				      g_liTagText.c_str(), (attr).c_str(),		\
				      it->second.c_str());				\
		else									\
			text->field = on == 1;						\
	} while (0)

/* a number attribute of a text */
#define LI_TEXT_NUMBER(attr, field)							\
	do {										\
		char *end;								\
		int number = sce_paf_strtol(it->second.c_str(), &end, 10);		\
											\
		if (*end != '\0')							\
			LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",	\
				      g_liTagText.c_str(), (attr).c_str(),		\
				      it->second.c_str());				\
		else									\
			text->field = number;						\
	} while (0)

/* a str's on or off style */
#define LI_STR_ON_OFF(attr, style_attr)							\
	do {										\
		int on = LiOnOffFromName(&it->second);					\
											\
		if (on == LI_ON_OFF_INVALID)						\
			LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",	\
				      g_liTagStr.c_str(), (attr).c_str(),		\
				      it->second.c_str());				\
		else									\
			style.SetStyleAttribute(style_attr, on == 1);			\
	} while (0)

/* a liveitem's text: its layout, then its strs (or the title) in one string, each styled */
LiText *LiParseText(XmlElement *element, LiLiveItem *item, LiParser *parser)
{
	if (item->HasElement(LI_ITEM_TEXT)) {
		LiParserError(parser, "tag:%s not only one!\n", g_liTagText.c_str());
		return NULL;
	}

	LiText *text = new LiText;

	if (text == NULL)
		return NULL;

	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (LiParsePlacement(&g_liTagText, &it, text, parser, true))
			continue;

		if (it->first == g_liAttrTextOrigin) {
			int origin = LiOriginFromName(&it->second);

			if (origin != LI_ITEM_FRAME && origin != LI_ITEM_BACKGROUND &&
			    origin != LI_ITEM_IMAGE)
				LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",
					      g_liTagText.c_str(), g_liAttrTextOrigin.c_str(),
					      it->second.c_str());
			else
				text->m_origin = origin;
		} else if (it->first == g_liAttrTextAlign) {
			int align = LiAlignFromName(&it->second, false);

			if (align == LI_ALIGN_INVALID)
				LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",
					      g_liTagText.c_str(), g_liAttrTextOrigin.c_str(),
					      it->second.c_str());
			else
				text->m_textAlign = align;
		} else if (it->first == g_liAttrTextValign) {
			int valign = LiValignFromName(&it->second, false);

			if (valign == LI_ALIGN_INVALID)
				LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",
					      g_liTagText.c_str(), g_liAttrTextOrigin.c_str(),
					      it->second.c_str());
			else
				text->m_textValign = valign;
		} else if (it->first == g_liAttrLineAlign) {
			int align = LiAlignFromName(&it->second, false);

			if (align == LI_ALIGN_INVALID)
				LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",
					      g_liTagText.c_str(), g_liAttrTextOrigin.c_str(),
					      it->second.c_str());
			else
				text->m_lineAlign = align;
		} else if (it->first == g_liAttrLineSpace) {
			LI_TEXT_NUMBER(g_liAttrLineSpace, m_lineSpace);
		} else if (it->first == g_liAttrLineBreak) {
			LI_TEXT_ON_OFF(g_liAttrLineBreak, m_lineBreak);
		} else if (it->first == g_liAttrWordWrap) {
			LI_TEXT_ON_OFF(g_liAttrWordWrap, m_wordWrap);
		} else if (it->first == g_liAttrEllipsis) {
			LI_TEXT_ON_OFF(g_liAttrEllipsis, m_ellipsis);
		} else if (it->first == g_liAttrWordScroll) {
			LI_TEXT_ON_OFF(g_liAttrWordScroll, m_wordScroll);
		} else if (it->first == g_liAttrPreBr) {
			LI_TEXT_ON_OFF(g_liAttrPreBr, m_preBr);
		} else if (it->first == g_liAttrMarginTop) {
			LI_TEXT_NUMBER(g_liAttrMarginTop, m_marginTop);
		} else if (it->first == g_liAttrMarginBottom) {
			LI_TEXT_NUMBER(g_liAttrMarginBottom, m_marginBottom);
		} else if (it->first == g_liAttrMarginLeft) {
			LI_TEXT_NUMBER(g_liAttrMarginLeft, m_marginLeft);
		} else if (it->first == g_liAttrMarginRight) {
			LI_TEXT_NUMBER(g_liAttrMarginRight, m_marginRight);
		} else {
			LiParserError(parser, "tag[%s] invalid attr:%s\n", g_liTagText.c_str(),
				      it->first.c_str());
		}
	}

	paf::vector<int> lengths;

	{
		paf::wstring string;
		int index = 0;

		for (paf::list<XmlElement>::iterator child = element->children.begin();
		     child != element->children.end(); ++child) {
			if (child->name == g_liTagTextTitle) {
				paf::wstring title(L"title");

				string.append(title.c_str(), title.size());
				lengths.push_back(title.size());
				text->m_titleIndex = index++;
			} else if (child->text.size() != 0 && child->name == g_liTagStr) {
				paf::wstring str;

				if (text->m_preBr) {
					paf::wstring utf16;

					paf::common::Utf8ToUtf16(child->text, &utf16, 0);
					str = utf16;
				} else {
					paf::string utf8(child->text);

					LiRemoveNewlines(&utf8);

					paf::wstring utf16;

					paf::common::Utf8ToUtf16(utf8, &utf16, 0);
					str = utf16;
				}
				string.append(str.c_str(), str.size());
				lengths.push_back(str.size());
				index++;
			}
		}
		if (string.size() > LI_TEXT_LENGTH_MAX) {
			LiParserError(parser, "tag[%s] Too Large:%d[byte]\n", g_liTagStr.c_str(),
				      string.size());
			delete text;
			return NULL;
		}
		text->m_string.SetString(string);
	}

	unsigned int pos = 0;
	int index = 0;

	for (paf::list<XmlElement>::iterator child = element->children.begin();
	     child != element->children.end(); ++child) {
		if (child->text.size() == 0 && !(child->name == g_liTagTextTitle))
			continue;
		if (!(child->name == g_liTagStr || child->name == g_liTagTextTitle))
			continue;

		paf::graph::TextStyle style;

		style.SetStyleAttribute(paf::graph::TextStyleAttribute_Point, 18.0f);
		style.SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
					paf::math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		style.SetStyleAttribute(paf::graph::TextStyleAttribute_EnlargeFontSize, false);
		style.SetStyleAttribute(paf::graph::TextStyleAttribute_ForceBold, false);
		for (XmlElement::AttrMap::iterator it = child->attrs.begin();
		     it != child->attrs.end(); ++it) {
			if (g_liAttrStrColor == it->first) {
				const char *value = it->second.c_str();

				if (value[0] == '#' && it->second.size() == 7) {
					const char *digit = value + 1;
					int i = 1;

					while (*digit >= '0' && *digit <= 'f') {
						i++;
						digit++;
						if (i < 7)
							continue;

						char red[3] = { it->second.c_str()[1],
								it->second.c_str()[2], 0 };
						char green[3] = { it->second.c_str()[3],
								  it->second.c_str()[4], 0 };
						char blue[3] = { it->second.c_str()[5],
								 it->second.c_str()[6], 0 };
						float r = (float)sce_paf_strtol(red, NULL, 16)
							/ 255.0f;
						float g = (float)sce_paf_strtol(green, NULL, 16)
							/ 255.0f;
						float b = (float)sce_paf_strtol(blue, NULL, 16)
							/ 255.0f;

						style.SetStyleAttribute(
							paf::graph::TextStyleAttribute_Color,
							paf::math::v4(r, g, b, 1.0f));
						goto next;
					}
				}
				LiParserError(parser, "tag[%s] attr[%s] invalid %s\n",
					      g_liTagStr.c_str(), g_liAttrStrColor.c_str(),
					      it->second.c_str());
			} else if (g_liAttrStrSize == it->first) {
				float size = sce_paf_atof(it->second.c_str());

				if (size == 0.0f)
					LiParserError(parser, "tag[%s] attr[%s] invalid %s\n",
						      g_liTagStr.c_str(), g_liAttrStrSize.c_str(),
						      it->second.c_str());
				else
					style.SetStyleAttribute(
						paf::graph::TextStyleAttribute_Point, size);
			} else if (g_liAttrStrBold == it->first) {
				LI_STR_ON_OFF(g_liAttrStrBold, paf::graph::TextStyleAttribute_Bold);
			} else if (g_liAttrStrOblique == it->first) {
				LI_STR_ON_OFF(g_liAttrStrOblique,
					      paf::graph::TextStyleAttribute_Oblique);
			} else if (g_liAttrStrUnderline == it->first) {
				LI_STR_ON_OFF(g_liAttrStrUnderline,
					      paf::graph::TextStyleAttribute_Underline);
			} else if (g_liAttrStrShadow == it->first) {
				LI_STR_ON_OFF(g_liAttrStrShadow,
					      paf::graph::TextStyleAttribute_Shadow);
			} else if (g_liAttrStrEmboss == it->first) {
				LI_STR_ON_OFF(g_liAttrStrEmboss,
					      paf::graph::TextStyleAttribute_Emboss);
			} else {
				LiParserError(parser, "tag[%s] invalid attr:%s\n",
					      g_liTagStr.c_str(), it->first.c_str());
			}
next:
			;
		}
		text->m_string.SetStyle(pos, lengths[index], style);
		pos += lengths[index];
		index++;
	}
	item->m_elements.push_back(text);
	return text;
}

void LiRemoveWhitespace(paf::string *str)
{
	paf::string::size_type pos;

	while ((pos = str->find(g_liCrLf)) != paf::string::npos)
		str->erase(pos, g_liCrLf.size());
	while ((pos = str->find(g_liLf)) != paf::string::npos)
		str->erase(pos, g_liLf.size());
	while ((pos = str->find(g_liTab)) != paf::string::npos)
		str->erase(pos, g_liTab.size());
	while ((pos = str->find(g_liSpace)) != paf::string::npos)
		str->erase(pos, g_liSpace.size());
}

/* an image's path in the livearea folder: a file name, or a folder and a file name */
int LiCheckPath(LiParser *parser, paf::string *path)
{
	if (path->find(g_liDotDotSlash) != paf::string::npos) {
		LiParserError(parser, "filepath invalid string:%s\n", path->c_str());
		return 0;
	}
	LiRemoveWhitespace(path);

	paf::list<paf::string> names;

	paf::common::string_util::tokenize(names, path->c_str(), 0, '/');
	switch (names.size()) {
	case 0:
		LiParserError(parser, "filepath none\n");
		return 0;
	case 1:
		if (names.front().size() > LI_FILENAME_LENGTH_MAX) {
			LiParserError(parser, "filename too long:%s\n", names.front().c_str());
			return 0;
		}
		break;
	case 2: {
		paf::list<paf::string>::iterator name = names.begin();

		if (name->size() > LI_DIRNAME_LENGTH_MAX) {
			LiParserError(parser, "directory too long:%s\n", name->c_str());
			return 0;
		}
		++name;
		if (name->size() > LI_FILENAME_LENGTH_MAX) {
			LiParserError(parser, "filename too long:%s\n", name->c_str());
			return 0;
		}
		break;
	}
	default:
		LiParserError(parser, "filepath subdirectory too deep:%s\n", path->c_str());
		return 0;
	}
	return 1;
}

LiImage *LiParseImage(XmlElement *element, LiLiveItem *item, LiParser *parser)
{
	LiImage *result = NULL;

	if (item->HasElement(LI_ITEM_IMAGE)) {
		LiParserError(parser, "tag:%s not only one!\n", g_liTagImage.c_str());
		return result;
	}

	LiImage *image = new LiImage;

	if (image == NULL)
		return result;

	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (LiParsePlacement(&g_liTagImage, &it, image, parser, true))
			continue;

		if (it->first == g_liAttrImageOrigin) {
			int origin = LiOriginFromName(&it->second);

			if (origin == LI_ITEM_FRAME || origin == LI_ITEM_BACKGROUND)
				image->m_origin = origin;
			else
				LiParserError(parser, "tag[%s] attr[%s] invalid value:%s\n",
					      g_liTagImage.c_str(), g_liAttrImageOrigin.c_str(),
					      it->second.c_str());
		} else {
			LiParserError(parser, "tag[%s] invalid attr:%s\n", g_liTagImage.c_str(),
				      it->first.c_str());
		}
	}
	if (element->text.size() == 0) {
		LiParserError(parser, "tag[%s] not filepath!\n", g_liTagImage.c_str());
		delete image;
		return result;
	}
	image->m_image = element->text;
	if (!LiCheckPath(parser, &image->m_image)) {
		LiParserError(parser, "tag:%s invalid filepath:%s\n", g_liTagImage.c_str(),
			      image->m_image.c_str());
		delete image;
		return result;
	}
	item->m_elements.push_back(image);
	return image;
}

LiItemBackground *LiParseItemBackground(XmlElement *element, LiLiveItem *item, LiParser *parser)
{
	if (item->HasElement(LI_ITEM_BACKGROUND)) {
		LiParserError(parser, "tag:%s not only one!\n", g_liTagItemBackground.c_str());
		return NULL;
	}

	LiItemBackground *background = new LiItemBackground;

	if (background == NULL)
		return NULL;

	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (!LiParsePlacement(&g_liTagItemBackground, &it, background, parser, false))
			LiParserError(parser, "tag[%s] invalid attr:%s\n",
				      g_liTagItemBackground.c_str(), it->first.c_str());
	}
	if (element->text.size() == 0) {
		LiParserError(parser, "tag[%s] not filepath!\n", g_liTagItemBackground.c_str());
		delete background;
		return NULL;
	}
	background->m_image = element->text;
	if (!LiCheckPath(parser, &background->m_image)) {
		LiParserError(parser, "tag:%s invalid filepath:%s\n", g_liTagItemBackground.c_str(),
			      background->m_image.c_str());
		delete background;
		return NULL;
	}
	item->m_elements.push_back(background);
	return background;
}

LiTarget *LiParseTarget(XmlElement *element, LiLiveItem *item, LiParser *parser)
{
	if (item->HasElement(LI_ITEM_TARGET)) {
		LiParserError(parser, "tag:%s not only one!\n", g_liTagTarget.c_str());
		return NULL;
	}

	LiTarget *target = new LiTarget;

	if (target == NULL)
		return NULL;

	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (it->first == g_liAttrReqAppVer) {
			if (SceLsdb_87172F61(&it->second) <= 100)
				LiParserError(parser, "tag[%s] invalid value attr:%s value:%s\n",
					      g_liTagTarget.c_str(), it->first.c_str(),
					      it->second.c_str());
			else
				target->m_reqAppVer = it->second;
		} else {
			LiParserError(parser, "tag[%s] invalid attr:%s\n", g_liTagTarget.c_str(),
				      it->first.c_str());
		}
	}
	target->m_uri = element->text;
	LiRemoveWhitespace(&target->m_uri);
	item->m_target = target;
	item->m_elements.push_back(target);
	return target;
}

/* a liveitem's limits, pokesute and elements, sorted in the order they are drawn */
int LiParseItemElements(LiLiveItem *item, XmlElement *element, const int *limits, int pokesute,
			LiParser *parser)
{
	item->m_ageLimit = limits[0];
	item->m_uageLimit = limits[1];
	item->m_pokesute = pokesute;
	for (paf::list<XmlElement>::iterator child = element->children.begin();
	     child != element->children.end(); ++child) {
		if (child->name == g_liTagTarget)
			LiParseTarget(&*child, item, NULL);
		else if (child->name == g_liTagItemBackground)
			LiParseItemBackground(&*child, item, NULL);
		else if (child->name == g_liTagImage)
			LiParseImage(&*child, item, NULL);
		else if (child->name == g_liTagText)
			LiParseText(&*child, item, NULL);
		else if (!LiIsFilterTag(&child->name))
			LiParserError(parser, "invalid tag:%s\n", child->name.c_str());
	}
	paf::sort(item->m_elements.begin(), item->m_elements.end(), LiItemElementLess());
	return 1;
}

/* a frame's liveitem, kept when it is (or is not) the default and is shown now */
int LiParser::ParseLiveitem(XmlElement *element, LiFrame *frame, bool isDefault, int *limits,
			    int pokesute)
{
	LiLiveItem *item = new LiLiveItem;

	if (item == NULL)
		return 0;

	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (it->first == g_liAttrFrom) {
			paf::rtc::Tick tick;

			if (it->second.size() != 0 &&
			    paf::rtc::ParseRFC3339(&tick, it->second.c_str()) == 0)
				item->m_from = tick;
			else
				Error("tag[%s] attr[%s] invalid string:%s\n",
				      g_liTagLiveitem.c_str(),
				      it->first.c_str(), it->second.c_str());
		} else if (it->first == g_liAttrUntil) {
			paf::rtc::Tick tick;

			if (it->second.size() != 0 &&
			    paf::rtc::ParseRFC3339(&tick, it->second.c_str()) == 0)
				item->m_until = tick;
			else
				Error("tag[%s] attr[%s] invalid string:%s\n",
				      g_liTagLiveitem.c_str(),
				      it->first.c_str(), it->second.c_str());
		} else if (it->first == g_liAttrItemDefault) {
			int on = LiOnOffFromName(&it->second);

			if (on == LI_ON_OFF_INVALID)
				Error("tag[%s] attr[%s] invalid value:%s\n",
				      g_liTagLiveitem.c_str(),
				      g_liAttrItemDefault.c_str(), it->second.c_str());
			else
				item->m_default = on == 1;
		} else {
			Error("tag[%s] invalid attr:%s\n", g_liTagLiveitem.c_str(),
			      it->first.c_str());
		}
	}
	if (isDefault && !item->m_default) {
		delete item;
		return 0;
	}
	if (!isDefault && item->m_default) {
		delete item;
		return 0;
	}
	if (!m_keepAll && m_checkTime && !SceLsdb_628CFBE1(item, true, m_networkTime)) {
		delete item;
		return 0;
	}
	if (!LiParseItemElements(item, element, limits, pokesute, this)) {
		delete item;
		return 0;
	}
	frame->m_items.push_back(item);
	return 1;
}

/* a frame of a style "aN": frame1 to the style's last */
int LiIsFrameAllowed(const paf::string *style, const paf::string *id)
{
	int lastFrame[5] = { 3, 3, 5, 5, 7 };

	if (style->size() != 2 || style->c_str()[0] != 'a')
		return 1;

	int number = style->c_str()[1] - '0';

	if (number <= 0 || number > 5)
		return 0;
	if (id->size() != 6 || sce_paf_strncmp(id->c_str(), "frame", 5) != 0)
		return 0;

	int frame = id->at(5) - '0';

	if (frame <= 0 || frame > lastFrame[number - 1])
		return 0;
	return 1;
}

int LiParser::ParseFrame(XmlElement *element)
{
	LiTemplate *tmpl = m_template.get();
	LiFrame *frame = new LiFrame;

	if (frame == NULL)
		return 0;

	if (tmpl->HasFrame(&frame->m_id)) {
		Error("tag[%s] attr[%s] %s not unique\n", g_liTagFrame.c_str(),
		      g_liAttrFrameId.c_str(), frame->m_id.c_str());
		delete frame;
		return 0;
	}

	bool hasId = false;

	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (it->first == g_liAttrFrameId) {
			frame->m_id = it->second;
			hasId = true;
		} else if (it->first == g_liAttrFrameMulti) {
			for (int multi = 0; multi < 2; multi++) {
				if (it->second == g_liMultiNames[multi]) {
					frame->m_multi = multi;
					break;
				}
			}
		} else if (it->first == g_liAttrFrameAutoflip) {
			frame->m_autoflip = sce_paf_strtoul(it->second.c_str(), NULL, 10);
		} else if (it->first == g_liAttrFrameRev) {
			char *end;
			SceInt64 rev = sce_paf_strtoll(it->second.c_str(), &end, 10);

			if (*end != '\0') {
				Error("attr[%s] invalid data\n", g_liAttrFrameRev.c_str());
				delete frame;
				return 0;
			}
			frame->m_rev = rev;
		} else if (it->first == g_liAttrFrameUserdata) {
			frame->m_userdata = it->second;
			if (it->second.size() > LI_USERDATA_LENGTH_MAX) {
				Error("tag[%s] attr[%s] string length over[%d] \n",
				      g_liTagFrame.c_str(), g_liAttrFrameUserdata.c_str(),
				      it->second.size());
				frame->m_userdata.resize(LI_USERDATA_LENGTH_MAX);
			}
		} else {
			Error("tag[%s] invalid attr:%s\n", g_liTagFrame.c_str(), it->first.c_str());
		}
	}
	if (!hasId) {
		Error("tag[%s] attr[%s] none\n", g_liTagFrame.c_str(), g_liAttrFrameId.c_str());
		delete frame;
		return 0;
	}
	if (frame->m_id.size() == 0) {
		Error("tag[%s] attr[%s] empty\n", g_liTagFrame.c_str(), g_liAttrFrameId.c_str());
		delete frame;
		return 0;
	}
	if (tmpl->HasFrame(&frame->m_id)) {
		Error("tag[%s] attr[%s] %s not unique\n", g_liTagFrame.c_str(),
		      g_liAttrFrameId.c_str(), frame->m_id.c_str());
		delete frame;
		return 0;
	}
	if (!LiIsFrameAllowed(&tmpl->m_style, &frame->m_id)) {
		Error("tag[%s] attr[%s] %s not allowed in style %s\n", g_liTagFrame.c_str(),
		      g_liAttrFrameId.c_str(), frame->m_id.c_str(), tmpl->m_style.c_str());
		delete frame;
		return 0;
	}

	int count = 0;

	for (paf::list<XmlElement>::iterator child = element->children.begin();
	     child != element->children.end(); ++child) {
		if (child->name == g_liTagLiveitem) {
			int limits[2] = { 0, 255 };
			int pokesute = LI_POKESUTE_NONE;

			if (m_keepAll ||
			    (LiFilterLiveitem(this, &child->children, m_language, m_country, m_age,
					      limits) &&
			     (!m_system || LiCheckModel(this, &child->children)) &&
			     LiCheckPokesute(this, &child->children, tmpl->m_formatVersion,
					     m_pokesute, &pokesute))) {
				if (ParseLiveitem(&*child, frame, false, limits, pokesute))
					count++;
			}
		} else {
			Error("invalid tag:%s\n", child->name.c_str());
		}
	}
	if (count == 0) {
		for (paf::list<XmlElement>::iterator child = element->children.begin();
		     child != element->children.end(); ++child) {
			if (child->name == g_liTagLiveitem) {
				int limits[2] = { 0, 255 };

				ParseLiveitem(&*child, frame, true, limits, LI_POKESUTE_NONE);
			} else {
				Error("invalid tag:%s\n", child->name.c_str());
			}
		}
	}
	if (!m_keepAll && count > LI_LIVEITEM_MAX) {
		Error("frame[%s] liveitem over:%d\n", frame->m_id.c_str(), count);
		delete frame;
		return 0;
	}
	tmpl->m_parts.push_back(frame);
	return 1;
}

/* the first startup-image kept (the default one when isDefault), from its text or its path */
int LiParser::ParseGate(XmlElement *element, bool isDefault)
{
	LiTemplate *tmpl = m_template.get();

	if (tmpl->HasPart(LI_PART_GATE)) {
		Error("tag:%s not only one!\n", g_liTagGate.c_str());
		return 0;
	}

	LiGate *gate = new LiGate;

	if (gate == NULL)
		return 0;

	paf::list<XmlElement>::iterator child;

	for (child = element->children.begin(); child != element->children.end(); ++child) {
		if (!(child->name == g_liTagStartupImage)) {
			Error("invalid tag:%s\n", child->name.c_str());
			continue;
		}

		bool def = false;

		for (XmlElement::AttrMap::iterator it = child->attrs.begin();
		     it != child->attrs.end(); ++it) {
			if (it->first == g_liAttrBackgroundDefault) {
				int on = LiOnOffFromName(&it->second);

				if (on == LI_ON_OFF_INVALID)
					Error("tag[%s] attr[%s] invalid value:%s\n",
					      g_liTagLiveitem.c_str(), g_liAttrItemDefault.c_str(),
					      it->second.c_str());
				else
					def = on == 1;
				break;
			}
			Error("tag[%s] invalid attr:%s\n", g_liTagLiveitem.c_str(),
			      it->first.c_str());
		}
		if (m_keepAll)
			goto found;
		if (isDefault) {
			if (def)
				goto found;
		} else if (!def && LiFilterLiveitem(this, &child->children, m_language, m_country,
						    m_age, NULL)) {
			goto found;
		}
	}
	delete gate;
	return 0;

found:

	paf::string *image = &gate->m_startupImage;

	if (child->text.size() != 0) {
		*image = child->text;
	} else {
		for (paf::list<XmlElement>::iterator path = child->children.begin();
		     path != child->children.end(); ++path) {
			if (path->name == g_liAttrGatePath) {
				*image = path->text;
				break;
			}
		}
	}
	if (!LiCheckPath(this, image)) {
		Error("tag:%s invalid filepath:%s\n", g_liTagGate.c_str(),
		      gate->m_startupImage.c_str());
		*image = "";
	}
	tmpl->m_parts.push_back(gate);
	return 1;
}

int LiParser::ParseFunctionZone(XmlElement *element)
{
	if (!m_system)
		return 0;

	LiTemplate *tmpl = m_template.get();

	if (tmpl->HasPart(LI_PART_FUNCTION_ZONE)) {
		Error("tag:%s not only one!\n", g_liTagFunctionZone.c_str());
		return 0;
	}

	LiFunctionZone *zone = new LiFunctionZone;

	if (zone == NULL)
		return 0;

	for (paf::list<XmlElement>::iterator child = element->children.begin();
	     child != element->children.end(); ++child) {
		if (!(child->name == g_liTagFunc)) {
			Error("invalid tag:%s\n", child->name.c_str());
			continue;
		}

		paf::string name(child->text);

		LiRemoveWhitespace(&name);

		for (int func = 0; func < 9; func++) {
			if (!(name == g_liFuncNames[func]))
				continue;

			zone->m_functions.resize(zone->m_functions.size() + 1, LiFunction());

			LiFunction *function = &zone->m_functions[zone->m_functions.size() - 1];

			function->func = func;
			for (paf::list<XmlElement>::iterator target = child->children.begin();
			     target != child->children.end(); ++target) {
				if (target->name == g_liAttrFuncTarget) {
					if (target->text.size() == 0)
						Error("tag[%s] string empty:%s\n",
						      target->name.c_str());
					function->target = target->text;
					break;
				}
				Error("invalid tag:%s\n", child->name.c_str());
			}
			break;
		}
	}
	tmpl->m_parts.push_back(zone);
	return 1;
}

/* the first image kept (the default one when isDefault), from its text or its path */
int LiParser::ParseBackground(XmlElement *element, bool isDefault)
{
	LiTemplate *tmpl = m_template.get();

	if (tmpl->HasPart(LI_PART_BACKGROUND)) {
		Error("tag:%s not only one!\n", g_liTagBackground.c_str());
		return 0;
	}

	LiBackground *background = new LiBackground;

	if (background == NULL)
		return 0;

	paf::list<XmlElement>::iterator child;

	for (child = element->children.begin(); child != element->children.end(); ++child) {
		if (!(child->name == g_liTagBackgroundImage)) {
			Error("invalid tag:%s\n", child->name.c_str());
			continue;
		}

		bool def = false;

		for (XmlElement::AttrMap::iterator it = child->attrs.begin();
		     it != child->attrs.end(); ++it) {
			if (it->first == g_liAttrBackgroundDefault) {
				int on = LiOnOffFromName(&it->second);

				if (on == LI_ON_OFF_INVALID)
					Error("tag[%s] attr[%s] invalid value:%s\n",
					      g_liTagLiveitem.c_str(), g_liAttrItemDefault.c_str(),
					      it->second.c_str());
				else
					def = on == 1;
				break;
			}
			Error("tag[%s] invalid attr:%s\n", g_liTagLiveitem.c_str(),
			      it->first.c_str());
		}
		if (m_keepAll)
			goto found;
		if (isDefault) {
			if (def)
				goto found;
		} else if (!def && LiFilterLiveitem(this, &child->children, m_language, m_country,
						    m_age, NULL)) {
			goto found;
		}
	}
	delete background;
	return 0;

found:
	if (child->text.size() != 0) {
		background->m_image = child->text;
	} else {
		for (paf::list<XmlElement>::iterator path = child->children.begin();
		     path != child->children.end(); ++path) {
			if (path->name == g_liAttrBackgroundPath) {
				if (path->text.size() == 0) {
					Error("tag[%s] string empty:%s\n", path->name.c_str());
					delete background;
					return 0;
				}
				background->m_image = path->text;
				break;
			}
		}
		if (background->m_image.size() == 0) {
			Error("tag[%s] string empty:%s\n", child->name.c_str());
			delete background;
			return 0;
		}
	}
	if (!LiCheckPath(this, &background->m_image)) {
		Error("tag:%s invalid filepath:%s\n", g_liTagBackground.c_str(),
		      background->m_image.c_str());
		delete background;
		return 0;
	}
	tmpl->m_parts.push_back(background);
	return 1;
}

/* the livearea tag's children read so far, but the last one until the document's end */
int LiParser::ParseParts(XmlElement *root, bool last)
{
	paf::list<XmlElement>::iterator child = root->children.begin();

	while (child != root->children.end()) {
		if (!last && root->children.size() == 1)
			break;

		XmlElement *element = &*child;

		if (element->name == g_liTagBackground) {
			if (!ParseBackground(element, false))
				ParseBackground(element, true);
		} else if (element->name == g_liTagFunctionZone) {
			ParseFunctionZone(element);
		} else if (element->name == g_liTagGate) {
			if (!ParseGate(element, false))
				ParseGate(element, true);
		} else if (element->name == g_liTagFrame) {
			ParseFrame(element);
		} else if (element->name == g_liTagTitle) {
			ParseTitle(element);
		} else {
			Error("invalid tag:%s\n", element->name.c_str());
		}
		child = root->children.erase(child);
	}
	return 1;
}

/* the livearea tag: its style (whose rco must exist), format-ver and content-rev */
int LiParser::ParseLivearea(XmlElement *root)
{
	if (!(root->name == g_liTagLivearea)) {
		Error("tag[%s] not found\n", g_liTagLivearea.c_str());
		return 0;
	}

	LiTemplate *tmpl = m_template.get();
	bool hasStyle = false;
	bool hasFormatVer = false;

	for (XmlElement::AttrMap::iterator it = root->attrs.begin(); it != root->attrs.end();
	     ++it) {
		if (it->first == g_liAttrStyle) {
			char path[1024];

			tmpl->m_style = it->second;
			sce_paf_snprintf(path, sizeof(path), "%s%s.rco", "vs0:vsh/shell/livearea/",
					 it->second.c_str());

			bool exists = paf::LocalFile::Exists(path);

			hasStyle = true;
			if (!exists)
				return 0;
		} else if (it->first == g_liAttrFormatVer) {
			if (m_supportedFormatOnly) {
				if (!LiIsSupportedFormatVersion(&it->second)) {
					Error("attr[%s] invalid data\n", g_liAttrFormatVer.c_str());
					return 0;
				}
			} else if (!LiIsFormatVersion(&it->second)) {
				Error("attr[%s] invalid data\n", g_liAttrFormatVer.c_str());
				return 0;
			}
			tmpl->m_formatVer = it->second;
			tmpl->m_formatVersion = SceLsdb_19BA5A0A(&it->second);
			hasFormatVer = true;
		} else if (it->first == g_liAttrContentRev) {
			char *end;
			SceInt64 rev = sce_paf_strtoll(it->second.c_str(), &end, 10);

			if (*end != '\0') {
				Error("attr[%s] invalid data\n", g_liAttrContentRev.c_str());
				return 0;
			}
			tmpl->m_contentRev = rev;
		}
	}
	if (!hasFormatVer) {
		Error("attr[%s] not found\n", g_liAttrFormatVer.c_str());
		return 0;
	}
	if (!hasStyle) {
		Error("attr[%s] not found\n", g_liAttrStyle.c_str());
		return 0;
	}
	return 1;
}

/*
 * The template: 0, -1 without the file (or the default one), -2 without memory, -3 for a file
 * that cannot be read or is not a template, -4 without the file, -6 for one too large.
 */
int LiParser::Parse()
{
	paf::common::SharedPtr<paf::MallocBuffer> buffer =
		paf::MallocBuffer::Allocate(LI_READ_SIZE);

	if (buffer.get() == NULL)
		return -2;

	int32_t error;
	paf::common::SharedPtr<paf::File> file =
		paf::LocalFile::Open(m_path.c_str(), SCE_O_RDONLY, 0, &error);

	if (m_useDefault) {
		if (file.get() == NULL)
			Error("file not found\n");
		if (file.get() != NULL && file->GetFileSize() > LI_TEMPLATE_SIZE_MAX) {
			Error("too large file\n", LI_TEMPLATE_SIZE_MAX);
			file->Close();
		}
		if (file.get() == NULL) {
			file = paf::LocalFile::Open(LI_DEFAULT_TEMPLATE, SCE_O_RDONLY, 0, &error);
			m_isDefault = true;
		}
		if (file.get() == NULL)
			return -1;
	} else {
		if (file.get() == NULL) {
#if !defined(FW_TYPE_CEX)	/* a retail system prints no errors */
			sceClibPrintf("[livearea xml]xmlfile is not found. "
				      "The defaul livearea is shown.\n");
#endif
			return -4;
		}
		if (file->GetFileSize() > LI_TEMPLATE_SIZE_MAX) {
			Error("too large file\n", LI_TEMPLATE_SIZE_MAX);
			return -6;
		}
	}

	m_template = paf::common::SharedPtr<LiTemplate>(new LiTemplate);
	if (m_template.get() == NULL)
		return -2;

	XmlElement root;
	XmlTreeParser parser(&root, true, true);
	bool first = true;
	bool last;

	do {
		int size = file->Read(buffer->GetAddr(), buffer->GetSize());

		if (size < 0) {
			m_template.reset();
			return -3;
		}
		last = size < (int)buffer->GetSize();
		if (parser.Parse((const char *)buffer->GetAddr(), size, last) != 0) {
			Error("fatal invalid xml\n");
			m_template.reset();
			return -3;
		}
		if (first) {
			first = false;
			if (!ParseLivearea(&root)) {
				m_template.reset();
				Error("fatal invalid xml\n");
				return -3;
			}
		}
		if (!ParseParts(&root, last)) {
			Error("fatal invalid xml\n");
			m_template.reset();
			return -3;
		}
	} while (!last);

	LiTemplate *tmpl = m_template.get();

	if (!tmpl->HasPart(LI_PART_GATE)) {
		LiGate *gate = new LiGate;

		if (gate != NULL)
			tmpl->m_parts.push_back(gate);
	}
	return 0;
}

int LiParser::SetFilter(const int *language, const char *country, int age)
{
	if (language != NULL) {
		m_languageValue = *language;
		m_language = &m_languageValue;
	}
	if (country != NULL && *country != '\0') {
		m_countryValue = paf::string(country);
		m_country = m_countryValue.c_str();
	}
	m_age = age;
	return 1;
}

/* a frame element alone, into a template of its own */
int LiParser::ParseFrameOnly(XmlElement *element)
{
	m_template = paf::common::SharedPtr<LiTemplate>(new LiTemplate);
	if (m_template.get() == NULL)
		return 0;
	return ParseFrame(element);
}

int LiParser::SetDefaultPath()
{
	m_path = LI_DEFAULT_TEMPLATE;
	m_isDefault = true;
	return 1;
}

/* a liveitem from its element, without its default and without filtering it */
int LiParseLiveitemElement(LiLiveItem *item, XmlElement *element, const int *limits,
			   int pokesute, LiParser *parser)
{
	for (XmlElement::AttrMap::iterator it = element->attrs.begin(); it != element->attrs.end();
	     ++it) {
		if (it->first == g_liAttrFrom) {
			paf::rtc::Tick tick;

			if (it->second.size() != 0 &&
			    paf::rtc::ParseRFC3339(&tick, it->second.c_str()) == 0)
				item->m_from = tick;
		} else if (it->first == g_liAttrUntil) {
			paf::rtc::Tick tick;

			if (it->second.size() != 0 &&
			    paf::rtc::ParseRFC3339(&tick, it->second.c_str()) == 0)
				item->m_until = tick;
		}
	}
	return LiParseItemElements(item, element, limits, pokesute, parser);
}

paf::string g_liDotDotSlash("../");
paf::string g_liSlash("/");
paf::string g_liCrLf("\r\n");
paf::string g_liLf("\n");
paf::string g_liTab("\t");
paf::string g_liSpace(" ");
