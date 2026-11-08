/*
 * paf::ui::RichText: a scroll view that lays out a marked-up text (a small set of HTML tags:
 * a, b, strong, br, center, font, h1 to h6, head, img, li, nobr, ol, p, table, td, tr, ul, and
 * the PS3_ND_start comments) or a plain text, as rows of text, link and image widgets it makes
 * only while they are in view. Its parser turns the text into elements, its layouter the
 * elements into items, a few lines a frame, and each item gets its widget as it scrolls in.
 * The class and its exported functions are Sony's (export NIDs, ScePaf-RE's header after
 * them); the helper classes, the other functions and the members are named by us.
 */
#ifndef PAF_WIDGET_W_RICH_TEXT_H
#define PAF_WIDGET_W_RICH_TEXT_H

#include <stddef.h>
#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/string>
#include <std/vector>
#include <widget/w_scroll_view_base.h>

namespace paf {
namespace ui {

class Text;

class RichText : public ScrollViewBase {
public:
	/* what it sends its listeners: each image's size wanted (m_data[0] the image), the
	   layout finished, an item's widget made (m_data[0] the item) and a link decided
	   (m_data[0] the link; names ours) */
	enum {
		CB_RICH_TEXT_REQ_IMG_SIZE = 0x10000007,
		CB_RICH_TEXT_FINISH_LAYOUT = 0x10000008,
		CB_RICH_TEXT_SCROLL_IN = 0x10000009,
		CB_RICH_TEXT_LINK = 0x1000000B
	};

	/* the item types GetItemType gives: a link text counts as a text */
	enum {
		ITEM_TYPE_TEXT = 1,
		ITEM_TYPE_IMAGE = 2,
		ITEM_TYPE_LINK_TEXT = 3
	};

	/* what the parser makes of the text */
	class Element {
	public:
		enum {
			TYPE_TEXT = 0,
			TYPE_LINK_TEXT = 1,
			TYPE_BR = 2,
			TYPE_IMG = 3,
			TYPE_TABLE = 4,
			TYPE_TABLE_END = 5,
			TYPE_CELL = 6,
			TYPE_CELL_END = 7
		};

		Element(int32_t type) : m_type(type)
		{
		}

		virtual ~Element()
		{
		}

		int32_t m_type;
	};

	/* a run of text in one style */
	class TextElement : public Element {
	public:
		TextElement() : Element(TYPE_TEXT)
		{
		}

		TextElement(int32_t type) : Element(type)
		{
		}

		virtual ~TextElement()
		{
			delete[] m_text;
		}

		wchar_t *m_text;
		bool m_bold;
		/* RGBA, one byte each */
		uint32_t m_color;
		float m_font_size;
		/* how far its lines start from the left */
		float m_indent;
		/* a list item's mark: laid out before the indent */
		bool m_mark;
		/* the nobr block it is in, 0 for none */
		int32_t m_nobr;
		bool m_center : 1;
	};

	class LinkTextElement : public TextElement {
	public:
		LinkTextElement() : TextElement(TYPE_LINK_TEXT)
		{
		}

		/* the index of its link in the links, -1 for none */
		int32_t m_link;
	};

	class BrElement : public Element {
	public:
		BrElement() : Element(TYPE_BR)
		{
		}
	};

	class ImgElement : public Element {
	public:
		ImgElement() : Element(TYPE_IMG), m_size(0.0f, 0.0f)
		{
		}

		/* the index of its image in the images */
		int32_t m_image;
		math::v2 m_size;
		/* nothing reads or writes it */
		int32_t m_reserved;
		float m_indent;
		int32_t m_nobr;
		int32_t m_link;
		bool m_center : 1;
	};

	class TableElement : public Element {
	public:
		TableElement() : Element(TYPE_TABLE)
		{
		}

		uint32_t m_rows;
		uint32_t m_cols;
	};

	class TableEndElement : public Element {
	public:
		TableEndElement() : Element(TYPE_TABLE_END)
		{
		}
	};

	class CellElement : public Element {
	public:
		CellElement() : Element(TYPE_CELL)
		{
		}

		uint32_t m_row;
		uint32_t m_col;
		float m_width;
		float m_height;
		uint32_t m_colspan;
	};

