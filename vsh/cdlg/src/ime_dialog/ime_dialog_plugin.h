/*
 * SceImeDialogPlugin's own interface between its files: the dialog that edits the caller's text
 * in a text box on the plugin's page. Sony's only name here is ImeDialogCallback (the name the
 * plugin writes into the work memory); the class, its methods, this header's name and the
 * dialog file's are ours.
 */
#ifndef IME_DIALOG_IME_DIALOG_PLUGIN_H
#define IME_DIALOG_IME_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <ime_dialog.h>
#include <plugin.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_button.h>
#include <widget/w_text_box.h>
#include <widget/w_scroll_view_base.h>

/* the plugin's resource: its name and page */
#define IME_DIALOG_PLUGIN_NAME		"ime_dialog_plugin"
#define IME_DIALOG_PAGE_ID		0x4EB6378F

/* what the dialog calls when it has closed: the button that closed it and the result */
typedef void (*ImeDialogFunction)(SceInt32 button, SceInt32 result);

/* the plugin (ime_dialog_plugin.cpp), which the dialog opens its page from */
extern paf::Plugin *g_imeDialogPlugin;

/* the one dialog the plugin has open (ime_dialog.cpp) */
class ImeDialog {
public:
	/* how the dialog closes: entered, the close (enter) key, or canceled */
	enum CloseReason {
		CLOSE_REASON_ENTER = 0,
		CLOSE_REASON_CLOSE = 1,
		CLOSE_REASON_CANCEL = 2
	};

	ImeDialog(SceImeDialogParam *param, ImeDialogFunction callback, bool infobarShown);
	~ImeDialog();

	void Close(int reason);
	void ScrollToCaret();
	void GetImeParam(paf::ui::TextBox::ImeParam *imeParam);
	void BeginEdit();

	static int Reset();
	static int Create(SceImeDialogParam *param, ImeDialogFunction callback, bool infobarShown);
	static int Destroy();
	static int Abort(bool soon);

	ImeDialogFunction m_callback;
	paf::ui::Scene *m_scene;
	paf::ui::TextBox *m_textBox;
	paf::ui::ScrollView *m_scrollView;
	/* the button that clears the text (SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR) */
	paf::ui::Button *m_clearButton;
	SceImeDialogParam *m_param;
	/* the caret's line, and the text's last line, as the scroll last followed them */
	SceInt32 m_caretLine;
	SceInt32 m_lineCount;
	/* the height the text box shows */
	SceInt32 m_height;
};

#endif
