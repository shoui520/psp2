/*
 * SceRemoteOSKDialogPlugin's own interface between its files: the dialog that shows the text a
 * companion device's keyboard types (SceCompanionUtil), in a text box on the plugin's page. Sony's
 * names here are the plugin's function names it writes into the work memory (Init, Exit, Callback,
 * OnUpdate, CloseDialogCallback); the classes, the other names, this header's name and the files'
 * are ours.
 */
#ifndef REMOTE_OSK_DIALOG_REMOTE_OSK_DIALOG_PLUGIN_H
#define REMOTE_OSK_DIALOG_REMOTE_OSK_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <libime.h>
#include <plugin.h>
#include <std/string>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_button.h>
#include <widget/w_text_box.h>
#include <widget/w_scroll_view_base.h>
#include "../cdlg_mgr/common_dialog_mgr.h"

/* the plugin's page */
#define REMOTE_OSK_DIALOG_PAGE_ID	0x4EB6378F

/* the plugin's errors (names ours) */
#define REMOTE_OSK_DIALOG_ERROR_BUSY		-2130706686	/* 0x81000002: open already, or not ready */
#define REMOTE_OSK_DIALOG_ERROR_DIALOG_MODE	-2130706685	/* 0x81000003 */
#define REMOTE_OSK_DIALOG_ERROR_TEXTBOX_MODE	-2130706684	/* 0x81000004 */
#define REMOTE_OSK_DIALOG_ERROR_NOT_READY	-2130706680	/* 0x81000008 */

/* the most text the companion's keyboard types */
#define REMOTE_OSK_DIALOG_MAX_TEXT_LENGTH	2048

/* what the text field tells the companion as its text changes (SceCompanionUtil_04BC85DC) */
typedef int (*RemoteOskTextFunction)(int composeStart, int composeLength, SceUInt32 caret,
				     int replaceStart, int replaceLength, const SceWChar16 *text);

/*
 * How the companion's keyboard edits the text box, from the flags the companion opens the text
 * field with: the system IME's input method (0), type, options and enter label, the text box's and
 * the dialog's modes as SceImeDialogParam's, the most text, the languages and the filter, and what
 * hears of the text's changes. The fields are SceImeParam's and SceImeDialogParam's names.
 */
typedef struct RemoteOskImeParam {
	SceUInt32 inputMethod;
	SceUInt32 type;
	SceUInt32 option;
	SceUInt8 enterLabel;
	SceUInt32 textBoxMode;
	SceUInt32 dialogMode;
	SceUInt32 maxTextLength;
	SceUInt64 supportedLanguages;
	SceUInt32 languagesForced;
	SceImeTextFilter filter;
	RemoteOskTextFunction textChanged;
} RemoteOskImeParam;

/* RemoteOskImeParam's types: SceIme's own (ime.h) */
#define REMOTE_OSK_IME_TYPE_MAX		5
#define REMOTE_OSK_IME_OPTION_MASK	7
#define REMOTE_OSK_IME_LANGUAGE_MASK	0xFFFFFULL

/*
 * What the dialog calls: closed, with the status of the companion's text field (1 entered, 2
 * canceled) and the common dialog's result, and (2, 0) when its enter button is pressed.
 */
typedef void (*RemoteOskDialogFunction)(SceUInt32 status, SceUInt32 result);

/* the plugin (remote_osk_dialog_plugin.cpp), which the dialog opens its page from */
extern paf::Plugin *g_remoteOskDialogPlugin;

void RemoteOskImeParamInit(RemoteOskImeParam *param);
int RemoteOskImeParamCheck(const RemoteOskImeParam *param);

/* the one dialog the plugin has open (remote_osk_dialog.cpp) */
class RemoteOskDialog {
public:
	RemoteOskDialog(RemoteOskDialogParam *param, RemoteOskDialogFunction callback,
			bool infobarShown, const RemoteOskImeParam *imeParam, bool idle);
	~RemoteOskDialog();

	void Layout(bool open);
	int SetText(const paf::wstring &text);
	void GetImeParam(paf::ui::TextBox::ImeParam *imeParam);
	void EndEdit();
	void BeginEdit();
	void Close(int reason);
	void TextChanged();

	static int Reset();
	static int Create(RemoteOskDialogParam *param, RemoteOskDialogFunction callback,
			  bool infobarShown, const RemoteOskImeParam *imeParam, bool idle);
	static int SetCurrentText(const paf::wstring &text);
	static int Abort(bool soon);
	static int SetActive(bool active, const RemoteOskImeParam *imeParam);
	static int GetActive(bool *active);
	static int Destroy();

	RemoteOskDialogFunction m_callback;
	paf::ui::Scene *m_scene;
	paf::ui::TextBox *m_textBox;
	paf::ui::ScrollView *m_scrollView;
	/* the button that clears the text (SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR) */
	paf::ui::Button *m_clearButton;
	RemoteOskDialogParam *m_param;
	/* the height the text box shows */
	SceInt32 m_height;
	bool m_infobarShown;
	RemoteOskImeParam m_imeParam;
	/* no text field of the companion's is open: the page waits for one */
	bool m_idle;
	/* a text came while the text box edited, to be set when its editing has gone */
	bool m_textPending;
	paf::wstring m_pendingText;
	/* the composing span and the text's length the companion last heard of */
	SceInt32 m_composeStart;
	SceInt32 m_composeLength;
	SceInt32 m_length;
	/* the caret the companion last heard of */
	bool m_caretSent;
	SceUInt32 m_caret;
};

#endif