	class CellEndElement : public Element {
	public:
		CellEndElement() : Element(TYPE_CELL_END)
		{
		}
	};

	/* the elements, in blocks of 256 */
	class ElementQueue {
	public:
		enum {
			BLOCK_SIZE = 256
		};

		class Block {
		public:
			Block *prev;
			Block *next;
			Element *elements[BLOCK_SIZE];
		};

		class iterator {
		public:
			Element *&operator*() const
			{
				return block->elements[index];
			}

			/* stays at the end of the last block */
			iterator &operator++()
			{
				if (++index >= BLOCK_SIZE && block->next != NULL) {
					block = block->next;
					index = 0;
				}
				return *this;
			}

			bool operator==(const iterator &other) const
			{
				return block == other.block && index == other.index;
			}

			bool operator!=(const iterator &other) const
			{
				return block != other.block || index != other.index;
			}

			Block *block;
			uint32_t index;
		};

		ElementQueue() : head(NULL), tail(NULL), tail_count(0)
		{
		}

		~ElementQueue()
		{
			Free();
		}

		iterator begin() const
		{
			iterator it;

			it.block = head;
			it.index = 0;
			return it;
		}

		iterator end() const
		{
			iterator it;

			it.block = tail;
			it.index = tail_count;
			return it;
		}

		void push_back(Element *e)
		{
			if (head == NULL || tail_count == BLOCK_SIZE) {
				Block *block = new Block;

				block->prev = tail;
				block->next = NULL;
				if (head != NULL)
					tail->next = block;
				else
					head = block;
				tail = block;
				tail_count = 0;
			}
			tail->elements[tail_count++] = e;
		}

		/* its blocks freed, the elements left */
		void Free()
		{
			for (Block *block = tail; block != NULL; block = tail) {
				tail = block->prev;
				delete block;
			}
			head = NULL;
			tail = NULL;
			tail_count = 0;
		}

		/* the other's elements taken, its own blocks freed */
		void Take(ElementQueue &other)
		{
			Free();
			head = other.head;
			tail = other.tail;
			tail_count = other.tail_count;
			other.head = NULL;
			other.tail = NULL;
			other.tail_count = 0;
		}

		/* its elements deleted and its blocks freed */
		__attribute__((noinline)) void Clear()
		{
			for (iterator it = begin(); it != end(); ++it)
				delete *it;
			Free();
		}

		Block *head;
		Block *tail;
		uint32_t tail_count;
	};

	/* what the layouter makes of the elements: a widget's place */
	class Item {
	public:
		Item(int32_t type) : m_type(type)
		{
		}

		virtual ~Item()
		{
		}

		int32_t m_type;
		/* its index in the items */
		int32_t m_index;
		/* its widget, while it is in view */
		Widget *m_widget;
		float m_top;
		float m_bottom;
		math::v2 m_pos;
		math::v2 m_size;
	};

	class TextItem : public Item {
	public:
		TextItem() : Item(ITEM_TYPE_TEXT)
		{
		}

		TextItem(int32_t type) : Item(type)
		{
		}

		virtual ~TextItem()
		{
			delete[] m_text;
		}

		wchar_t *m_text;
		bool m_bold;
		uint32_t m_color;
		float m_font_size;
		/* its first line's ascent */
		float m_ascent;
	};

	class LinkTextItem : public TextItem {
	public:
		LinkTextItem() : TextItem(ITEM_TYPE_LINK_TEXT)
		{
		}

		int32_t m_link;
	};

	class ImageItem : public Item {
	public:
		ImageItem() : Item(ITEM_TYPE_IMAGE)
		{
		}

		string m_url;
		int32_t m_link;
	};

	/* the items of a line, or of a table's cell, and how far they reach */
	class LineGroup {
	public:
		LineGroup() : m_done(false)
		{
		}

		bool m_done;
		list<Item *> m_items;
		float m_width;
		float m_height;
	};

	/* a table's cell, laid out on its own */
	class Cell {
	public:
		enum {
			STATE_PENDING = 1,
			/* the last of its row: its width is what the others leave */
			STATE_LAST = 2
		};

