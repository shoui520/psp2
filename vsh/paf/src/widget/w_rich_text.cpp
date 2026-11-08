/*
 * paf::ui::RichText (TOOL 0x8110cd0c..0x81112f78): the scroll view of a marked-up text, its
 * parsers, its layouter and line builders, the listeners on its items' widgets, and its
 * factories. No record or string names the file; the names of the helper classes and the
 * static functions are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/algorithm>
#include <std/ctype.h>
#include <std/list>
#include <std/map>
#include <std/math.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/string.h>
#include <std/vector>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/core/handler.h>
#include <widget/factory.h>
#include <widget/w_plane.h>
#include <widget/w_plane_button.h>
#include <widget/w_rich_text.h>
#include <widget/w_text_box.h>

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace ui {

/* the font sizes of the size 0 (the base) and of font size= 1 to 7 */
static const float s_font_size[8] = {
	30.0f, 16.0f, 20.0f, 24.0f, 28.0f, 32.0f, 36.0f, 40.0f,
};

/* where a word may break: after one of these */
static const wchar_t s_break_chars[] = L")}]!@#%^&*-=_+;:~,./<>?\t\n ";

/* the vector with the two as its x and y */
static inline math::v4 XY(const Vector2 &xy)
{
	return math::v4(Vector4(xy, Vector2(0.0f)));
}

/* the colour of the RGBA bytes */
static inline math::v4 ColorV4(uint32_t color)
{
	return math::v4(Vector4(vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(vmovl_u8(
							vreinterpret_u8_u32(vdup_n_u32(color)))))),
					  vdupq_n_f32(1.0f / 255.0f))));
}

/* the items deleted and the list emptied */
template<class _Ty>
void DeleteItems(list<_Ty *> &items)
{
	for (typename list<_Ty *>::iterator it = items.begin(); it != items.end(); ++it)
		delete *it;
	items.clear();
}

/* what a link's widget sends its rich text when it is decided: the link */
class LinkListener : public EventListener {
public:
	LinkListener(RichText *owner, int32_t link) : m_owner(owner), m_link(link)
	{
	}

	virtual ~LinkListener()
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	RichText *m_owner;
	int32_t m_link;
};

static void *Alloc(size_t size);
static void Free(void *ptr);

RichText::Layouter::Layouter()
	: m_state(0), m_queue(NULL), m_out(NULL), m_height(0.0f), m_line_spacing(1.0f),
	  m_top(0.0f), m_bottom(0.0f)
{
}

RichText::RichText(Widget *parent, CreateParam *param)
	: ScrollViewBase(parent, param), m_str(NULL), m_next_bottom(NULL), m_next_top(NULL),
	  m_plain(0), m_color(1.0f, 1.0f, 1.0f, 1.0f), m_scale(1.0f), m_no_image_size(false),
	  m_no_link(true), m_no_list_type(false), m_flags(0), m_content_height(0.0f),
	  m_visible_top(0.0f), m_layout_done(false), m_scroll_mode(1), m_builder_count(3),
	  m_line_spacing(1.0f), m_margin_x(0.0f), m_margin_y(0.0f), m_margin_width(0.0f),
	  m_reserved_byte0(false), m_reserved_byte1(true)
{
	m_alloc = Alloc;
	m_free = Free;
	m_layouter.m_owner = this;
	m_view_parent->SetOrigin(1, 2, 0, NULL);

	float *dst = m_font_size;

	for (const float *src = s_font_size; src != s_font_size + 1; ++src)
		*dst++ = *src;
	for (int32_t i = 0; i < 8; ++i)
		m_font_size[i] = s_font_size[i];
}

const char RichText::m_widget_type[] = "rich_text";

bool RichText::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ScrollViewBase::IsInherit(type);
}

char *RichText::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *RichText::TypeName()
{
	return m_widget_type;
}

void RichText::Layouter::Reset()
{
	m_state = 0;
	for (vector<LineBuilder>::iterator it = m_builders.begin(); it != m_builders.end(); ++it)
		it->m_state = LineBuilder::STATE_IDLE;
	for (list<LineGroup>::iterator it = m_groups.begin(); it != m_groups.end(); ++it)
		DeleteItems(it->m_items);
	m_groups.clear();
}

RichText::Layouter::~Layouter()
{
	Reset();
}

RichText::~RichText()
{
	for (ElementQueue::iterator it = m_queue.begin(); it != m_queue.end(); ++it)
		delete *it;
	for (list<Item *>::iterator it = m_lines.begin(); it != m_lines.end(); ++it)
		delete *it;
	for (vector<Item *>::iterator it = m_items.begin(); it != m_items.end(); ++it)
		delete *it;
	if (m_str != NULL)
		m_free(m_str);
}

void RichText::UpdateViewBox()
{
	float height = m_content_height;

	if (height == 0.0f) {
		SetViewBox(math::v4(Vector4(0.0f)));
	} else {
		float size_y = GetSize(0)->extract_y();

		if (height < size_y)
			height = size_y;

		float half_y = size_y * 0.5f;
		float half_x = GetSize(0)->extract_x() * 0.5f;

		SetViewBox(math::v4(-half_x, half_y, half_x, half_y - height));
	}
}

void RichText::Clear()
{
	m_queue.Clear();
	m_images.clear();
	if (m_str != NULL) {
		m_free(m_str);
		m_str = NULL;
	}
	m_content_height = 0.0f;
	m_visible_top = 0.0f;
	m_layout_done = true;
	m_layouter.Reset();
	DeleteItems(m_lines);
	m_items_top.clear();
	m_items_bottom.clear();
	for (vector<Item *>::iterator it = m_items.begin(); it != m_items.end(); ++it) {
		if ((*it)->m_widget != NULL)
			(*it)->m_widget->DestroyWidget();
		delete *it;
	}
	m_items.clear();
	UpdateViewBox();
}

void RichText::LinkFocus()
{
	vector<Item *>::iterator prev = m_items.end();

	for (vector<Item *>::iterator it = m_items.begin(); it != m_items.end(); ++it) {
		if (m_scroll_mode == 1)
			continue;

		Widget *widget = (*it)->m_widget;

		if (widget == NULL || (*it)->m_type != ITEM_TYPE_LINK_TEXT)
			continue;
		if (prev == m_items.end()) {
			ScePafWidget_EC6175D6(widget, true);
		} else {
			int32_t dir = 0;
			int32_t back = 1;

			if (m_scroll_mode == 3) {
				dir = 2;
				back = 3;
			}
			widget->SetFocusDirectionWidget(dir, (*prev)->m_widget);
			(*prev)->m_widget->SetFocusDirectionWidget(back, (*it)->m_widget);
		}
		prev = it;
	}
}

void RichText::CreateItemWidget(Item *item)
{
	switch (item->m_type) {
	case ITEM_TYPE_TEXT:
	case ITEM_TYPE_LINK_TEXT: {
		TextItem *text_item = (TextItem *)item;
		Text *text = new Text(m_view_parent, NULL);

		item->m_widget = text;
		text->SetPos(XY(item->m_pos.getImpl()));
		text->SetAdjust(2, 2, 0);
		text->SetAlign(1, 1, 0, NULL);
		scePafWidgetSetFontSize(text, text_item->m_font_size, graph::TextStyleAttribute_Point,
					0, 0);
		text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
					ColorV4(text_item->m_color));
		text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, text_item->m_bold);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 2);
		text->SetString(wstring(text_item->m_text));
		if (item->m_type == ITEM_TYPE_LINK_TEXT) {
			ScePafWidget_4D5F35D5(text, 0, sce_paf_wcslen(text_item->m_text));
			text->AddEventListener(0x10000003,
					       new LinkListener(this,
								((LinkTextItem *)item)->m_link));
		}
		break;
	}
	case ITEM_TYPE_IMAGE: {
		ImageItem *image_item = (ImageItem *)item;
		Widget *image;

		if (image_item->m_link >= 0) {
			image = new PlaneButton(m_view_parent, NULL);
			image->AddEventListener(0x10000008,
						new LinkListener(this, image_item->m_link));
		} else {
			image = new Plane(m_view_parent, NULL);
		}
		item->m_widget = image;
		image->SetPos(XY(item->m_pos.getImpl()));
		image->SetSize(XY(item->m_size.getImpl()));
		image->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		image->SetAlign(1, 1, 0, NULL);

		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		e.m_data[0] = item->m_index;
		DoEvent(CB_RICH_TEXT_SCROLL_IN, &e);
		break;
	}
	}
}

void RichText::scroll_down(float top)
{
	float bottom = top - GetSize(0)->extract_y();

	if (m_items_bottom.size() != 0 && m_next_bottom == m_items_bottom.end())
		--m_next_bottom;
	while ((*m_next_bottom)->m_bottom <= top) {
		if ((*m_next_bottom)->m_top > bottom && (*m_next_bottom)->m_widget == NULL)
			CreateItemWidget(*m_next_bottom);
		if (m_next_bottom == m_items_bottom.begin())
			break;
		--m_next_bottom;
	}
	if (m_items_top.size() != 0 && m_next_top == m_items_top.end())
		--m_next_top;
	while ((*m_next_top)->m_top < bottom) {
		if ((*m_next_top)->m_widget != NULL) {
			(*m_next_top)->m_widget->DestroyWidget();
			(*m_next_top)->m_widget = NULL;
		}
		if (m_next_top == m_items_top.begin())
			break;
		--m_next_top;
	}
	LinkFocus();
}

void RichText::scroll_up(float top)
{
	float bottom = top - GetSize(0)->extract_y();

	for (; m_next_top != m_items_top.end(); ++m_next_top) {
		if ((*m_next_top)->m_top < bottom)
			break;
		if ((*m_next_top)->m_bottom < top && (*m_next_top)->m_widget == NULL)
			CreateItemWidget(*m_next_top);
	}
	for (; m_next_bottom != m_items_bottom.end(); ++m_next_bottom) {
		if ((*m_next_bottom)->m_bottom <= top)
			break;
		if ((*m_next_bottom)->m_widget != NULL) {
			(*m_next_bottom)->m_widget->DestroyWidget();
			(*m_next_bottom)->m_widget = NULL;
		}
	}
	LinkFocus();
}

