/*
 * paf::graph::AttributedString: a wide string with its styles, as a list of style runs that
 * cover it from its start, each a TextStyle over a number of characters. Neighbouring runs of
 * the same style are merged as the styles change. Class and method names from the mangled
 * export names (the v2 attribute methods by NID) and ScePaf-RE's header (restored from Sony's
 * DWARF); the members and the helpers no export names are ours.
 */
#ifndef PAF_GRAPHICS_TEXT_ATTRIBUTED_STRING_H
#define PAF_GRAPHICS_TEXT_ATTRIBUTED_STRING_H

#include <stdint.h>
#include <graphics/text/text_style.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/string>

namespace paf {
namespace graph {

/* a style over len characters */
class TextStyleRun {
public:
	TextStyleRun() : len(0)
	{
	}

	virtual ~TextStyleRun()
	{
	}

	TextStyle style;
	uint32_t len;
};

class AttributedString {
public:
	AttributedString();
	AttributedString(const TextStyle &style);
	AttributedString(const AttributedString &str);
	virtual ~AttributedString();

	AttributedString &operator=(const AttributedString &str);

	/* the attribute of the characters from pos, len of them (both 0: all of them) */
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len, int32_t val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len, float val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len,
				  const math::v2 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len,
				  const math::v4 &val);
	int32_t SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len, bool val);
	/* the attribute of the character at pos */
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, int32_t &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, float &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, math::v2 &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, math::v4 &val);
	int32_t GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, bool &val);
	int32_t SetStyle(uint32_t pos, uint32_t len, const TextStyle &style);
	int32_t GetStyle(uint32_t pos, TextStyle &style);
	/* the whole string, the runs cut to it (the last one taking what it adds) */
	int32_t SetString(const wstring &str);
	/* len characters from pos replaced by str (name ours) */
	int32_t ReplaceString(uint32_t pos, uint32_t len, const wstring &str);
	int32_t GetString(wstring &str);
	int32_t GetLength(uint32_t &len);
	int32_t GetStringBuffer(const wchar_t *&buf, uint32_t &len);
	int32_t GetStyleRuns(_list_iterator<TextStyleRun *> &begin,
			     _list_iterator<TextStyleRun *> &end);

private:
	/* names ours */
	void ClearStyleRuns();
	void Copy(const AttributedString &str);
	/* a run of the style (the default one for none), at the end */
	void AddStyleRun(const TextStyle *style, uint32_t len)
	{
		TextStyleRun *run = new TextStyleRun();

		if (run != NULL) {
			if (style != NULL)
				run->style = *style;
			run->len = len;
			m_styleRuns.push_back(run);
		}
	}
	/* the runs over len characters from pos made to cover new_len of them */
	int32_t ResizeStyleRuns(uint32_t pos, uint32_t len, uint32_t new_len);
	template<class T>
	int32_t SetStyleAttributeT(TextStyleAttribute attr, uint32_t pos, uint32_t len,
				   const T &val);
	template<class T>
	int32_t GetStyleAttributeT(TextStyleAttribute attr, uint32_t pos, T &val);

	wstring m_str;
	list<TextStyleRun *> m_styleRuns;
};

}
}

#endif
