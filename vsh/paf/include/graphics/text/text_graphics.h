/*
 * paf::graph::TextGraphics: the glyphs a text layout draws, one character each with its glyph
 * surface and style, with the layout's box, caret and selection. The text threads add, move
 * and remove characters and set the box, the caret and the selection under the graphics
 * mutex, each change marked dirty; Update takes the changes over for drawing and Draw draws
 * them under a world matrix in a color with a shader. TextGraphicsImpl draws each as a quad
 * clipped to the box. Names ours (after Sony's ScePafTextGraphicsMutex), as are the members.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_GRAPHICS_H
#define PAF_GRAPHICS_TEXT_TEXT_GRAPHICS_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_style.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/memory>
#include <std/vector>

namespace paf {

namespace thread {
class RMutex;
}

namespace graph {

class Shader;

/* the rectangle (x, y, width, height) a and b both cover, empty where they do not meet (name
   ours) */
int32_t IntersectRect(const math::v4 &a, const math::v4 &b, math::v4 &out);

class TextGraphics {
public:
	/* a character: the box laid out for it, and its glyph drawn width wide from offset in */
	class Char {
	public:
		Char()
			: m_rect(0.0f, 0.0f, 0.0f, 0.0f), m_offset(0.0f), m_width(0.0f),
			  m_removed(false)
		{
		}

		virtual ~Char()
		{
		}

		math::v4 m_rect;
		float m_offset;
		float m_width;
		common::SharedPtr<TextStyle> m_style;
		intrusive_ptr<Surface> m_surface;
		/* taken off by the next Update */
		bool m_removed;
	};

	/* what Update has to take over */
	enum {
		Dirty_Chars = 0x01,
		Dirty_Pos = 0x02,
		Dirty_Size = 0x04,
		Dirty_Scale = 0x08,
		Dirty_CharRects = 0x10,
		Dirty_Caret = 0x20,
		Dirty_Selection = 0x40
	};

	TextGraphics(thread::RMutex *mutex);
	virtual ~TextGraphics();

	virtual int32_t Update() = 0;
	virtual int32_t Draw(const math::matrix &world, const math::v4 &color, Shader *shader) = 0;
	/* a character added, its glyph the surface's */
	virtual int32_t AddChar(const math::v4 &rect, float offset, float width,
				const common::SharedPtr<TextStyle> &style,
				const intrusive_ptr<Surface> &surface, Char *&c) = 0;
	/* a character laid out at rect again */
	virtual int32_t MoveChar(Char *c, const math::v4 &rect) = 0;

	void Unlock();
	void Lock();
	int32_t RemoveChar(Char *c);
	int32_t RemoveAllChars();
	int32_t SetPos(const math::v2 &pos);
	int32_t SetSize(const math::v2 &size);
	int32_t SetScale(const math::v2 &scale);
	/* the caret at pos, height high */
	int32_t SetCaret(const math::v2 &pos, float height);
	/* the selection's rectangles */
	int32_t SetSelection(const vector<math::v4> &rects);
	int32_t SetCaretVisible(bool visible);
	int32_t GetCaretVisible(bool &visible);
	int32_t SetCaretColor(const math::v4 &color);
	int32_t GetCaretColor(math::v4 &color);
	int32_t SetSelectionColor(const math::v4 &color);
	int32_t GetSelectionColor(math::v4 &color);

	list<Char *> m_chars;
	/* the box: its position, then its size */
	math::v4 m_rect;
	math::v2 m_scale;
	math::v2 m_caret_pos;
	float m_caret_height;
	vector<math::v4> m_selection;
	int8_t m_dirty;
	bool m_caret_visible;
	/* RGBA bytes */
	uint32_t m_caret_color;
	uint32_t m_selection_color;
	thread::RMutex *m_mutex;
};

class TextGraphicsImpl : public TextGraphics {
public:
	/* a character with where its glyph and its box are drawn, clipped to the graphics' box */
	class Char : public TextGraphics::Char {
	public:
		Char() : m_draw_glyph_rect(0.0f, 0.0f, 0.0f, 0.0f), m_draw_rect(0.0f, 0.0f, 0.0f, 0.0f)
		{
		}

		math::v4 m_draw_glyph_rect;
		math::v4 m_draw_rect;
	};

	TextGraphicsImpl(thread::RMutex *mutex);
	virtual ~TextGraphicsImpl();

	virtual int32_t Update();
	virtual int32_t Draw(const math::matrix &world, const math::v4 &color, Shader *shader);
	virtual int32_t AddChar(const math::v4 &rect, float offset, float width,
				const common::SharedPtr<TextStyle> &style,
				const intrusive_ptr<Surface> &surface, TextGraphics::Char *&c);
	virtual int32_t MoveChar(TextGraphics::Char *c, const math::v4 &rect);

private:
	/* where every character is drawn, worked out again */
	int32_t UpdateCharRects();
	/* the characters removed, the rest taken over for drawing */
	int32_t UpdateChars();

	/* what Update took over */
	list<TextGraphics::Char *> m_draw_chars;
	math::v4 m_draw_rect;
	math::v2 m_draw_scale;
	/* the caret clipped to the box */
	math::v2 m_draw_caret_pos;
	float m_draw_caret_height;
	vector<math::v4> m_draw_selection;
};

}
}

#endif