void RichText::SetVisibleTop(float top)
{
	if (m_items.size() == 0)
		return;
	SetViewFocus(math::v2(0.0f, -top), NULL, true, true);

	float max = 0.0f;
	float range = m_content_height - GetSize(0)->extract_y();
	float old = m_visible_top;

	if (range > 0.0f)
		max = range;
	if (top < 0.0f)
		top = 0.0f;
	if (max > top)
		max = top;
	m_visible_top = max;
	if (old < max)
		scroll_up(-max);
	else if (old > max)
		scroll_down(-max);
}

void RichText::Layouter::Start(ElementQueue *queue, list<Item *> *out)
{
	m_builders.resize(m_builder_count, LineBuilder());
	for (vector<LineBuilder>::iterator it = m_builders.begin(); it != m_builders.end(); ++it) {
		it->m_state = LineBuilder::STATE_IDLE;
		it->m_owner = m_owner;
		it->m_group = NULL;
		it->m_next_group = NULL;
		if (it->m_text_widget != NULL) {
			it->m_text_widget->DeleteEventCallback(0x100004, NULL, NULL);
			it->m_text_widget->SetString(wstring(L""));
		}
		it->m_line_height = m_line_height;
		it->m_line_spacing = m_line_spacing;
	}
	Reset();
	m_queue = queue;
	m_it = queue->begin();
	m_out = out;
	m_state = STATE_RUNNING;
}

/* Sony's build calls it rather than inlining it. */
__attribute__((noinline)) int32_t RichText::StartLayout()
{
	DeleteItems(m_lines);

	float line_height = m_font_size[3] * m_scale;

	m_layouter.m_flags |= m_flags;
	m_layouter.m_builder_count = m_builder_count;
	m_layouter.m_line_height = line_height;
	m_layouter.m_line_spacing = m_line_spacing;
	m_layouter.m_width = GetSize(0)->extract_x() - m_margin_width;
	m_layouter.m_top = m_margin_x;
	m_layouter.m_bottom = m_margin_y;
	m_layouter.Start(&m_queue, &m_lines);
	return 0;
}


/* the colour as RGBA bytes, each saturated */
static inline uint32_t ColorRGBA8(const math::v4 &color)
{
	uint16x4_t rgba = vqmovn_u32(
		vcvtq_u32_f32(vmulq_f32(color.getImpl().get128(), vdupq_n_f32(255.0f))));

	return vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(rgba, rgba))), 0);
}

/* the character of the UTF-8 sequence at p ('?' for a bad one), p moved past it */
static inline uint32_t DecodeUTF8(const unsigned char *&p, const unsigned char *end)
{
	uint32_t ch = *p++;

	if (ch >= 0x80) {
		if (ch >= 0xC2 && ch <= 0xF4) {
			uint32_t limit = 0x40;

			ch &= 0x7F;
			for (;;) {
				if (p == end) {
					ch = '?';
					break;
				}

				uint32_t next = *p++;
				uint32_t high = (limit - 1) & ch;

				limit <<= 5;
				ch = (next & 0x3F) | (high << 6);
				if (ch < limit)
					break;
			}
		} else {
			ch = '?';
		}
	}
	if (ch > 0xFFFD)
		ch = '?';
	return ch;
}

/* a wide copy of the len characters */
static inline wchar_t *CopyText(const wchar_t *src, uint32_t len)
{
	wchar_t *text = new wchar_t[len + 1];

	if (text != NULL) {
		sce_paf_wcsncpy(text, src, len);
		text[len] = 0;
	}
	return text;
}

/* the characters of the text being parsed, grown 256 at a time */
class WideBuffer {
public:
	WideBuffer() : m_buf(NULL), m_size(0), m_len(0)
	{
	}

	~WideBuffer()
	{
		delete[] m_buf;
	}

	void PushBack(wchar_t ch)
	{
		if (m_len >= m_size) {
			uint32_t size = m_len != 0 ? m_size + 256 : 2040;
			wchar_t *buf = new wchar_t[size];

			if (buf == NULL)
				return;
			sce_paf_memcpy(buf, m_buf, m_size * sizeof(wchar_t));
			delete[] m_buf;
			m_size = size;
			m_buf = buf;
		}
		m_buf[m_len++] = ch;
	}

	/* emptied, and freed when it has grown */
	void Reset()
	{
		m_len = 0;
		if (m_size > 2040) {
			delete[] m_buf;
			m_buf = NULL;
			m_size = 0;
		}
	}

	wchar_t *m_buf;
	uint32_t m_size;
	uint32_t m_len;
};

/* turns a plain text into text and br elements */
class TextParser {
public:
	TextParser(const char *begin, const char *end, const math::v4 &size, const math::v4 &color,
		   float font_size)
		: m_cur(begin), m_end(end), m_size(size), m_color(color), m_font_size(font_size)
	{
	}

	/* the text so far made a text element */
	void FlushText();
	void PushBr();
	void Parse();

	const char *m_cur;
	const char *m_end;
	math::v4 m_size;
	math::v4 m_color;
	float m_font_size;
	RichText::ElementQueue m_queue;
	RichText::TextElement m_style;
	WideBuffer m_text;
};

void TextParser::FlushText()
{
	if (m_text.m_len == 0)
		return;

	RichText::TextElement *e = new RichText::TextElement(m_style);

	if (e == NULL)
		return;
	e->m_text = CopyText(m_text.m_buf, m_text.m_len);
	if (e->m_text != NULL) {
		m_text.Reset();
		m_queue.push_back(e);
	} else {
		delete e;
	}
}

void TextParser::PushBr()
{
	RichText::BrElement *br = new RichText::BrElement;

	if (br != NULL)
		m_queue.push_back(br);
}

void TextParser::Parse()
{
	m_style.m_bold = false;
	m_style.m_text = NULL;
	m_style.m_font_size = m_font_size;
	m_style.m_indent = 0.0f;
	m_style.m_mark = false;
	m_style.m_nobr = 0;
	m_style.m_center = false;
	m_text.m_len = 0;
	m_style.m_color = ColorRGBA8(m_color);
	while (m_cur != m_end) {
		char ch = *m_cur;

		if (ch == '\n') {
			FlushText();
			PushBr();
			++m_cur;
		} else if (ch == '\r') {
			++m_cur;
		} else {
			const unsigned char *p = (const unsigned char *)m_cur;
			uint32_t wch = DecodeUTF8(p, (const unsigned char *)m_end);

			m_cur = (const char *)p;
			m_text.PushBack(wch);
		}
	}
	FlushText();
}

/* a tag of the marked-up text: what its start and its end tags do */
class HtmlParser;

typedef void (HtmlParser::*TagOpen)(const string &tag, map<string, string> &attrs);
typedef void (HtmlParser::*TagClose)(const string &tag);

class Tag {
public:
	TagOpen m_open;
	TagClose m_close;
};

class TagEntry {
public:
	const char *m_name;
	TagOpen m_open;
	TagClose m_close;
};

class EntityEntry {
public:
	const char *m_name;
	wchar_t m_char;
};

class ColorEntry {
public:
	const char *m_name;
	uint32_t m_rgb;
};

/* the style of the text: a link text's, the tag that set it and its list's numbering */
class Style : public RichText::LinkTextElement {
public:
	Style()
	{
	}

	virtual ~Style()
	{
	}

	string m_tag;
	char m_list_type;
	int32_t m_list_index;
};

/* an image of the text: its url and its size in the text */
class ImageEntry {
public:
	ImageEntry(const string &url, const math::v2 &size) : m_url(url), m_size(size)
	{
	}

	string m_url;
	math::v2 m_size;
};

/* turns a marked-up text into elements, its images and its links */
class HtmlParser {
public:
	HtmlParser(const char *begin, const char *end, const math::v4 &size);

	~HtmlParser()
	{
		delete[] m_text.m_buf;
		m_text.m_buf = NULL;
	}

	void Parse();
	/* its elements, images and links taken */
	void Take(RichText::ElementQueue *queue, list<ImageEntry> *images, list<string> *links);

	/* the element queued, or deleted inside a hidden block */
	bool PushElement(RichText::Element *e);
	/* the text so far made a text or link text element */
	void FlushText();
	void PutChar(wchar_t ch);
	/* the character of the entity at the cursor ('&' where it is none) */
	wchar_t ParseEntity();
	/* the style restored where the tag set it */
	bool PopStyle(const string &tag);
	/* a PS3_ND_start comment: the text hidden up to its end */
	void BeginHidden();
	bool ParseTag();

	/* the text and link text element of the style, without its text */
	RichText::TextElement *MakeTextElement()
	{
		if (m_style.m_link >= 0)
			return new RichText::LinkTextElement(m_style);

		RichText::TextElement *e = new RichText::TextElement(m_style);

		e->m_type = RichText::Element::TYPE_TEXT;
		return e;
	}

	void PushStyle(const string &tag)
	{
		if (m_stack.size() < 16) {
			m_stack.push_front(m_style);
			m_stack.front().m_tag = tag;
		}
	}

	void Break()
	{
		RichText::BrElement *br = new RichText::BrElement;

		if (br != NULL) {
			PushElement(br);
			m_newline = false;
			m_space = false;
			m_last = 0;
		}
	}

