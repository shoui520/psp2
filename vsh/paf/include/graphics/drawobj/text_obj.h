/*
 * paf::graph::TextObj: a draw object that draws a text layout, which it shares with the text
 * threads that lay it out; nearly every method passes on to the layout, and a change the layout
 * says needs new geometry marks the object for preparing. And paf::graph::TextObjParam, its
 * draw object parameters.
 *
 * Class, method and enum names from the mangled export names (OnDraw, Draw, ReleaseAll,
 * HitTestChar and the v2 style attributes by NID) and ScePaf-RE's header; the parameters'
 * names from the names they are read under ("max_line_count", ...). SetPlaceholderStyle and
 * ReplaceString are unexported: the first named after ui::Text's (by NID), the second ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_TEXT_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_TEXT_OBJ_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_style.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/string>

namespace paf {
namespace graph {

class TextObj : public DrawObj {
public:
	static const char *DrawObjType();
	static void DrawCallback(const DrawCallbackArg &arg);

	TextObj(const DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~TextObj();
	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info);
	virtual void Draw(const math::matrix &world, uint32_t color, float alpha);
	virtual void ReleaseAll();
	virtual void SetSize(math::v3 size);
	virtual void Update();
	virtual void UpdatePost(const math::matrix &world_mat);
	virtual void CalcWorldMatrix(math::matrix &world_mat, const math::matrix &parent,
				     bool unscaled);
	virtual void SetSurfaceDraw(const SurfaceDrawParam &param);

	int32_t SetLayoutAttribute(TextLayoutAttribute attr, int32_t val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, float val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, const math::v2 &val);
	int32_t SetLayoutAttribute(TextLayoutAttribute attr, bool val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, int32_t &val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, float &val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, math::v2 &val);
	int32_t GetLayoutAttribute(TextLayoutAttribute attr, bool &val);

	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, int32_t val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, float val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				  const math::v2 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len,
				  const math::v4 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t index, uint32_t len, bool val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, int32_t &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, float &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v2 &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, math::v4 &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t index, bool &val);
	int32_t SetStyle(uint32_t index, uint32_t len, const TextStyle &style);
	int32_t GetStyle(uint32_t index, TextStyle &style);

	int32_t SetString(const wstring &str);
	int32_t ReplaceString(uint32_t index, uint32_t len, const wstring &str);
	int32_t GetString(wstring &str);
	int32_t GetLength(uint32_t &len);
	int32_t GetStringBuffer(const wchar_t *&buf, uint32_t &len);
	int32_t SetAttributedString(const AttributedString &str);
	int32_t GetAttributedString(AttributedString &str);
	int32_t SetPlaceholderStyle(const TextStyle &style);
	int32_t SetPlaceholderString(const wstring &str);
	int32_t GetPlaceholderString(wstring &str);

	int32_t GetBounds(math::v2 &bounds);
	int32_t GetLineCount(uint32_t &count);
	int32_t SetCharPos(uint32_t index, const math::v2 &pos);
	int32_t GetCharInfo(uint32_t index, TextLayoutCharInfo &info);
	int32_t GetLineInfo(uint32_t line, TextLayoutLineInfo &info);
	int32_t GetNextUpHit(uint32_t index, uint32_t &next);
	int32_t GetNextDownHit(uint32_t index, uint32_t &next);
	int32_t GetNextLeftHit(uint32_t index, uint32_t &next);
	int32_t GetNextRightHit(uint32_t index, uint32_t &next);
	int32_t HitTestChar(const math::v2 &pos, bool inside, uint32_t &index);
	int32_t MoveCaretUp();
	int32_t MoveCaretDown();
	int32_t MoveCaretLeft();
	int32_t MoveCaretRight();
	int32_t SetCaretPos(const math::v2 &pos);
	int32_t SetSelectionPos(const math::v2 &start, const math::v2 &end);
	int32_t SetCaretIndex(uint32_t index);
	int32_t GetCaretIndex(uint32_t &index);
	int32_t SetSelectionRange(uint32_t index, uint32_t len);
	int32_t GetSelectionRange(uint32_t &index, uint32_t &len);
	int32_t SetCaretVisible(bool visible);
	int32_t GetCaretVisible(bool &visible);
	int32_t SetCaretColor(const math::v4 &color);
	int32_t GetCaretColor(math::v4 &color);
	int32_t SetSelectionColor(const math::v4 &color);
	int32_t GetSelectionColor(math::v4 &color);
	int32_t Invalidate();

	static const char m_drawobj_type[];

	common::SharedPtr<TextLayout> m_layout;

private:
	/* a layout change that needs new geometry: the object is prepared again and drawn dirty
	   (name ours) */
	int32_t LayoutChanged(int32_t ret)
	{
		if (ret > 0 && (ret & 2) != 0 && !m_flags.m_prepare) {
			m_flags.m_prepare = true;
			UpdateDirty();
		}
		return ret;
	}
};

class TextObjParam : public DrawObjParam {
public:
	TextObjParam();

	static const char m_param_name[];
	static const char *ParamName();

	virtual const char *GetParamName() const;

	virtual ~TextObjParam();

	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(DrawObj *obj);
	virtual int32_t GetEach(DrawObj *obj);

	math::v3 m_pos;
	math::v3 m_size;
	int32_t m_align_x;
	int32_t m_align_y;
	int32_t m_text_align;
	int32_t m_max_line_count;
	float m_line_space;
	float m_line_pitch;
	math::v2 m_visible_pos;
	math::v2 m_visible_size;
	bool m_adjust_x;
	bool m_adjust_y;
	bool m_condense_x;
	bool m_condense_y;
	bool m_word_wrap;
	bool m_kinsoku;
	bool m_ellipsis;
	/* -1: the layout's own */
	int8_t m_enlarge_line_pitch;
	int32_t m_family;
	float m_font_size;
	math::v4 m_font_color;
	math::v4 m_shadow_color;
	math::v4 m_back_color;
	math::v4 m_underline_color;
	math::v4 m_strike_color;
	bool m_bold;
	bool m_oblique;
	bool m_underline;
	bool m_strike;
	bool m_mask;
	bool m_shadow;
	bool m_emboss;
	/* -1: the style's own */
	int8_t m_enlarge_font_size;
	int8_t m_force_bold;
};

}
}

#endif
