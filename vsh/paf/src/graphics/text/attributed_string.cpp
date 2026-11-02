/*
 * paf::graph::AttributedString: a wide string with its style runs (the file's name ours, after
 * ScePaf-RE's header). A style change splits the runs at the ends of its range and merges
 * neighbours left with the same style; a string change cuts and grows the runs it covers.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/text/attributed_string.h>
#include <graphics/text/text_style.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/string>
#include "../../paf_types.h"

namespace paf {
namespace graph {

/* Sony's build calls it from the constructors, Copy and the destructors rather than inlining it */
__attribute__((noinline)) void AttributedString::ClearStyleRuns()
{
	for (list<TextStyleRun *>::iterator it = m_styleRuns.begin(); it != m_styleRuns.end(); ++it)
		delete *it;
	m_styleRuns.clear();
}

AttributedString::AttributedString() : m_str(L"")
{
	ClearStyleRuns();
	AddStyleRun(NULL, 0);
}

AttributedString::AttributedString(const TextStyle &style) : m_str(L"")
{
	ClearStyleRuns();
	AddStyleRun(&style, 0);
}

void AttributedString::Copy(const AttributedString &str)
{
	m_str = str.m_str;
	ClearStyleRuns();
	for (list<TextStyleRun *>::iterator it = str.m_styleRuns.begin();
	     it != str.m_styleRuns.end(); ++it) {
		TextStyleRun *run = new TextStyleRun();

		if (run != NULL) {
			TextStyleRun *src = *it;

			run->style = src->style;
			run->len = src->len;
			m_styleRuns.push_back(run);
		}
	}
}

AttributedString::AttributedString(const AttributedString &str) : m_str(L"")
{
	Copy(str);
}

AttributedString::~AttributedString()
{
	ClearStyleRuns();
}

AttributedString &AttributedString::operator=(const AttributedString &str)
{
	Copy(str);
	return *this;
}

int32_t AttributedString::SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len,
					    int32_t val)
{
	return SetStyleAttributeT(attr, pos, len, val);
}

int32_t AttributedString::GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, int32_t &val)
{
	return GetStyleAttributeT(attr, pos, val);
}

int32_t AttributedString::SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len,
					    float val)
{
	return SetStyleAttributeT(attr, pos, len, val);
}

int32_t AttributedString::GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, float &val)
{
	return GetStyleAttributeT(attr, pos, val);
}

int32_t AttributedString::SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len,
					    const math::v2 &val)
{
	return SetStyleAttributeT(attr, pos, len, val);
}

int32_t AttributedString::GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, math::v2 &val)
{
	return GetStyleAttributeT(attr, pos, val);
}

int32_t AttributedString::SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len,
					    const math::v4 &val)
{
	return SetStyleAttributeT(attr, pos, len, val);
}

int32_t AttributedString::GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, math::v4 &val)
{
	return GetStyleAttributeT(attr, pos, val);
}

int32_t AttributedString::SetStyleAttribute(TextStyleAttribute attr, uint32_t pos, uint32_t len,
					    bool val)
{
	return SetStyleAttributeT(attr, pos, len, val);
}

int32_t AttributedString::GetStyleAttribute(TextStyleAttribute attr, uint32_t pos, bool &val)
{
	return GetStyleAttributeT(attr, pos, val);
}

int32_t AttributedString::SetStyle(uint32_t pos, uint32_t len, const TextStyle &style)
{
	if (pos == 0 && len == 0)
		len = m_str.length();

	uint32_t end = pos + len;

	if (end > m_str.length())
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	int32_t ret = SCE_PAF_ERROR_TEXT_UNCHANGED;
	uint32_t start = 0;
	TextStyleRun *prev = NULL;
	list<TextStyleRun *>::iterator it = m_styleRuns.begin();

	while (it != m_styleRuns.end()) {
		TextStyleRun *run = *it;
		uint32_t run_end = start + run->len;

		if (run->style != style) {
			if (pos <= start && end >= run_end) {
				run->style = style;
				ret = SCE_PAF_OK;
			} else if (pos > start && pos < run_end) {
				/* the run ends where the range starts, the rest a run of its own */
				TextStyleRun *rest = new TextStyleRun();

				if (rest == NULL) {
					ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
					break;
				}
				rest->style = run->style;
				rest->len = run_end - pos;
				run->len = pos - start;
				start = pos;
				++it;
				it = m_styleRuns.insert(it, rest);
				prev = run;
				continue;
			} else if (end > start && end < run_end) {
				/* a run up to where the range ends before it */
				TextStyleRun *head = new TextStyleRun();

				if (head == NULL) {
					ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
					break;
				}
				head->style = run->style;
				head->len = end - start;
				run->len = run_end - end;
				it = m_styleRuns.insert(it, head);
				continue;
			}
		}
		if (prev == NULL || run->style != prev->style) {
			++it;
			prev = run;
		} else {
			prev->len += run->len;
			it = m_styleRuns.erase(it);
			delete run;
		}
		start = run_end;
	}
	return ret;
}

int32_t AttributedString::GetStyle(uint32_t pos, TextStyle &style)
{
	if (pos == 0 || pos < m_str.length()) {
		uint32_t start = 0;

		for (list<TextStyleRun *>::iterator it = m_styleRuns.begin();
		     it != m_styleRuns.end(); ++it) {
			TextStyleRun *run = *it;
			uint32_t len = run->len;

			if (pos == start || (pos > start && pos < start + len)) {
				style = run->style;
				return SCE_PAF_OK;
			}
			start += len;
		}
	}
	return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
}