	/* the tags' handlers */
	void OnA(const string &tag, map<string, string> &attrs);
	void OnAEnd(const string &tag);
	void OnB(const string &tag, map<string, string> &attrs);
	void OnBEnd(const string &tag);
	void OnBr(const string &tag, map<string, string> &attrs);
	void OnCenter(const string &tag, map<string, string> &attrs);
	void OnCenterEnd(const string &tag);
	void OnFont(const string &tag, map<string, string> &attrs);
	void OnFontEnd(const string &tag);
	void OnH(const string &tag, map<string, string> &attrs);
	void OnHEnd(const string &tag);
	void OnHead(const string &tag, map<string, string> &attrs);
	void OnHeadEnd(const string &tag);
	void OnImg(const string &tag, map<string, string> &attrs);
	void OnLi(const string &tag, map<string, string> &attrs);
	void OnLiEnd(const string &tag);
	void OnNobr(const string &tag, map<string, string> &attrs);
	void OnNobrEnd(const string &tag);
	void OnOl(const string &tag, map<string, string> &attrs);
	void OnOlEnd(const string &tag);
	void OnP(const string &tag, map<string, string> &attrs);
	void OnPEnd(const string &tag);
	void OnTable(const string &tag, map<string, string> &attrs);
	void OnTableEnd(const string &tag);
	void OnTd(const string &tag, map<string, string> &attrs);
	void OnTdEnd(const string &tag);
	void OnTr(const string &tag, map<string, string> &attrs);
	void OnTrEnd(const string &tag);
	void OnUl(const string &tag, map<string, string> &attrs);
	void OnUlEnd(const string &tag);

	map<string, Tag> m_tags;
	map<string, wchar_t> m_entities;
	/* the longest entity name */
	uint32_t m_entity_len;
	map<string, uint32_t> m_colors;
	RichText::ElementQueue m_queue;
	list<ImageEntry> m_images;
	list<string> m_links;
	const char *m_begin;
	const char *m_end;
	math::v4 m_size;
	const char *m_cur;
	WideBuffer m_text;
	Style m_style;
	/* the styles the open tags replaced, the latest first */
	list<Style> m_stack;
	/* how many hidden blocks (head, PS3_ND_start) it is in */
	int32_t m_hidden;
	math::v4 m_color;
	float m_scale;
	float m_font_size[8];
	bool m_no_image_size;
	bool m_no_link;
	bool m_no_list_type;
	uint32_t m_flags;
	/* a line break and a space seen since the last character, and the last character */
	bool m_newline;
	bool m_space;
	wchar_t m_last;
	int32_t m_list_depth;
	int32_t m_nobr_count;
	/* the table being parsed, and its row and column */
	RichText::TableElement *m_table;
	uint32_t m_row;
	uint32_t m_col;
};

void HtmlParser::Take(RichText::ElementQueue *queue, list<ImageEntry> *images,
		      list<string> *links)
{
	queue->Take(m_queue);
	images->splice(images->end(), m_images);
	links->splice(links->end(), m_links);
}

bool HtmlParser::PushElement(RichText::Element *e)
{
	if (m_hidden <= 0) {
		m_queue.push_back(e);
		return true;
	}
	delete e;
	return false;
}

void HtmlParser::FlushText()
{
	if (m_text.m_len == 0)
		return;

	RichText::TextElement *e = MakeTextElement();

	if (e == NULL)
		return;
	e->m_text = CopyText(m_text.m_buf, m_text.m_len);
	if (e->m_text != NULL) {
		m_text.Reset();
		PushElement(e);
	} else {
		delete e;
	}
}

void HtmlParser::PutChar(wchar_t ch)
{
	if (m_space)
		m_text.PushBack(L' ');
	else if (m_newline && ch < 0x2E00)
		m_text.PushBack(L' ');
	m_last = ch;
	m_space = false;
	m_newline = false;
	m_text.PushBack(ch);
}

wchar_t HtmlParser::ParseEntity()
{
	const char *start = m_cur;
	const char *p = start + 1;
	wchar_t ch;

	if (p == m_end)
		return L'&';
	if (*p == '#') {
		p = start + 2;
		if (p == m_end)
			return L'&';

		uint32_t value = 0;
		bool over = false;

		if ((sce_paf_look_ctype_table(*p) & 4) != 0) {
			for (; p != m_end; ++p) {
				uint32_t digit = *p - '0';

				if (digit > 9)
					break;
				value = digit + value * 10;
				if (value > 0xFFFF)
					over = true;
			}
		} else {
			if (*p != 'x' && *p != 'X')
				return L'&';
			p = start + 3;
			if (p == m_end)
				return L'&';
			if ((sce_paf_look_ctype_table(*p) & 0x44) == 0)
				return L'&';
			for (; p != m_end; ++p) {
				int32_t c = *p;
				uint32_t digit;

				if (c >= 'A' && c <= 'F') {
					digit = c - 'A' + 10;
				} else if (c >= 'a' && c <= 'f') {
					digit = c - 'a' + 10;
				} else {
					digit = c - '0';
					if (digit > 9)
						break;
				}
				value = digit + value * 16;
				if (value > 0xFFFF)
					over = true;
			}
		}
		ch = L'?';
		if (!over)
			ch = (wchar_t)value;
	} else {
		const char *name = start + 1;
		uint32_t len = 0;

		for (; p != m_end; len = ++p - name) {
			int32_t c = *p;

			if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
				break;
			if (len > m_entity_len)
				return L'&';
		}

		string key(name, len);
		map<string, wchar_t>::iterator it = m_entities.find(key);

		if (it == m_entities.end())
			return L'&';
		ch = it->second;
	}
	if (p == m_end || *p != ';')
		--p;
	m_cur = p;
	return ch;
}

bool HtmlParser::PopStyle(const string &tag)
{
	if (m_stack.size() == 0)
		return false;
	if (m_stack.front().m_tag != tag)
		return false;
	m_style = m_stack.front();
	m_stack.pop_front();
	return true;
}

/* Sony's build calls it rather than inlining it. */
__attribute__((noinline)) void HtmlParser::BeginHidden()
{
	FlushText();
	PushStyle(string("PS3_ND_start"));
	++m_hidden;
}

/* where it is lowered */
static char ToLower(char ch);

static const char s_delims[] = "= \t\n/>";

/* the word at the cursor (in quotes, or up to a delimiter), the cursor moved past it */
static string ReadWord(const char *&cur, const char *end, bool single, bool lower)
{
	const char *start = cur;

	if (start == end)
		return string("");

	char quote = *start;

	if (quote == '"' || (quote == '\'' && single)) {
		const char *first = start + 1;
		const char *p;

		cur = first;
		for (p = first; p != end; ++p)
			if (*p == quote)
				break;

		string word(first, p - first);

		cur = p;
		if (p != end)
			cur = p + 1;
		return word;
	}

	const char *p = start;

	for (; p != end; ++p) {
		const char *d;

		for (d = s_delims; d != s_delims + sizeof(s_delims) - 1; ++d)
			if (*p == *d)
				break;
		if (d != s_delims + sizeof(s_delims) - 1)
			break;
	}
	if (lower) {
		vector<char> buf(p - start);
		vector<char>::iterator out = buf.begin();

		for (const char *in = cur; in != p; ++out)
			*out = ToLower(*in++);
		cur = p;
		return string(&buf[0], buf.size());
	}
	cur = p;
	return string(start, p - start);
}

static const char s_comment_end[] = "-->";

