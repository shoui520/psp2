/*
 * applauncher_ipnotice.cpp: the intellectual property notice, a view in the topmenu's layer
 * with the notice's text and a close button (0x810DB040-0x810DB87C). The file's name is ours
 * (PLACEHOLDER): no record of the 3.6x shell names it.
 */

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_plane.h>
#include <widget/w_plane_button.h>
#include <widget/w_text.h>
#include <widget/w_scene.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../plugins.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"

using namespace paf;

/* the notice's template, opened in the topmenu's layer, and the ID string it is given */
#define APPLAUNCHER_IP_TEMPLATE		0x851F0CBD
#define APPLAUNCHER_IP_NAME		"Intellectual"

/* the template's widgets: its view (the information's), its text and the close button */
#define APPLAUNCHER_IP_VIEW		0x0CD9299D
#define APPLAUNCHER_IP_TEXT		0x9004DE80
#define APPLAUNCHER_IP_CLOSE		0xC57612A0

/* the notice's text, with the PlayStation marks put in its %s tokens */
#define APPLAUNCHER_IP_MSG		0x892E5852

/* PLACEHOLDER: the key the close button is pressed with */
#define APPLAUNCHER_IP_CLOSE_KEY	32

/* the input locked while the view slides out */
#define APPLAUNCHER_IP_LOCK_DEVICES	127

/* PLACEHOLDER: the notice (4 bytes) */
class ApplauncherIntellectualProperty {
public:
	ApplauncherIntellectualProperty();
	~ApplauncherIntellectualProperty();

	ui::Widget *m_root;		/* the topmenu's layer it is opened in */
};

/* (0x815443DC) */
static ApplauncherIntellectualProperty *s_intellectualProperty;

#define APPLAUNCHER_IP_FIND(parent, type, id)						\
	((parent)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (parent)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(parent)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

/* hides a widget over the timer's time (inline in paf's callers) */
static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* the close button pressed: the edit asked for (0x810DB7F4) */
static void applauncherIntellectualPropertyOnClose(int32_t type, ui::Handler *self,
						   ui::Event *e, void *data);

/*
 * The template opened in the topmenu's layer and slid in, sized to its scene, its text with the
 * PlayStation marks, and its close button (0x810DB040).
 */
ApplauncherIntellectualProperty::ApplauncherIntellectualProperty()
	: m_root(NULL)
{
	s_intellectualProperty = this;

	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);
	Plugin *plugin = g_applauncherHome->m_plugin;

	m_root = ((ui::Widget *(*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD])(0);
	plugin->TemplateOpen(m_root, IDParam(s_empty_name, APPLAUNCHER_IP_TEMPLATE),
			     Plugin::TemplateOpenParam());
	m_root->m_name.SetID(string(APPLAUNCHER_IP_NAME));

	ui::Widget *view = APPLAUNCHER_IP_FIND(m_root, ui::Plane, APPLAUNCHER_IP_VIEW);

	if (view == NULL)
		return;

	int32_t width, height;

	ScePafWidget_29BCC252(view->m_scene, &width, &height);
	view->SetSize(math::v4((float)width, (float)height, 0.0f, 0.0f));

	wstring text = shellFormatText(plugin->GetString(IDParam(s_empty_name, APPLAUNCHER_IP_MSG)),
		L"\xF8E3", L"\xF8E4", L"\xF8E5", L"\xF8ED", L"\xF8E6", L"\xF8E7", L"\xF8EA",
		L"\xF8EB", L"\xF8EC");
	ui::Text *message = APPLAUNCHER_IP_FIND(view, ui::Text, APPLAUNCHER_IP_TEXT);

	if (message != NULL)
		message->SetString(text);

	ScePafWidget_42C5A4D4(view, 0, 0);
	HideWidget(view, NULL);
	ScePafWidget_44B22ACA(view, common::transition::Type_SlideFromBottom5, 0.0f, NULL, NULL);

	ui::Widget *close = APPLAUNCHER_IP_FIND(view, ui::PlaneButton, APPLAUNCHER_IP_CLOSE);

	if (close != NULL) {
		ScePafWidget_21125870(close, APPLAUNCHER_IP_CLOSE_KEY);
		close->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ui::EventCBListener(applauncherIntellectualPropertyOnClose, this));
		ScePafWidget_EC6175D6(close, true);
		close->SetFocusDirectionWidget(0, close);
		close->SetFocusDirectionWidget(1, close);
		close->SetFocusDirectionWidget(2, close);
		close->SetFocusDirectionWidget(3, close);
	}
}

/* (0x810DB78C) */
void applauncherIntellectualPropertyOpen(void)
{
	new ApplauncherIntellectualProperty;
}

/* the view slid out, the input locked meanwhile (inline in applauncherIntellectualPropertyClose) */
ApplauncherIntellectualProperty::~ApplauncherIntellectualProperty()
{
	if (m_root != NULL) {
		float time = ScePafWidget_F1176ACC(m_root,
			common::transition::Type_SlideFromBottom5, 0.0f, NULL, NULL);

		ScePafWidget_5A9793B0(m_root, time);
		ScePafMisc_CAABF0B0(APPLAUNCHER_IP_LOCK_DEVICES, time);
	}
	s_intellectualProperty = NULL;
}

/* (0x810DB79E) */
void applauncherIntellectualPropertyClose(void)
{
	if (s_intellectualProperty != NULL)
		delete s_intellectualProperty;
}

static void applauncherIntellectualPropertyOnClose(int32_t type, ui::Handler *self,
						   ui::Event *e, void *data)
{
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}
