/*
 * impose_screenshot.cpp: impose_plugin's screenshot: the capture asked of the shell, the
 * flash shown while it is taken, and the notices of its errors (0x81149A7C-0x8114A9A4).
 * The file's and the names are ours (PLACEHOLDER), after its "ScreenShotCapture" and
 * "SCREENSHOT_*" notices.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <thread/thread.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/animlistener.h>
#include <widget/w_plane.h>
#include <widget/w_button.h>
#include <screenshot/error.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../../plugins.h"
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

/* the flash's template and widgets (PLACEHOLDER names) */
#define IMPOSE_SCREENSHOT_TEMPLATE		0xA04EDB0B
#define IMPOSE_SCREENSHOT_FLASH			0x9C9CF24C
#define IMPOSE_SCREENSHOT_BUTTON		0xF282D1C6
/* the notices' icon */
#define IMPOSE_SCREENSHOT_TEX_NOTICE		0x2617A32D

/* the notices' messages (PLACEHOLDER names) */
#define IMPOSE_SCREENSHOT_MSG_PROHIBITED	0xBA629E30
#define IMPOSE_SCREENSHOT_MSG_FULL		0xE2A2DDCC
#define IMPOSE_SCREENSHOT_MSG_NOT_EXIST		0x9E9A4434
#define IMPOSE_SCREENSHOT_MSG_CONTENT_FULL	0xE5EA6CFC
#define IMPOSE_SCREENSHOT_MSG_CONTENT_FULL_ARG	0x93777E84
#define IMPOSE_SCREENSHOT_MSG_ERROR		0xC51AA4A4

/* the shell's screenshot errors no SDK header names (PLACEHOLDER names, after the notices) */
#define IMPOSE_SCREENSHOT_ERROR_REFUSED		0x80102FA1
#define IMPOSE_SCREENSHOT_ERROR_PROHIBITED	0x80102FA6
#define IMPOSE_SCREENSHOT_ERROR_DEBUG_SAVED	0x80102FAB

/* the flash's timer, hidden a frame after it faded */
#define IMPOSE_SCREENSHOT_TIMER			0x1234

/* the flash's fade (ms) and the frame it is hidden after (s) */
#define IMPOSE_SCREENSHOT_FADE			800.0f
#define IMPOSE_SCREENSHOT_FRAME			(1.0f / 60.0f)

/* (0x815448E8) */
ImposeScreenshot *g_imposeScreenshot;

/* the child, when it is of the type */
#define IMPOSE_SCREENSHOT_FIND(parent, type, id)					\
	((parent)->FindChild(IDParam(id), 0) != NULL &&					\
	 (parent)->FindChild(IDParam(id), 0)->IsInherit(type::TypeName()) ?		\
	 (type *)(parent)->FindChild(IDParam(id), 0) : NULL)

/* shows a widget at once (inline in paf's callers) */
static inline void imposeScreenshotShowWidget(ui::Widget *widget)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
}