bool HtmlParser::ParseTag()
{
	const char *start = m_cur;
	const char *p = start + 1;

	if (p == m_end)
		return false;

	char c = start[1];

	if (c != '!' && c != '/' && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
		return false;
	m_cur = p;
	if (start[1] == '!' && (uint32_t)(m_end - p) >= 3 && start[2] == '-' && start[3] == '-') {
		const char *q = start + 4;

		for (m_cur = q; q != m_end; ++q) {
			const char *s = s_comment_end;

			for (const char *i = q; s != s_comment_end + 3; ++i) {
				if (*i != *s)
					break;
				++s;
			}
			if (s == s_comment_end + 3)
				break;
		}
		if (q != m_end) {
			string word = ReadWord(m_cur, q, false, true);

			if (word == "ps3_nd_start") {
				BeginHidden();
			} else if (word == "ps3_nd_end") {
				if (PopStyle(string("PS3_ND_start")))
					--m_hidden;
			}
			m_cur = q + 2;
		}
		return true;
	}

	bool close = false;

	if (start[1] == '/') {
		m_cur = start + 2;
		if (m_cur == m_end)
			return true;
		close = true;
	}

	string name = ReadWord(m_cur, m_end, false, true);

	if (m_cur == m_end)
		return true;

	map<string, string> attrs;

	while (m_cur != m_end && attrs.size() < 8) {
		while (m_cur != m_end && (sce_paf_look_ctype_table(*m_cur) & 8) != 0)
			++m_cur;
		if (m_cur == m_end)
			return true;

		string key = ReadWord(m_cur, m_end, false, true);

		if (m_cur == m_end)
			return true;
		while (m_cur != m_end && (sce_paf_look_ctype_table(*m_cur) & 8) != 0)
			++m_cur;
		if (m_cur == m_end)
			return true;

		char d = *m_cur;

		if (d == '/' || d == '>') {
			if (key.size() != 0)
				attrs[key] = "";
			break;
		}
		++m_cur;
		if (d == '=') {
			while (m_cur != m_end && (sce_paf_look_ctype_table(*m_cur) & 8) != 0)
				++m_cur;
			if (m_cur == m_end)
				return true;

			string value = ReadWord(m_cur, m_end, (m_flags & 1) != 0, false);

			if (m_cur == m_end)
				return true;
			attrs[key] = value;
		}
	}

	map<string, Tag>::iterator it = m_tags.find(name);

	if (it != m_tags.end()) {
		if (close) {
			if (it->second.m_close != NULL)
				(this->*it->second.m_close)(name);
		} else {
			(this->*it->second.m_open)(name, attrs);
		}
	}
	while (m_cur != m_end && *m_cur != '>')
		++m_cur;
	return true;
}

void HtmlParser::Parse()
{
	m_style.m_bold = false;
	m_style.m_indent = 0.0f;
	m_style.m_mark = false;
	m_style.m_nobr = 0;
	m_style.m_list_type = 0;
	m_style.m_center = false;
	m_style.m_color = ColorRGBA8(m_color);
	m_style.m_link = -1;
	m_text.m_len = 0;
	m_newline = false;
	m_space = false;
	m_last = 0;
	m_list_depth = 0;
	m_nobr_count = 0;
	m_table = NULL;
	m_cur = m_begin;
	m_style.m_font_size = m_font_size[3] * m_scale;
	for (; m_cur != m_end; ++m_cur) {
		switch (*m_cur) {
		case ' ':
		case '\t':
			if (m_last != 0)
				m_space = true;
			break;
		case '\n':
			if (m_last != 0) {
				if (m_newline)
					m_space = true;
				else
					m_newline = true;
			}
			break;
		case '\r':
			break;
		default:
			if (*m_cur == '<' && ParseTag()) {
				if (m_cur == m_end) {
					FlushText();
					return;
				}
			} else {
				wchar_t ch;

				if (*m_cur == '&') {
					ch = ParseEntity();
				} else {
					const unsigned char *p = (const unsigned char *)m_cur;

					ch = DecodeUTF8(p, (const unsigned char *)m_end);
					m_cur = (const char *)p - 1;
				}
				if (m_hidden == 0)
					PutChar(ch);
			}
			break;
		}
	}
	FlushText();
}

static const TagEntry s_tags[] = {
	{ "a", &HtmlParser::OnA, &HtmlParser::OnAEnd },
	{ "b", &HtmlParser::OnB, &HtmlParser::OnBEnd },
	{ "br", &HtmlParser::OnBr, NULL },
	{ "center", &HtmlParser::OnCenter, &HtmlParser::OnCenterEnd },
	{ "font", &HtmlParser::OnFont, &HtmlParser::OnFontEnd },
	{ "h1", &HtmlParser::OnH, &HtmlParser::OnHEnd },
	{ "h2", &HtmlParser::OnH, &HtmlParser::OnHEnd },
	{ "h3", &HtmlParser::OnH, &HtmlParser::OnHEnd },
	{ "h4", &HtmlParser::OnH, &HtmlParser::OnHEnd },
	{ "h5", &HtmlParser::OnH, &HtmlParser::OnHEnd },
	{ "h6", &HtmlParser::OnH, &HtmlParser::OnHEnd },
	{ "head", &HtmlParser::OnHead, &HtmlParser::OnHeadEnd },
	{ "img", &HtmlParser::OnImg, NULL },
	{ "li", &HtmlParser::OnLi, &HtmlParser::OnLiEnd },
	{ "nobr", &HtmlParser::OnNobr, &HtmlParser::OnNobrEnd },
	{ "ol", &HtmlParser::OnOl, &HtmlParser::OnOlEnd },
	{ "p", &HtmlParser::OnP, &HtmlParser::OnPEnd },
	{ "strong", &HtmlParser::OnB, &HtmlParser::OnBEnd },
	{ "table", &HtmlParser::OnTable, &HtmlParser::OnTableEnd },
	{ "td", &HtmlParser::OnTd, &HtmlParser::OnTdEnd },
	{ "tr", &HtmlParser::OnTr, &HtmlParser::OnTrEnd },
	{ "ul", &HtmlParser::OnUl, &HtmlParser::OnUlEnd },
};

static const EntityEntry s_entities[] = {
	{ "lt", L'<' },
	{ "gt", L'>' },
	{ "amp", L'&' },
	{ "apos", L'\'' },
	{ "quot", L'"' },
	{ "nbsp", 0xA0 },
	{ "reg", 0xAE },
	{ "copy", 0xA9 },
	{ "trade", 0x2122 },
};

static const ColorEntry s_colors[] = {
	{ "black", 0x000000 },
	{ "gray", 0x808080 },
	{ "silver", 0xC0C0C0 },
	{ "white", 0xFFFFFF },
	{ "red", 0xFF0000 },
	{ "yellow", 0xFFFF00 },
	{ "lime", 0x00FF00 },
	{ "aqua", 0x00FFFF },
	{ "blue", 0x0000FF },
	{ "fuchsia", 0xFF00FF },
	{ "maroon", 0x800000 },
	{ "olive", 0x808000 },
	{ "green", 0x008000 },
	{ "teal", 0x008080 },
	{ "navy", 0x000080 },
	{ "purple", 0x800080 },
	{ "orange", 0xFFA500 },
};

HtmlParser::HtmlParser(const char *begin, const char *end, const math::v4 &size)
	: m_begin(begin), m_end(end), m_size(size), m_hidden(0), m_color(1.0f, 1.0f, 1.0f, 1.0f),
	  m_scale(1.0f), m_no_image_size(false)
{
	if (m_tags.size() == 0) {
		for (uint32_t i = 0; i < sizeof(s_tags) / sizeof(s_tags[0]); ++i) {
			Tag &tag = m_tags[string(s_tags[i].m_name)];

			tag.m_open = s_tags[i].m_open;
			tag.m_close = s_tags[i].m_close;
		}
	}
	if (m_entities.size() == 0) {
		m_entity_len = 0;
		for (uint32_t i = 0; i < sizeof(s_entities) / sizeof(s_entities[0]); ++i) {
			m_entities[string(s_entities[i].m_name)] = s_entities[i].m_char;

			uint32_t len = sce_paf_strlen(s_entities[i].m_name);

			if (m_entity_len < len)
				m_entity_len = len;
		}
	}
	if (m_colors.size() == 0) {
		for (uint32_t i = 0; i < sizeof(s_colors) / sizeof(s_colors[0]); ++i)
			m_colors[string(s_colors[i].m_name)] = s_colors[i].m_rgb;
	}
	m_style.m_text = NULL;
}

int32_t RichText::Parse()
{
	m_queue.Clear();

	const char *begin = m_str;
	const char *end = m_str_end;

	if (m_plain != 0) {
		TextParser parser(begin, end, *GetSize(0), m_color, m_font_size[3]);

		parser.Parse();
		m_queue.Take(parser.m_queue);
	} else {
		HtmlParser parser(begin, end, *GetSize(0));

		parser.m_color = m_color;
		parser.m_scale = m_scale;
		for (int32_t i = 0; i < 8; ++i)
			parser.m_font_size[i] = m_font_size[i];
		parser.m_no_image_size = m_no_image_size;
		parser.m_no_link = m_no_link;
		parser.m_no_list_type = m_no_list_type;
		parser.m_flags = m_flags;
		parser.Parse();

		list<ImageEntry> images;
		list<string> links;

		parser.Take(&m_queue, &images, &links);

		ImageInfo info;

		m_images.resize(images.size(), info);

		uint32_t i = 0;

		for (list<ImageEntry>::iterator it = images.begin(); it != images.end(); ++it, ++i) {
			m_images[i].m_state = ImageInfo::STATE_REQUESTED;
			m_images[i].m_url = it->m_url;
			m_images[i].m_html_size = it->m_size;
		}
		for (uint32_t j = 0; j < m_images.size(); ++j) {
			Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

			e.m_data[0] = j;
			DoEvent(CB_RICH_TEXT_REQ_IMG_SIZE, &e);
		}
		m_links.resize(links.size(), string());
		copy(links.begin(), links.end(), m_links.begin());
	}
	m_free(m_str);
	m_str = NULL;
	return 0;
}

int32_t RichText::SetCText(const char *text, size_t len)
{
	Clear();
	m_str = (char *)m_alloc(len + 1);
	if (m_str == NULL)
		return -1;
	sce_paf_strncpy(m_str, text, len);
	m_str[len] = 0;
	m_layout_done = false;
	m_str_end = m_str + len;
	Parse();
	StartLayout();
	return 0;
}

int32_t RichText::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t index,
			     uint32_t unused)
{
	if (index < 0 || index >= (int32_t)m_items.size())
		return -1;
	if (m_items[index]->m_widget == NULL)
		return -1;
	m_items[index]->m_widget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	return m_items[index]->m_widget->SetTexture(surf);
}

Widget *RichText::GetItemWidget(int32_t index)
{
	if (index < 0 || index >= (int32_t)m_items.size())
		return NULL;
	return m_items[index]->m_widget;
}

/* what an image's widget does when its texture is ready: shown */
class TextureListener : public EventListener {
public:
	virtual ~TextureListener()
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e);
};

/* and what it sends its rich text: shown, and the item's index */
class TextureReadyListener : public EventListener {
public:
	TextureReadyListener(RichText *owner) : m_owner(owner)
	{
	}

	virtual ~TextureReadyListener()
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	RichText *m_owner;
};

int32_t RichText::SetDrawObjMaskBit(const void *param, int32_t index, uint32_t unused)
{
	if (index < 0)
		return -1;
	if (index >= (int32_t)m_items.size())
		return -1;

	Widget *widget = m_items[index]->m_widget;

	if (widget == NULL)
		return -1;
	widget->AddEventListener(0x100005, new TextureListener);
	widget->AddEventListener(0x100006, new TextureReadyListener(this));
	return widget->SetDrawObjMaskBit(param, 0, 0);
}

int32_t RichText::GetItemType(int32_t index) const
{
	if (index < 0 || index >= (int32_t)m_items.size())
		return 0;

	int32_t type = m_items[index]->m_type;

	if (type == ITEM_TYPE_LINK_TEXT)
		return ITEM_TYPE_TEXT;
	return type;
}

int32_t RichText::GetItemUrl(int32_t index, string &url) const
{
	if (index < 0 || index >= (int32_t)m_items.size())
		return -1;

	Item *item = m_items[index];

	if (item->m_type != ITEM_TYPE_IMAGE)
		return -1;
	url = ((ImageItem *)item)->m_url;
	return 0;
}

int32_t RichText::GetImageUrl(int32_t index, string &url) const
{
	if (index < 0 || index >= (int32_t)m_images.size())
		return -1;
	url = m_images[index].m_url;
	return 0;
}

int32_t RichText::GetHtmlImageSize(int32_t index, math::v4 &size)
{
	if (index < 0 || index >= (int32_t)m_images.size())
		return -1;
	size = XY(m_images[index].m_html_size.getImpl());
	return 0;
}

int32_t RichText::SetImageSize(int32_t index, const math::v4 &size)
{
	if (index < 0 || index >= (int32_t)m_images.size())
		return 0 - 1;

	ImageInfo &image = m_images[index];

	if (image.m_state == ImageInfo::STATE_REQUESTED) {
		image.m_state = ImageInfo::STATE_SIZED;
		image.m_size = math::v2(size.getImpl().getXY()) * m_scale;
	}
	return 0;
}

