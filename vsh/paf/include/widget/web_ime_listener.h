/*
 * paf::ui::WebImeListener (2014 PAF WebView IME listener, 0x81117dfc..0x81118316):
 * default WebEditListener instantiated by wutil::Init when PAF_HAS_WEBVIEW is enabled
 * and no caller-provided web_edit_listener is supplied. Bridges WebViewBase text fields
 * to ScePafLowlayer's system IME (sceImeVshOpen / sceImeVshSetParam / sceImeVshClose /
 * sceImeVshUpdate). Class and member names are ours (following TextBoxImeListener).
 */
#ifndef PAF_WIDGET_WEB_IME_LISTENER_H
#define PAF_WIDGET_WEB_IME_LISTENER_H

#include <stddef.h>
#include <stdint.h>
#include <ime/ime.h>
#include <math/ngp/v4.h>
#include <widget/w_webview_base.h>

namespace paf {
namespace ui {

/* 0x80 bytes (inherits 0x3C-byte WebEditListener base). */
class WebImeListener : public WebEditListener {
public:
	WebImeListener();
	virtual ~WebImeListener();

	virtual int32_t BeginEdit(WebViewBase *view);
	virtual int32_t EndEdit(bool switching, WebViewBase *view);
	virtual int32_t Update(WebViewBase *view);
	virtual int32_t OnBeginEdit(WebViewBase *view);
	virtual int32_t HandleImeEvent(const SceImeVshEvent *e);

	uint64_t ConvertLanguages(uint32_t languages);
	static void OnImeEvent(void *arg, const SceImeVshEvent *e);

	WebViewBase *m_view;
	wchar_t *m_work;
	void *m_ime;
	uint32_t m_preedit_index;
	uint32_t m_preedit_len;
	uint32_t m_caret_pos;
	math::v4 m_preedit_back_color;
	math::v4 m_preedit_underline_color;
	bool m_switching;
};

}
}

#endif