/* hides a widget at once (inline in paf's callers) */
static inline void imposeScreenshotHideWidget(ui::Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

static void imposeScreenshotCapture(void *data);
static void imposeScreenshotOnFlash(Timer *t, ui::AnimListener *listener, void *data1,
				    void *data2, void *data3);
static void imposeScreenshotOnFlashEnd(SceInt32 type, ui::Handler *self, ui::Event *e,
				       void *data);
static void imposeScreenshotOnPower(void);
static void imposeScreenshotRequest(void *data);

/* (0x81149A7C) */
ImposeScreenshot::ImposeScreenshot(Plugin *plugin)
	: m_plugin(plugin), m_page(NULL), m_reserved(-1), m_capturing(false)
{
	g_imposeScreenshot = this;
	g_imposeInterface->screenshot = (void (*)(void))imposeScreenshotCapture;
}

/* (0x81149AAA) */
ImposeScreenshot::~ImposeScreenshot()
{
}

/* the flash made from its template in the plugin's second page, hidden, and the shell's
   screenshot requests taken (0x81149AAC) */
void ImposeScreenshot::Open()
{
	if (m_page != NULL)
		return;

	ui::Scene *page = g_impose != NULL ? g_impose->m_page2 : NULL;

	if (page != NULL) {
		m_plugin->TemplateOpen(page, IDParam(IMPOSE_SCREENSHOT_TEMPLATE),
				       Plugin::TemplateOpenParam());

		ui::Plane *flash = IMPOSE_SCREENSHOT_FIND(page, ui::Plane, IMPOSE_SCREENSHOT_FLASH);

		if (flash == NULL)
			return;
		flash->m_hit_detect_mode = 2;
		flash->m_hit_detect_b_mode = 2;
		imposeScreenshotHideWidget(flash);

		ui::Button *button = IMPOSE_SCREENSHOT_FIND(page, ui::Button,
							    IMPOSE_SCREENSHOT_BUTTON);

		if (button == NULL)
			return;
		imposeScreenshotHideWidget(button);
	}
	m_page = page;
	shellScreenshotSetRequestCallback(imposeScreenshotRequest, this);
}

/* the error the capture gave told in a notice (0x81149E96) */
void ImposeScreenshot::ShowResult(int error)
{
	ImposeNotice notice;

	if (error == 0 || error == SCE_SCREENSHOT_ERROR_CAPTURE_FAILED)
		return;
	if (error == IMPOSE_SCREENSHOT_ERROR_REFUSED) {
		g_imposeInterface->zoomRefuse();
	} else if (error == IMPOSE_SCREENSHOT_ERROR_PROHIBITED) {
		notice.name = "SCREENSHOT_PROHIBITED";
		notice.iconHash = IMPOSE_SCREENSHOT_TEX_NOTICE;
		notice.message = g_imposeInterface->getString(
			IDParam(IMPOSE_SCREENSHOT_MSG_PROHIBITED));
		g_imposeInterface->showNotice(&notice);
	} else if (error == SCE_SCREENSHOT_ERROR_MEDIA_FULL) {
		notice.name = "SCREENSHOT_ERROR_FULL";
		notice.iconHash = IMPOSE_SCREENSHOT_TEX_NOTICE;
		notice.message = g_imposeInterface->getString(IDParam(IMPOSE_SCREENSHOT_MSG_FULL));
		g_imposeInterface->showNotice(&notice);
	} else if (error == SCE_SCREENSHOT_ERROR_MEDIA_NOT_EXIST) {
		notice.name = "SCREENSHOT_ERROR_NOT_EXIST";
		notice.iconHash = IMPOSE_SCREENSHOT_TEX_NOTICE;
		notice.message = g_imposeInterface->getString(
			IDParam(IMPOSE_SCREENSHOT_MSG_NOT_EXIST));
		g_imposeInterface->showNotice(&notice);
	} else if (error == SCE_SCREENSHOT_ERROR_CONTENT_FULL) {
		notice.name = "SCREENSHOT_ERROR_CONTENT_FULL";
		notice.iconHash = IMPOSE_SCREENSHOT_TEX_NOTICE;

		wstring text = shellFormatText(
			g_imposeInterface->getString(IDParam(IMPOSE_SCREENSHOT_MSG_CONTENT_FULL)),
			g_imposeInterface->getString(IDParam(IMPOSE_SCREENSHOT_MSG_CONTENT_FULL_ARG)));

		notice.message = text.c_str();
		g_imposeInterface->showNotice(&notice);
	} else if (error == IMPOSE_SCREENSHOT_ERROR_DEBUG_SAVED) {
		notice.name = "SCREENSHOT_ERROR_DEBUG_SAVED_OK";
		notice.iconHash = IMPOSE_SCREENSHOT_TEX_NOTICE;
		notice.message = L"Need [Debug Settings]>[System]>[Force Rebuild Database]>"
				 L"[Photo Database]";
		g_imposeInterface->showNotice(&notice);
	} else {
		notice.name = "SCREENSHOT_ERROR";
		notice.iconHash = IMPOSE_SCREENSHOT_TEX_NOTICE;
		notice.message = g_imposeInterface->getString(IDParam(IMPOSE_SCREENSHOT_MSG_ERROR));
		g_imposeInterface->showNotice(&notice);
	}
}

/* the flash shown and faded out while the capture is taken (0x8114A348) */
void ImposeScreenshot::ShowCapturing()
{
	if (m_page == NULL)
		return;

	ui::Plane *flash = IMPOSE_SCREENSHOT_FIND(m_page, ui::Plane, IMPOSE_SCREENSHOT_FLASH);

	if (flash == NULL)
		return;

	math::v4 faded(1.0f, 1.0f, 1.0f, 0.0f);
	math::v4 shown(1.0f, 1.0f, 1.0f, 1.0f);
	ShellPluginInterface *power = (ShellPluginInterface *)
		applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((void (*)(void (*)(void), void *))
			power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(imposeScreenshotOnPower,
									    this);
	imposeScreenshotShowWidget(flash);
	flash->SetColor(shown, NULL, ui::Widget::ANIM_COLOR, 2);
	flash->SetColor(faded, new Timer(IMPOSE_SCREENSHOT_FADE), ui::Widget::ANIM_COLOR, 0, 0.0f,
			imposeScreenshotOnFlash, this);
}

/* the flash taken down once the capture is done (0x8114A5F4) */
void ImposeScreenshot::HideCapturing()
{
	ui::Plane *flash = IMPOSE_SCREENSHOT_FIND(m_page, ui::Plane, IMPOSE_SCREENSHOT_FLASH);

	if (flash == NULL)
		return;
	flash->KillAnimType(ui::Widget::ANIM_COLOR);
	imposeScreenshotHideWidget(flash);
	flash->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_COLOR);
	if (flash->m_map_t_listener.find(ui::Handler::CB_TIMEOUT | IMPOSE_SCREENSHOT_TIMER) !=
	    flash->m_map_t_listener.end())
		flash->KillTimeoutEvent(IMPOSE_SCREENSHOT_TIMER);

	ShellPluginInterface *power = (ShellPluginInterface *)
		applauncherPluginGetInterface("power_manage_plugin", 1);

	if (power != NULL)
		((void (*)(void (*)(void), void *))
			power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(
			imposeScreenshotOnPower, this);
}

/* the flash faded out: hidden a frame later (0x8114A85A) */
static void imposeScreenshotOnFlash(Timer *t, ui::AnimListener *listener, void *data1,
				    void *data2, void *data3)
{
	listener->m_widget->SetTimeoutEvent(IMPOSE_SCREENSHOT_TIMER, IMPOSE_SCREENSHOT_FRAME,
					    imposeScreenshotOnFlashEnd, data1);
}

/* the flash hidden (0x8114A8B8) */
static void imposeScreenshotOnFlashEnd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	imposeScreenshotHideWidget((ui::Widget *)self);
}

static void imposeScreenshotOnCaptured(int *result, void *data);

/* the interface's screenshot: the zoom held off while the shell takes the capture, the flash
   shown, or the error told (0x8114A8FE) */
static void imposeScreenshotCapture(void *data)
{
	ImposeScreenshot *screenshot = g_imposeScreenshot;

	if (screenshot != NULL) {
		shellZoomDisable();

		int ret = shellScreenshotCapture(imposeScreenshotOnCaptured, screenshot);

		if (ret == 0) {
			screenshot->m_capturing = true;
			screenshot->ShowCapturing();
		} else {
			screenshot->ShowResult(ret);
			shellZoomEnable();
		}
	}
}

/* the capture done: the flash taken down, an error told, and the zoom given back (0x8114A93C) */
static void imposeScreenshotOnCaptured(int *result, void *data)
{
	ImposeScreenshot *screenshot = g_imposeScreenshot;

	if (screenshot != NULL) {
		int ret = *result;

		screenshot->HideCapturing();
		if (screenshot->m_capturing)
			screenshot->m_capturing = false;
		if (ret != 0)
			screenshot->ShowResult(ret);
		shellZoomEnable();
	}
}

/* a power event while the capture is taken (0x8114A96A) */
static void imposeScreenshotOnPower(void)
{
	if (g_imposeInterface != NULL)
		g_imposeInterface->zoomRefuse();
}

/* the shell's screenshot request: the capture taken on the main thread (0x8114A980) */
static void imposeScreenshotRequest(void *data)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("ScreenShotCapture",
							 imposeScreenshotCapture, data);
}