int32_t RichText::ScePafWidget_6A60296B(uint32_t size, float value)
{
	if (size >= 8)
		return -1;
	m_font_size[size] = value;
	return 0;
}

int32_t RichText::ScePafWidget_C7BE2076(int32_t index, string &url) const
{
	if (index < 0 || index >= (int32_t)m_links.size())
		return -1;
	url = m_links[index];
	return 0;
}

/* an item lower in the text than another, by its top and by its bottom */
class ByTop {
public:
	bool operator()(RichText::Item *a, RichText::Item *b) const
	{
		return a->m_top > b->m_top;
	}
};

class ByBottom {
public:
	bool operator()(RichText::Item *a, RichText::Item *b) const
	{
		return a->m_bottom > b->m_bottom;
	}
};

/* the heap below base[k] of the n from base[1] made a heap again */
template<class _RanIt, class _Pr>
void SiftDown(_RanIt base, int32_t k, int32_t n, _Pr pred)
{
	for (int32_t j = 2 * k; j <= n; j = 2 * k) {
		if (j < n && pred(*(base + j), *(base + j + 1)))
			++j;
		if (!pred(*(base + k), *(base + j)))
			break;
		swap(*(base + k), *(base + j));
		k = j;
	}
}

template<class _RanIt, class _Pr>
void HeapSort(_RanIt first, _RanIt last, _Pr pred)
{
	uint32_t n = last - first;

	--first;
	for (uint32_t k = n / 2; k >= 1; --k)
		SiftDown(first, k, n, pred);
	for (uint32_t m = n; m >= 2; --m) {
		swap(*(first + 1), *(first + m));
		SiftDown(first, 1, m - 1, pred);
	}
}

void RichText::BuildItems()
{
	m_queue.Clear();
	for (vector<Item *>::iterator it = m_items.begin(); it != m_items.end(); ++it) {
		if ((*it)->m_widget != NULL)
			(*it)->m_widget->DestroyWidget();
		delete *it;
	}
	m_items.resize(m_lines.size(), NULL);
	copy(m_lines.begin(), m_lines.end(), m_items.begin());
	m_lines.clear();
	m_items_top.resize(m_items.size(), NULL);
	copy(m_items.begin(), m_items.end(), m_items_top.begin());
	HeapSort(m_items_top.begin(), m_items_top.end(), ByTop());
	m_next_top = m_items_top.begin();
	m_items_bottom.resize(m_items.size(), NULL);
	copy(m_items.begin(), m_items.end(), m_items_bottom.begin());
	HeapSort(m_items_bottom.begin(), m_items_bottom.end(), ByBottom());
	m_next_bottom = m_items_bottom.begin();
	m_visible_top = -1.0f;
	SetVisibleTop(0.0f);
}


void RichText::Layouter::ParseTable()
{
	TableElement *element = (TableElement *)*m_it;

	++m_it;

	Table table;
	Cell cell;
	bool in_cell = false;

	table.m_rows = element->m_rows;
	table.m_cols = element->m_cols;
	for (; m_it != m_queue->end(); ++m_it) {
		Element *e = *m_it;

		if (e->m_type == Element::TYPE_CELL) {
			CellElement *cell_e = (CellElement *)e;

			cell.m_state = Cell::STATE_PENDING;
			cell.m_row = cell_e->m_row;
			cell.m_col = cell_e->m_col;
			cell.m_width = cell_e->m_width;
			cell.m_height = cell_e->m_height;
			cell.m_colspan = cell_e->m_colspan;
			cell.m_begin = m_it;
			cell.m_avail = m_width;
			if ((m_flags & 2) != 0 && cell_e->m_width != 0.0f)
				cell.m_avail = cell_e->m_width;
			if (cell_e->m_col + cell_e->m_colspan >= table.m_cols)
				cell.m_state = Cell::STATE_LAST;
			in_cell = true;
		} else if (e->m_type == Element::TYPE_CELL_END && in_cell) {
			cell.m_end = m_it;
			table.m_cells.push_back(cell);
			in_cell = false;
		} else if (e->m_type == Element::TYPE_TABLE_END) {
			if (table.m_cells.size() != 0) {
				m_groups.push_back(LineGroup());
				table.m_group = &m_groups.back();
				m_tables.push_back(table);

				Table &added = m_tables.back();

				for (vector<Cell>::iterator it = added.m_cells.begin();
				     it != added.m_cells.end(); ++it)
					if (it->m_state == Cell::STATE_PENDING)
						m_pending.push_back(&*it);
			}
			break;
		}
	}
}

void RichText::LineBuilder::AlignLine(list<Item *>::iterator first, list<Item *>::iterator last,
				      float y, float &height, float &text_height)
{
	float ascent = 0.0f;
	float descent = 0.0f;

	text_height = 0.0f;
	for (list<Item *>::iterator it = first; it != last; ++it) {
		Item *item = *it;

		if (item->m_type == ITEM_TYPE_TEXT || item->m_type == ITEM_TYPE_LINK_TEXT) {
			TextItem *text = (TextItem *)item;
			if (text->m_ascent > ascent)
				ascent = text->m_ascent;

			float below = text->m_size.extract_y() - text->m_ascent;

			if (below > descent)
				descent = below;
			text_height = max(text_height, text->m_size.extract_y());
		} else if (item->m_size.extract_y() > ascent) {
			ascent = item->m_size.extract_y();
		}
	}
	height = ascent + descent;
	for (list<Item *>::iterator it = first; it != last; ++it) {
		Item *item = *it;

		if (item->m_type == ITEM_TYPE_TEXT || item->m_type == ITEM_TYPE_LINK_TEXT)
			item->m_pos.getImpl().setY(sce::Vectormath::Simd::floatInVec(y - height));
		else
			item->m_pos.getImpl().setY(sce::Vectormath::Simd::floatInVec(y - ascent));
		item->m_bottom = item->m_pos.extract_y();
		item->m_top = item->m_pos.extract_y() + item->m_size.extract_y();
	}
}

void RichText::LineBuilder::Center(list<Item *>::iterator first, list<Item *>::iterator last,
				   float width, float x)
{
	float offset = (width - x) * 0.5f;

	if (offset > 0.0f)
		for (list<Item *>::iterator it = first; it != last; ++it)
			(*it)->m_pos = (*it)->m_pos + math::v2(offset, 0.0f);
}

void RichText::LineBuilder::EndLine()
{
	float height;
	float text_height;

	if (m_new_line) {
		m_text_widget->GetStyleAttribute(graph::TextStyleAttribute_Point, 0, height);
		text_height = height;
	} else {
		if (m_center)
			Center(m_line_start, m_group->m_items.end(), m_width, m_cursor.extract_x());
		AlignLine(m_line_start, m_group->m_items.end(), m_cursor.extract_y(), height,
			  text_height);
	}
	m_new_line = true;
	m_cursor.set_x(0.0f);
	m_cursor.set_y(m_cursor.extract_y() -
		       sce_paf_ceilf(height + text_height * (m_line_spacing - 1.0f)));
	m_group->m_height = -m_cursor.extract_y();
}

void RichText::LineBuilder::Rewind()
{
	m_it = m_nobr_it;
	while (m_group->m_items.size() > m_nobr_count) {
		delete m_group->m_items.back();
		if (m_group->m_items.size() != 0)
			m_group->m_items.pop_back();
	}
	m_line_start = m_nobr_line_start;
	m_new_line = m_nobr_new_line;
	m_text.clear();
	m_cursor = m_nobr_cursor;
}

bool RichText::LineBuilder::Place(Item *item, bool mark)
{
	item->m_widget = NULL;
	if (mark) {
		/* a list item's mark: before the indent */
		item->m_pos = m_cursor;
		item->m_pos.getImpl().setX(
			sce::Vectormath::Simd::floatInVec(item->m_pos.extract_x() - item->m_size.extract_x()));
		m_group->m_items.push_back(item);
	} else {
		if (m_cursor.extract_x() + item->m_size.extract_x() > m_width + 1.0f) {
			m_trim = true;
			if (m_nobr_rewind && m_nobr != 0) {
				/* the nobr block goes to the next line whole, once */
				m_nobr_rewind = false;
				delete item;
				Rewind();
				if (!m_new_line)
					EndLine();
				return false;
			}
			EndLine();
		}
		item->m_pos = m_cursor;
		m_group->m_items.push_back(item);
		m_cursor.getImpl().setX(
			sce::Vectormath::Simd::floatInVec(m_cursor.extract_x() + item->m_size.extract_x()));
		m_group->m_width = max(m_group->m_width, m_cursor.extract_x());
	}
	if (m_new_line) {
		m_new_line = false;
		m_line_start = --m_group->m_items.end();
	}
	if (m_cursor.extract_x() >= m_width) {
		m_trim = true;
		EndLine();
	}
	return true;
}