		int32_t m_state;
		uint32_t m_row;
		uint32_t m_col;
		float m_width;
		float m_height;
		uint32_t m_colspan;
		ElementQueue::iterator m_begin;
		ElementQueue::iterator m_end;
		LineGroup m_group;
		float m_avail;
	};

	class Table {
	public:
		uint32_t m_rows;
		uint32_t m_cols;
		vector<Cell> m_cells;
		LineGroup *m_group;
	};

	/* lays out a run of elements into a line group, a line at a time: its text widget
	   measures the text and says where the lines break */
	class LineBuilder {
	public:
		enum {
			STATE_IDLE = 1,
			STATE_RUNNING = 2,
			STATE_MEASURING = 3
		};

		LineBuilder()
			: m_state(0), m_group(NULL), m_line_start(NULL), m_text_widget(NULL),
			  m_next_group(NULL), m_nobr_line_start(NULL), m_line_spacing(1.0f)
		{
		}

		/* the run from it to end laid out into the group, the width wide */
		void Start(ElementQueue::iterator it, ElementQueue::iterator end, LineGroup *group,
			   float width);
		void Run();
		/* the items from first to last moved down from the line at y: the texts to the
		   line's height, the others to its ascent; the height and the highest text's */
		void AlignLine(list<Item *>::iterator first, list<Item *>::iterator last, float y,
			       float &height, float &text_height);
		/* the items from first to last moved right by half of what the line leaves */
		void Center(list<Item *>::iterator first, list<Item *>::iterator last, float width,
			    float x);
		/* the line ended: its items moved up to the baseline and down the line */
		void EndLine();
		/* back to where the nobr block began */
		void Rewind();
		/* the item placed at the cursor; false where it went back to a nobr block */
		bool Place(Item *item, bool mark);
		static void CBMeasure(int32_t type, Handler *self, Event *e, void *data);

		/* the element's nobr block entered: where it began kept */
		void SetNobr(int32_t nobr)
		{
			m_nobr = nobr;
			m_nobr_rewind = true;
			if (nobr != 0) {
				m_nobr_it = m_it;
				m_nobr_cursor = m_cursor;
				m_nobr_count = m_group->m_items.size();
				m_nobr_line_start = m_line_start;
				m_nobr_new_line = m_new_line;
			}
		}

		int32_t m_state;
		RichText *m_owner;
		ElementQueue::iterator m_it;
		ElementQueue::iterator m_end;
		LineGroup *m_group;
		float m_width;
		/* the first item of the line */
		list<Item *>::iterator m_line_start;
		bool m_new_line;
		bool m_trim;
		/* the text being measured */
		wstring m_text;
		Text *m_text_widget;
		math::v2 m_cursor;
		/* the run Start gave it while it was busy */
		ElementQueue::iterator m_next_it;
		ElementQueue::iterator m_next_end;
		LineGroup *m_next_group;
		float m_next_width;
		/* the nobr block it is in, and whether it may go back to its start */
		int32_t m_nobr;
		bool m_nobr_rewind;
		bool m_center : 1;
		/* where the nobr block began */
		ElementQueue::iterator m_nobr_it;
		uint32_t m_nobr_count;
		list<Item *>::iterator m_nobr_line_start;
		bool m_nobr_new_line;
		math::v2 m_nobr_cursor;
		float m_line_height;
		float m_line_spacing;
	};

	/* lays the elements out into lines with its line builders, a pass a frame */
	class Layouter {
	public:
		enum {
			STATE_RUNNING = 1,
			STATE_DONE = 2
		};

		Layouter();
		~Layouter();

		/* its builders idle and its groups gone */
		void Reset();
		void Start(ElementQueue *queue, list<Item *> *out);
		void Run();
		/* the tables' cells measured: their columns' widths and rows' heights set */
		void LayoutTables();
		/* the table at the element */
		void ParseTable();

		int32_t m_state;
		RichText *m_owner;
		ElementQueue *m_queue;
		ElementQueue::iterator m_it;
		list<Item *> *m_out;
		float m_height;
		vector<LineBuilder> m_builders;
		list<LineGroup> m_groups;
		uint32_t m_flags;
		int32_t m_builder_count;
		float m_line_height;
		float m_line_spacing;
		float m_width;
		float m_top;
		float m_bottom;
		list<Table> m_tables;
		list<Cell *> m_pending;
	};