int32_t AttributedString::ResizeStyleRuns(uint32_t pos, uint32_t len, uint32_t new_len)
{
	uint32_t end = pos + len;

	if (end > m_str.length())
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	TextStyleRun *prev = NULL;
	uint32_t start = 0;
	list<TextStyleRun *>::iterator it = m_styleRuns.begin();

	while (it != m_styleRuns.end()) {
		TextStyleRun *run = *it;
		uint32_t run_len = run->len;
		/* what the run keeps outside the range */
		uint32_t kept = 0;
		int32_t after = start + run_len - end;

		if ((int32_t)(pos - start) > 0)
			kept = pos - start;
		if (after > 0)
			kept += after;
		if (kept > run->len)
			kept = run->len;
		/* the run the new characters go into */
		if ((pos == start + run_len && pos == m_str.length()) ||
		    (pos >= start && pos < start + run_len))
			kept += new_len;
		if (m_styleRuns.size() == 1 || kept != 0) {
			run->len = kept;
			if (prev == NULL || run->style != prev->style) {
				++it;
				prev = run;
			} else {
				prev->len += run->len;
				it = m_styleRuns.erase(it);
				delete run;
			}
		} else {
			it = m_styleRuns.erase(it);
			delete run;
		}
		start += run_len;
	}
	return SCE_PAF_OK;
}

int32_t AttributedString::SetString(const wstring &str)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_UNCHANGED;

	if (str != m_str) {
		ret = ResizeStyleRuns(0, m_str.length(), str.length());
		if (ret == SCE_PAF_OK)
			m_str = str;
	}
	return ret;
}

int32_t AttributedString::ReplaceString(uint32_t pos, uint32_t len, const wstring &str)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_UNCHANGED;

	if (str != m_str.substr(pos, len)) {
		ret = ResizeStyleRuns(pos, len, str.length());
		if (ret == SCE_PAF_OK) {
			m_str.erase(pos, len);
			m_str.insert(pos, str.c_str(), str.length());
		}
	}
	return ret;
}

int32_t AttributedString::GetString(wstring &str)
{
	str = m_str;
	return SCE_PAF_OK;
}

int32_t AttributedString::GetLength(uint32_t &len)
{
	len = m_str.length();
	return SCE_PAF_OK;
}

int32_t AttributedString::GetStringBuffer(const wchar_t *&buf, uint32_t &len)
{
	buf = m_str.c_str();
	len = m_str.length();
	return SCE_PAF_OK;
}

int32_t AttributedString::GetStyleRuns(_list_iterator<TextStyleRun *> &begin,
				       _list_iterator<TextStyleRun *> &end)
{
	begin = m_styleRuns.begin();
	end = m_styleRuns.end();
	return SCE_PAF_OK;
}

template<class T>
__attribute__((noinline)) int32_t AttributedString::SetStyleAttributeT(TextStyleAttribute attr, uint32_t pos, uint32_t len,
					     const T &val)
{
	if (pos == 0 && len == 0)
		len = m_str.length();

	uint32_t end = pos + len;

	if (end > m_str.length())
		return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;

	int32_t ret = SCE_PAF_ERROR_TEXT_UNCHANGED;
	uint32_t start = 0;
	TextStyleRun *prev = NULL;
	list<TextStyleRun *>::iterator it = m_styleRuns.begin();

	while (it != m_styleRuns.end()) {
		TextStyleRun *run = *it;
		uint32_t run_end = start + run->len;
		T cur;
		int32_t res = run->style.GetStyleAttribute(attr, cur);

		if (res < 0) {
			ret = res;
			break;
		}
		if (!(cur == val)) {
			if (pos <= start && end >= run_end) {
				ret = run->style.SetStyleAttribute(attr, val);
			} else if (pos > start && pos < run_end) {
				TextStyleRun *rest = new TextStyleRun();

				if (rest == NULL) {
					ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
					break;
				}
				rest->style = run->style;
				rest->len = run_end - pos;
				run->len = pos - start;
				start = pos;
				++it;
				it = m_styleRuns.insert(it, rest);
				prev = run;
				continue;
			} else if (end > start && end < run_end) {
				TextStyleRun *head = new TextStyleRun();

				if (head == NULL) {
					ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
					break;
				}
				head->style = run->style;
				head->len = end - start;
				run->len = run_end - end;
				it = m_styleRuns.insert(it, head);
				continue;
			}
		}
		if (prev == NULL || run->style != prev->style) {
			++it;
			prev = run;
		} else {
			prev->len += run->len;
			it = m_styleRuns.erase(it);
			delete run;
		}
		start = run_end;
	}
	return ret;
}

template<class T>
__attribute__((noinline)) int32_t AttributedString::GetStyleAttributeT(TextStyleAttribute attr, uint32_t pos, T &val)
{
	if (pos == 0 || pos < m_str.length()) {
		uint32_t start = 0;

		for (list<TextStyleRun *>::iterator it = m_styleRuns.begin();
		     it != m_styleRuns.end(); ++it) {
			TextStyleRun *run = *it;
			uint32_t len = run->len;

			if (pos == start || (pos > start && pos < start + len))
				return run->style.GetStyleAttribute(attr, val);
			start += len;
		}
	}
	return SCE_PAF_ERROR_TEXT_INVALID_ARGUMENT;
}

}
}