void RichText::LineBuilder::Run()
{
	m_state = STATE_RUNNING;
	for (uint32_t i = 0; i < 256; ++i) {
		if (m_it == m_end) {
			if (!m_new_line)
				EndLine();
			m_group->m_done = true;
			if (m_next_group == NULL) {
				m_text_widget->DestroyWidget();
				m_text_widget = NULL;
				m_state = STATE_IDLE;
				return;
			}
			m_it = m_next_it;
			m_width = m_next_width;
			m_end = m_next_end;
			m_group = m_next_group;
			m_next_group = NULL;
			DeleteItems(m_group->m_items);
			m_group->m_width = 0.0f;
			m_group->m_height = 0.0f;
			m_trim = false;
			m_text.clear();
			m_nobr = 0;
			m_nobr_rewind = true;
			m_cursor = math::v2(0.0f, 0.0f);
			continue;
		}

		Element *e = *m_it;

		switch (e->m_type) {
		case Element::TYPE_TEXT:
		case Element::TYPE_LINK_TEXT: {
			TextElement *text = (TextElement *)e;

			if (m_text.size() == 0) {
				m_text.assign(text->m_text);
				if (m_text.size() == 0) {
					++m_it;
					break;
				}
			}
			if (m_new_line)
				m_cursor.set_x(text->m_indent);
			if (text->m_nobr != m_nobr)
				SetNobr(text->m_nobr);
			if (m_nobr_rewind && m_nobr != 0) {
				m_text_widget->SetAdjust(2, 2, 0);
				m_text_widget->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap,
								  false);
			} else {
				m_text_widget->SetAdjust(0, 2, 0);
				m_text_widget->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap,
								  true);
			}
			if (m_trim) {
				m_trim = false;
				if (m_text.size() > 1 && m_text[0] == L' ')
					m_text.erase(0, 1);
			}
			m_center = text->m_center;
			m_text_widget->SetSize(
				math::v4(m_width - m_cursor.extract_x(), 0.0f, 0.0f, 0.0f));
			scePafWidgetSetFontSize(m_text_widget, text->m_font_size,
						graph::TextStyleAttribute_Point, 0, 0);
			m_text_widget->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0,
							 text->m_bold);
			/* measured by CBMeasure once the widget has laid the text out */
			m_text_widget->SetString(wstring(L""));
			m_text_widget->SetString(m_text);
			m_state = STATE_MEASURING;
			return;
		}
		case Element::TYPE_BR:
			EndLine();
			++m_it;
			break;
		case Element::TYPE_IMG: {
			ImgElement *img = (ImgElement *)e;
			math::v2 size = img->m_size;
			ImageInfo &info = m_owner->m_images[img->m_image];

			/* waits for the image's size where the text leaves it out */
			if ((size.extract_x() == 0.0f || size.extract_y() == 0.0f) &&
			    info.m_state != ImageInfo::STATE_SIZED)
				return;

			math::v2 image_size = info.m_size;

			if (size.extract_x() == 0.0f && size.extract_y() == 0.0f)
				size = image_size;
			else if (size.extract_x() == 0.0f)
				size.set_x(size.extract_y() * image_size.extract_x() /
					   image_size.extract_y());
			else if (size.extract_y() == 0.0f)
				size.set_y(size.extract_x() * image_size.extract_y() /
					   image_size.extract_x());
			if (m_new_line)
				m_cursor.set_x(img->m_indent);
			if (img->m_nobr != m_nobr)
				SetNobr(img->m_nobr);
			m_center = img->m_center;

			ImageItem *item = new ImageItem;

			item->m_size = size;
			item->m_pos = m_cursor;

			int32_t index = img->m_image;

			if (index >= 0 && index < (int32_t)m_owner->m_images.size())
				item->m_url = m_owner->m_images[index].m_url;
			item->m_link = img->m_link;
			++m_it;
			Place(item, false);
			break;
		}
		default:
			++m_it;
			break;
		}
	}
}

void RichText::LineBuilder::Start(ElementQueue::iterator it, ElementQueue::iterator end,
				  LineGroup *group, float width)
{
	if (m_state != STATE_IDLE) {
		/* taken up when the run it is on is done */
		m_next_it = it;
		m_next_width = width;
		m_next_end = end;
		m_next_group = group;
		return;
	}
	m_it = it;
	m_width = width;
	m_end = end;
	m_group = group;
	DeleteItems(group->m_items);
	m_trim = false;
	m_group->m_width = 0.0f;
	m_group->m_height = 0.0f;
	m_new_line = true;
	m_text.clear();
	m_nobr = 0;
	m_nobr_rewind = true;
	m_state = STATE_RUNNING;
	m_cursor = math::v2(0.0f, 0.0f);
	if (m_text_widget == NULL) {
		m_text_widget = new Text(m_owner, NULL);
		if (m_text_widget == NULL)
			return;
		m_text_widget->SetLayoutAttribute(graph::TextLayoutAttribute_LayoutOnly, true);
		scePafWidgetSetFontSize(m_text_widget, m_line_height,
					graph::TextStyleAttribute_Point, 0, 0);
		m_text_widget->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	}
	m_text_widget->AddEventListener(0x100004, new EventCBListener(CBMeasure, this), true);
	Run();
}

void RichText::Layouter::LayoutTables()
{
	for (list<Table>::iterator t = m_tables.begin(); t != m_tables.end(); ++t) {
		vector<Cell>::iterator c;

		/* not before every cell is measured */
		for (c = t->m_cells.begin(); c != t->m_cells.end(); ++c)
			if (c->m_state == Cell::STATE_PENDING && !c->m_group.m_done)
				break;
		if (c != t->m_cells.end())
			continue;

		/* the columns' left edges and the rows' tops */
		vector<float> cols(t->m_cols + 1);
		vector<float> rows(t->m_rows + 1);

		for (vector<float>::iterator it = cols.begin(); it != cols.end(); ++it)
			*it = 0.0f;
		for (vector<float>::iterator it = rows.begin(); it != rows.end(); ++it)
			*it = 0.0f;
		for (c = t->m_cells.begin(); c != t->m_cells.end(); ++c) {
			if (c->m_state == Cell::STATE_LAST)
				continue;

			float width = sce_paf_ceilf(max(c->m_group.m_width, c->m_width));
			uint32_t end = c->m_col + c->m_colspan;
			float wider = cols[c->m_col] + width - cols[end];

			if (wider > 0.0f)
				for (uint32_t i = end; i < t->m_cols + 1; ++i)
					cols[i] += wider;

			float taller = rows[c->m_row] + c->m_group.m_height - rows[c->m_row + 1];

			if (taller > 0.0f)
				for (uint32_t i = c->m_row + 1; i < t->m_rows + 1; ++i)
					rows[i] += taller;
		}

		/* the last cell of each row gets what the others leave, and is laid out again */
		bool again = false;

		for (c = t->m_cells.begin(); c != t->m_cells.end(); ++c) {
			if (c->m_state == Cell::STATE_LAST) {
				again = true;
				c->m_avail = m_width - cols[c->m_col];
				c->m_state = Cell::STATE_PENDING;
				m_pending.push_back(&*c);
			}
		}
		if (again)
			continue;
		for (c = t->m_cells.begin(); c != t->m_cells.end(); ++c) {
			while (c->m_group.m_items.size() != 0) {
				Item *item = c->m_group.m_items.front();

				item->m_pos = item->m_pos + math::v2(cols[c->m_col], -rows[c->m_row]);
				item->m_top -= rows[c->m_row];
				item->m_bottom -= rows[c->m_row];
				t->m_group->m_items.splice(t->m_group->m_items.end(),
							   c->m_group.m_items,
							   c->m_group.m_items.begin());
			}
		}

		LineGroup *group = t->m_group;

		group->m_width = 0.0f;
		group->m_height = 0.0f;
		for (vector<float>::iterator it = cols.begin(); it != cols.end(); ++it)
			group->m_width = max(group->m_width, *it);
		for (vector<float>::iterator it = rows.begin(); it != rows.end(); ++it)
			group->m_height = max(group->m_height, *it);
		m_tables.erase(t);
		break;
	}
}

void RichText::Layouter::Run()
{
	for (vector<LineBuilder>::iterator b = m_builders.begin(); b != m_builders.end(); ++b)
		if (b->m_state == LineBuilder::STATE_RUNNING)
			b->Run();
	if (m_tables.size() == 0 && m_it == m_queue->end()) {
		/* done once every builder is: the groups' items moved below each other */
		for (vector<LineBuilder>::iterator b = m_builders.begin(); b != m_builders.end();
		     ++b)
			if (b->m_state != LineBuilder::STATE_IDLE)
				return;

		float y = m_top;
		int32_t index = 0;

		for (list<LineGroup>::iterator g = m_groups.begin(); g != m_groups.end(); ++g) {
			while (g->m_items.size() != 0) {
				Item *item = g->m_items.front();

				item->m_index = index++;
				item->m_pos = item->m_pos - math::v2(0.0f, y);
				item->m_top -= y;
				item->m_bottom -= y;
				m_out->splice(m_out->end(), g->m_items, g->m_items.begin());
			}
			y += g->m_height;
		}
		m_state = STATE_DONE;
		m_height = y + m_bottom;
		return;
	}
	if (m_tables.size() != 0)
		LayoutTables();

	/* each idle builder given a cell to measure, or the next run of lines */
	ElementQueue::iterator start = m_it;

	for (int32_t pass = 0; pass < 2; ++pass) {
		for (vector<LineBuilder>::iterator b = m_builders.begin(); b != m_builders.end();
		     ++b) {
			if (b->m_next_group != NULL)
				continue;
			if (m_pending.size() != 0) {
				Cell *cell = m_pending.front();

				m_pending.pop_front();
				b->Start(cell->m_begin, cell->m_end, &cell->m_group, cell->m_avail);
				continue;
			}

			ElementQueue::iterator end = m_queue->end();
			bool table = false;

			while (m_it != end && (*m_it)->m_type == Element::TYPE_BR)
				++m_it;
			for (; m_it != end; ++m_it) {
				int32_t type = (*m_it)->m_type;

				if (type == Element::TYPE_BR)
					break;
				if (type == Element::TYPE_TABLE) {
					table = true;
					break;
				}
			}
			while (m_it != end && (*m_it)->m_type == Element::TYPE_BR)
				++m_it;
			if (start != m_it) {
				m_groups.push_back(LineGroup());
				b->Start(start, m_it, &m_groups.back(), m_width);
				start = m_it;
			}
			if (table) {
				ParseTable();
				start = m_it;
			}
			if (m_it == m_queue->end())
				return;
		}
	}
}

/* where it is in its parent and its size, as a rich text keeps it after its layout */
static inline void SetLayoutRect(Widget *widget, math::rect rect)
{
	if (widget->m_layout_rect == NULL)
		widget->m_layout_rect = new math::rect;
	*widget->m_layout_rect = rect;
}