	/* an image of the text: requested, given a size, its url and its size in the text */
	class ImageInfo {
	public:
		enum {
			STATE_REQUESTED = 1,
			STATE_SIZED = 2
		};

		int32_t m_state;
		string m_url;
		math::v2 m_size;
		math::v2 m_html_size;
	};

	static const char m_widget_type[];
	static const char *TypeName();

	RichText(Widget *parent, CreateParam *param);
	virtual ~RichText();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateSize();
	virtual void Update();
	/* the texture of the index-th item's widget */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using ScrollViewBase::SetTexture;
	/* the index-th item's widget's, with listeners for its texture ready */
	virtual int32_t SetDrawObjMaskBit(const void *param, int32_t obj, uint32_t bit);
	using Widget::SetDrawObjMaskBit;
	virtual void SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b)
	{
		ScrollViewBase::SetViewFocus(pos, timer, flag_a, flag_b);
	}
	virtual void OnPartsLayoutUpdated(bool is_dirty);
	/* the focus moved in the direction: in scroll mode 1 as the view's, else to a link */
	virtual bool ScePafWidget_AD706F2A(const math::v2 &dir, Widget *from);

	/* its text from the UTF-8 marked-up string of the length */
	int32_t SetCText(const char *text, size_t len);
	/* all its items, elements, images and text gone */
	void Clear();
	/* the size of the index-th image of its text, and the size it is shown at */
	int32_t GetHtmlImageSize(int32_t index, math::v4 &size);
	int32_t SetImageSize(int32_t index, const math::v4 &size);
	/* the index-th item's widget, type, link URL and image URL */
	Widget *GetItemWidget(int32_t index);
	int32_t GetItemType(int32_t index) const;
	int32_t GetItemUrl(int32_t index, string &url) const;
	int32_t GetImageUrl(int32_t index, string &url) const;
	/* the font size of the size (0 the base, 1 to 7 font size= and h7 to h1) */
	int32_t ScePafWidget_6A60296B(uint32_t size, float value);
	/* the index-th link */
	int32_t ScePafWidget_C7BE2076(int32_t index, string &url) const;
	/* the items' widgets made or destroyed as the top moved down or up */
	void scroll_down(float top);
	void scroll_up(float top);
	void SetVisibleTop(float top);

	/* the view box sized to the contents */
	void UpdateViewBox();
	/* the links' focus directions set among them */
	void LinkFocus();
	/* the item's widget made */
	void CreateItemWidget(Item *item);
	int32_t StartLayout();
	int32_t Parse();
	/* the layouter's lines made the items */
	void BuildItems();

	void *(*m_alloc)(size_t size);
	void (*m_free)(void *ptr);
	char *m_str;
	char *m_str_end;
	ElementQueue m_queue;
	list<Item *> m_lines;
	vector<Item *> m_items;
	/* the items by top and by bottom, and the next of each to come into view */
	vector<Item *> m_items_top;
	vector<Item *> m_items_bottom;
	vector<Item *>::iterator m_next_bottom;
	vector<Item *>::iterator m_next_top;
	vector<string> m_links;
	/* the text is plain, not marked up */
	int32_t m_plain;
	math::v4 m_color;
	float m_scale;
	float m_font_size[8];
	/* the parser leaves out the images' sizes, the links and the list types */
	bool m_no_image_size;
	bool m_no_link;
	bool m_no_list_type;
	uint32_t m_flags;
	float m_content_height;
	float m_visible_top;
	bool m_layout_done;
	Layouter m_layouter;
	/* how the focus moves: 1 as the view's, 3 to the links or by a step */
	int32_t m_scroll_mode;
	vector<ImageInfo> m_images;
	int32_t m_builder_count;
	float m_line_spacing;
	/* the margins around its text, made 0 by its constructor (npeula sets them): above,
	   below and taken off its width */
	float m_margin_x;
	float m_margin_y;
	float m_margin_width;
	bool m_reserved_byte0;
	bool m_reserved_byte1;
};

}
}

/* how far its contents scroll (npeula negates it into a position): the view box's height */
#define m_scroll_extent m_view_box_size.extract_y()

#endif