void RichText::Update()
{
	if (m_hold_count > 0) {
		Widget::Update();
		return;
	}
	if (m_layouter.m_state == Layouter::STATE_RUNNING)
		m_layouter.Run();
	if (!m_layout_done && m_layouter.m_state == Layouter::STATE_DONE) {
		m_layout_done = true;
		m_content_height = m_layouter.m_height;
		UpdateViewBox();
		BuildItems();

		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(CB_RICH_TEXT_FINISH_LAYOUT, &e);

		math::v4 pos = GetPosCenter(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 anchor = GetAnchorOffset(0);
		math::v2 center =
			math::v2(pos.getImpl().getXY()) - math::v2(anchor.getImpl().getXY());
		math::v2 size = math::v2(GetSize(0)->getImpl().getXY()) + math::v2(1.0f, 0.0f);

		math::rect rect(center, size);

		SetLayoutRect(this, rect);
	}
	Widget::Update();
}

void RichText::UpdateSize()
{
	ScrollViewBase::UpdateSize();
	UpdateViewBox();
}

void RichText::OnPartsLayoutUpdated(bool is_dirty)
{
	if (is_dirty) {
		if (m_items.size() == 0)
			return;

		/* the visible top kept within the view's scroll */
		float limit = -GetViewFocus().extract_y();
		float top = 0.0f;
		float range = m_content_height - GetSize(0)->extract_y();
		float old = m_visible_top;

		if (range > 0.0f)
			top = range;
		if (limit < 0.0f)
			limit = 0.0f;
		if (top > limit)
			top = limit;
		m_visible_top = top;
		if (old < top)
			scroll_up(-top);
		else if (old > top)
			scroll_down(-top);
	}
	ScrollViewBase::OnPartsLayoutUpdated(is_dirty);
}

/* whether the two are equal, compared lane by lane */
static inline bool IsSame(const math::v2 &a, const math::v2 &b)
{
	return ((a.getImpl().getX() == b.getImpl().getX()) &
		(a.getImpl().getY() == b.getImpl().getY()))
		.getAsBool();
}

/* the widget the focus may go to: one that takes events, is focusable and is shown */
static inline bool CanFocus(Widget *widget)
{
	return widget->m_hold_count <= 0 && (widget->m_event_enable & 0x400) != 0 &&
	       widget->IsTextEventEnabled() && ScePafWidget_DB0FFD33(widget);
}

/* the widget the focuser's hint names for the direction, where the focus may go to it */
static inline Widget *GetHintFocus(Widget *widget, int32_t dir)
{
	Focuser *focuser = widget->m_focuser;

	if (focuser == NULL)
		return NULL;

	Widget *hint = focuser->GetHintWidget(dir);

	if (hint == NULL || !CanFocus(hint))
		return NULL;
	return hint;
}

bool RichText::ScePafWidget_AD706F2A(const math::v2 &dir, Widget *from)
{
	if (m_scroll_mode == 1)
		return ScrollViewBase::ScePafWidget_AD706F2A(math::v2(dir), from);

	bool moved = false;

	if (!IsSame(m_focus_target, math::v2(0.0f, 0.0f))) {
		bool turned = !IsSame(m_focus_target, dir);
		ScrollViewTimer *timer = m_focus_timer;

		/* the move the same way is still under way */
		if (timer != NULL && timer->m_type == ScrollViewTimer::TIMER_TYPE_MOVE && !turned &&
		    timer->m_mapped_x < 0.5f)
			return false;
		if (m_env->m_focus_cursor.m_target == this)
			from = GetRootFocus(this);
	}
	if (m_focus_root != NULL)
		m_focus_root->m_focus_moved = 0;

	/* to the widget the focused one's hint names, else the view scrolled */
	Widget *focus = m_env->m_focus_cursor.m_target;
	Widget *hint = NULL;

	if (focus != NULL) {
		int32_t focus_dir = GetFocusDir(dir);
		Widget *w;

		if (focus_dir == 1 && (w = GetHintFocus(focus, 0)) != NULL)
			hint = w;
		else if (focus_dir == 2 && (w = GetHintFocus(focus, 1)) != NULL)
			hint = w;
		else if (focus_dir == 3 && (w = GetHintFocus(focus, 2)) != NULL)
			hint = w;
		else if (focus_dir == 4 && (w = GetHintFocus(focus, 3)) != NULL)
			hint = w;
		else
			hint = NULL;
	}
	if (hint != this && hint != NULL) {
		SetFocusInfo info;

		info.m_mode = 0;
		info.m_notify = true;
		info.m_direction = dir;
		info.m_keep = false;
		hint->SetFocus(&info);
		moved = true;
		m_focus_target = dir;
	} else if (ScrollByKey(math::v2(dir))) {
		moved = true;
	}
	m_focus_start = m_focus_target;
	m_focus_target = math::v2(0.0f, 0.0f);
	if (!moved) {
		moved = ScrollParentByKey(math::v2(dir));
		if (!moved && m_scroll_mode == 3) {
			/* left and right scroll by a step */
			m_focus_start = math::v2(0.0f, 0.0f);
			if (IsSame(dir, math::v2(1.0f, 0.0f)))
				moved = ScrollByKey(math::v2(0.0f, -1.0f));
			else if (IsSame(dir, math::v2(-1.0f, 0.0f)))
				moved = ScrollByKey(math::v2(0.0f, 1.0f));
		}
	}
	return moved;
}

/* the number the digits at the start of the string give, and where they end */
static inline const char *ReadNumber(const string &str, uint32_t &value)
{
	const char *p = str.c_str();
	const char *end = p + str.size();

	value = 0;
	for (; p != end; ++p) {
		int32_t c = *p;

		if (c < '0' || c > '9')
			break;
		value = c - '0' + value * 10;
	}
	return p;
}

void HtmlParser::OnA(const string &tag, map<string, string> &attrs)
{
	if (m_no_link)
		return;
	FlushText();
	PushStyle(tag);

	map<string, string>::iterator it = attrs.find(string("href"));

	if (it != attrs.end()) {
		m_links.push_back(it->second);
		m_style.m_link = m_links.size() - 1;
	}
}

void HtmlParser::OnAEnd(const string &tag)
{
	if (m_no_link)
		return;
	FlushText();
	PopStyle(tag);
}

void HtmlParser::OnB(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);
	m_style.m_bold = true;
}

void HtmlParser::OnBEnd(const string &tag)
{
	FlushText();
	PopStyle(tag);
}

void HtmlParser::OnBr(const string &tag, map<string, string> &attrs)
{
	FlushText();
	Break();
}

void HtmlParser::OnCenter(const string &tag, map<string, string> &attrs)
{
	if (m_text.m_len != 0) {
		FlushText();
		Break();
	}
	PushStyle(tag);
	m_style.m_center = true;
}

void HtmlParser::OnCenterEnd(const string &tag)
{
	FlushText();
	PopStyle(tag);
	Break();
}

/* the size of font size= or h1 to h6, from 1 to 7 */
static inline int32_t ClampSize(int32_t size)
{
	int32_t value = min(size, 7);

	return value > 1 ? value : 1;
}

/* the RGB colour as the style's RGBA bytes */
static inline uint32_t ColorFromRGB(uint32_t rgb)
{
	return (rgb & 0xFF00) | ((rgb >> 16) & 0xFF) | ((rgb & 0xFF) << 16) | 0xFF000000;
}

void HtmlParser::OnFont(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);

	map<string, string>::iterator it = attrs.find(string("color"));

	if (it != attrs.end()) {
		if (it->second.size() == 7 && it->second.c_str()[0] == '#') {
			const char *p = it->second.c_str() + 1;
			const char *end = it->second.c_str() + 7;
			uint32_t rgb = 0;

			for (; p != end; ++p) {
				int32_t c = *p;
				uint32_t digit;

				if (c >= '0' && c <= '9')
					digit = c - '0';
				else if (c >= 'A' && c <= 'F')
					digit = c - 'A' + 10;
				else if (c >= 'a' && c <= 'f')
					digit = c - 'a' + 10;
				else
					break;
				rgb = digit + rgb * 16;
			}
			m_style.m_color = ColorFromRGB(rgb);
		} else {
			string name(it->second);

			char *c = (char *)name.c_str();
			char *end = c + name.size();

			for (; c != end; ++c)
				*c = ToLower(*c);

			map<string, uint32_t>::iterator color = m_colors.find(name);

			if (color != m_colors.end())
				m_style.m_color = ColorFromRGB(color->second);
		}
	}
	it = attrs.find(string("size"));
	if (it != attrs.end()) {
		uint32_t size;
		const char *p = ReadNumber(it->second, size);

		if (p != it->second.c_str())
			m_style.m_font_size = m_font_size[ClampSize(size)] * m_scale;
	}
}

void HtmlParser::OnFontEnd(const string &tag)
{
	FlushText();
	PopStyle(tag);
}

void HtmlParser::OnH(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);

	const char *p = tag.c_str() + 1;
	float scale = m_scale;
	RichText::ElementQueue::Block *head = m_queue.head;
	uint32_t level = 0;

	for (; p != tag.c_str() + tag.size(); ++p) {
		int32_t c = *p;

		if (c < '0' || c > '9')
			break;
		level = c - '0' + level * 10;
	}
	m_style.m_font_size = m_font_size[ClampSize(7 - level)] * scale;
	if (head != NULL)
		Break();
}

void HtmlParser::OnHEnd(const string &tag)
{
	FlushText();
	PopStyle(tag);
	Break();
}

void HtmlParser::OnHead(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);
	++m_hidden;
}

void HtmlParser::OnHeadEnd(const string &tag)
{
	if (PopStyle(tag))
		--m_hidden;
}

/* the width or height of the attribute: in pixels (at most 2048) or a percentage of the full */
static inline bool ReadLength(const string &str, float scale, float full, float &length)
{
	uint32_t value;
	const char *p = ReadNumber(str, value);

	if (p == str.c_str())
		return false;
	if (p == str.c_str() + str.size() || *p != '%') {
		length = (float)value * scale;
		if (length > 2048.0f)
			length = 2048.0f;
	} else {
		if (value > 100)
			value = 100;
		length = full * ((float)value / 100.0f);
	}
	return true;
}

void HtmlParser::OnImg(const string &tag, map<string, string> &attrs)
{
	FlushText();

	math::v2 size(0.0f, 0.0f);
	map<string, string>::iterator it = attrs.find(string("width"));
	float length;

	if (it != attrs.end() && ReadLength(it->second, m_scale, m_size.extract_x(), length))
		size.set_x(length);
	it = attrs.find(string("height"));
	if (it != attrs.end() && ReadLength(it->second, m_scale, m_size.extract_y(), length))
		size.set_y(length);
	it = attrs.find(string("src"));
	if (it == attrs.end())
		return;

	RichText::ImgElement *img = new RichText::ImgElement;

	if (img == NULL)
		return;
	m_images.push_back(ImageEntry(it->second, size));
	img->m_image = m_images.size() - 1;
	img->m_indent = m_font_size[0] * (float)m_list_depth * m_scale;
	img->m_nobr = m_style.m_nobr;
	img->m_link = m_style.m_link;
	img->m_center = m_style.m_center;
	if (!m_no_image_size)
		img->m_size = size;
	PushElement(img);
}

/* an unordered list item's mark: a middle dot and a space */
static const wchar_t s_bullet[] = L"\x30FB ";

void HtmlParser::OnLi(const string &tag, map<string, string> &attrs)
{
	if (m_text.m_len != 0) {
		FlushText();
		Break();
	}

	int32_t index = m_style.m_list_index++;
	const wchar_t *mark = s_bullet;
	char type = m_style.m_list_type;
	wchar_t buf[16];

	if (type == 'a' || type == 'A') {
		if (index <= 0 || index > 26)
			goto number;
		buf[0] = type + index - 1;
		sce_paf_wcsncpy(&buf[1], L". ", 3);
		mark = buf;
	}
	if (type == 'i' || type == 'I' || type == '1') {
number:
		mark = buf;
		sce_paf_swprintf(buf, 16, L"%d. ", index);
	}
	PushStyle(tag);

	RichText::TextElement *e = MakeTextElement();

	e->m_text = CopyText(mark, sce_paf_wcslen(mark));
	e->m_bold = false;
	e->m_mark = true;
	PushElement(e);
}

void HtmlParser::OnLiEnd(const string &tag)
{
	FlushText();
	PopStyle(tag);
	Break();
}

void HtmlParser::OnNobr(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);
	m_style.m_nobr = ++m_nobr_count;
}

void HtmlParser::OnNobrEnd(const string &tag)
{
	FlushText();
	PopStyle(tag);
}

void HtmlParser::OnOl(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);
	Break();

	char type = 0;

	if (!m_no_list_type) {
		type = '1';

		map<string, string>::iterator it = attrs.find(string("type"));

		if (it != attrs.end() && it->second.size() != 0)
			type = it->second.c_str()[0];
	}
	++m_list_depth;
	m_style.m_list_type = type;
	m_style.m_list_index = 1;
	m_style.m_indent = m_font_size[0] * (float)m_list_depth * m_scale;
}

void HtmlParser::OnOlEnd(const string &tag)
{
	FlushText();
	if (PopStyle(tag) && m_list_depth != 0) {
		--m_list_depth;
		m_style.m_indent = m_font_size[0] * (float)m_list_depth * m_scale;
	}
	Break();
}

void HtmlParser::OnP(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);
	if (m_queue.head != NULL)
		Break();
}

void HtmlParser::OnPEnd(const string &tag)
{
	FlushText();
	PopStyle(tag);
	Break();
}

void HtmlParser::OnTable(const string &tag, map<string, string> &attrs)
{
	FlushText();
	if (m_table != NULL)
		return;

	RichText::TableElement *table = new RichText::TableElement;

	if (PushElement(table)) {
		table->m_cols = 0;
		m_table = table;
		m_row = 0;
		m_col = 0;
	}
}

void HtmlParser::OnTableEnd(const string &tag)
{
	FlushText();
	if (m_table == NULL)
		return;
	m_table->m_rows = m_row;
	m_table = NULL;
	PushElement(new RichText::TableEndElement);
}

void HtmlParser::OnTd(const string &tag, map<string, string> &attrs)
{
	FlushText();
	if (m_table == NULL)
		return;
	m_space = false;
	m_newline = false;
	m_last = 0;

	float width = 0.0f;
	map<string, string>::iterator it = attrs.find(string("width"));

	if (it != attrs.end())
		ReadLength(it->second, m_scale, m_size.extract_x(), width);

	uint32_t colspan = 1;

	it = attrs.find(string("colspan"));
	if (it != attrs.end()) {
		uint32_t value;
		const char *p = ReadNumber(it->second, value);

		if (p != it->second.c_str() && value > 1 && m_col + value <= 16)
			colspan = value;
	}

	RichText::CellElement *cell = new RichText::CellElement;

	cell->m_row = m_row;
	cell->m_col = m_col;
	cell->m_width = width;
	cell->m_height = 0.0f;
	cell->m_colspan = colspan;
	PushElement(cell);
	m_col += colspan;
}

void HtmlParser::OnTdEnd(const string &tag)
{
	FlushText();
	if (m_table != NULL)
		PushElement(new RichText::CellEndElement);
}

void HtmlParser::OnTr(const string &tag, map<string, string> &attrs)
{
	FlushText();
}

void HtmlParser::OnTrEnd(const string &tag)
{
	FlushText();
	if (m_table == NULL)
		return;
	m_table->m_cols = max(m_table->m_cols, m_col);
	m_col = 0;
	++m_row;
}

void HtmlParser::OnUl(const string &tag, map<string, string> &attrs)
{
	FlushText();
	PushStyle(tag);
	Break();
	++m_list_depth;
	m_style.m_list_type = 0;
	m_style.m_indent = m_font_size[0] * (float)m_list_depth * m_scale;
}

void HtmlParser::OnUlEnd(const string &tag)
{
	FlushText();
	if (PopStyle(tag) && m_list_depth != 0) {
		--m_list_depth;
		m_style.m_indent = m_font_size[0] * (float)m_list_depth * m_scale;
	}
	Break();
}

/* the line the text widget measured made into items, a line at a time, until the text is
   used up or a nobr block goes back */
void RichText::LineBuilder::CBMeasure(int32_t type, Handler *self, Event *e, void *data)
{
	LineBuilder *builder = (LineBuilder *)data;

	if (builder->m_state != STATE_MEASURING)
		return;

	TextElement *text = (TextElement *)*builder->m_it;
	int32_t link = -1;

	if (text->m_type == Element::TYPE_LINK_TEXT)
		link = ((LinkTextElement *)text)->m_link;

	graph::TextLayoutLineInfo info;

	builder->m_text_widget->GetLineInfo(0, info);
	if (info.len == 0)
		return;

	uint32_t count;

	builder->m_text_widget->GetLineCount(count);
	if (count > 1 && !builder->m_new_line) {
		/* a word broken at the end of a line goes to the next line whole */
		wchar_t ch = builder->m_text[info.len - 1];

		if (ch <= 0xFF || (ch >= 0x400 && ch <= 0x4FF)) {
			const wchar_t *p;

			for (p = s_break_chars; *p != L'\0'; ++p)
				if (*p == ch)
					break;
			if (*p == L'\0') {
				builder->EndLine();
				builder->Run();
				return;
			}
		}
	}

	float x = builder->m_cursor.extract_x();

	for (uint32_t line = 0; line < count; ++line) {
		if (line != 0)
			builder->m_text_widget->GetLineInfo(line, info);

		TextItem *item;

		if (link >= 0) {
			LinkTextItem *link_item = new LinkTextItem;

			link_item->m_link = link;
			item = link_item;
		} else {
			item = new TextItem;
		}
		item->m_pos = builder->m_cursor;
		item->m_size = math::v2(info.frame.extract_z(), info.frame.extract_w());
		item->m_bold = text->m_bold;
		item->m_color = text->m_color;
		item->m_font_size = text->m_font_size;
		item->m_ascent = info.ascender;
		item->m_text = CopyText(builder->m_text.c_str(), info.len);
		if (!builder->Place(item, text->m_mark))
			break;
		builder->m_text = builder->m_text.substr(info.len, wstring::npos);
		if (builder->m_text.size() == 0) {
			++builder->m_it;
			break;
		}
		if (!builder->m_new_line)
			builder->EndLine();
		builder->m_cursor.set_x(text->m_indent);
		if (builder->m_cursor.extract_x() != x)
			break;
		builder->m_trim = false;
	}
	builder->Run();
}

int32_t TextureListener::Do(int32_t type, Handler *self, Event *e)
{
	((Widget *)self)->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	return 0;
}

int32_t TextureReadyListener::Do(int32_t type, Handler *self, Event *e)
{
	RichText *owner = m_owner;

	((Widget *)self)->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	for (uint32_t i = 0; i < owner->m_items.size(); ++i) {
		if (owner->m_items[i]->m_widget == self) {
			e->m_data[1] = i;
			owner->DoEvent(type, e);
			break;
		}
	}
	return 0;
}

int32_t LinkListener::Do(int32_t type, Handler *self, Event *e)
{
	int32_t index = m_link;
	RichText *owner = m_owner;
	Event link(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	link.m_data[0] = index;
	owner->DoEvent(RichText::CB_RICH_TEXT_LINK, &link);
	return 0;
}

static Widget *NewRichText(Widget *parent)
{
	return new RichText(parent, NULL);
}

static WidgetParam *NewRichTextParam()
{
	return new ScrollViewBaseParam;
}

static VersatileParam *NewRichTextLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewRichTextFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewRichTextStyleParam()
{
	return new ScrollViewBaseStyleParam;
}

static VersatileParam *NewRichTextHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewRichTextSoundParam()
{
	return new ScrollViewBaseSoundParam;
}

static void *Alloc(size_t size)
{
	return sce_paf_malloc(size);
}

static void Free(void *ptr)
{
	sce_paf_free(ptr);
}

static char ToLower(char ch)
{
	return (char)sce_paf_tolower(ch);
}

}
}

using namespace paf;
using namespace paf::ui;

FactoryFuncTable g_factoryRichText = {
	RichText::TypeName(),
	NewRichText,
	NewRichTextParam,
	NewRichTextLayoutParam,
	NewRichTextFocusParam,
	NewRichTextStyleParam,
	NewRichTextHandlerParam,
	NewRichTextSoundParam,
};
