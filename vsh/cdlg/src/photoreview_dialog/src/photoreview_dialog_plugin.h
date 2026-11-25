/*
 * ScePhotoReviewDialogPlugin's own interface between its files: the plugin's dialog, which shows
 * the caller's photo on its page (PhotoReviewView) and, when the caller asks, exports it to the
 * Photos application with the caller's metadata. The names the plugin writes into the work
 * memory (GetReviewParam, GetCommonParam, PhotoReviewDialogCallback, OnUpdate) and the jobs'
 * and heap's names are Sony's; the classes, the other names and this header's name are ours.
 */
#ifndef PHOTOREVIEW_DIALOG_PHOTOREVIEW_DIALOG_PLUGIN_H
#define PHOTOREVIEW_DIALOG_PHOTOREVIEW_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <photoreview_dialog.h>
#include <plugin.h>
#include <std/string>
#include <common/job_slots.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <memory/heap_allocator.h>
#include "dialog_page.h"
#include "event_dispatcher.h"
#include "gesture_handler.h"
#include "gui_dialog.h"
#include "input_listener.h"

/* the work memory is missing for a call, another call holds it (names ours) */
#define PHOTOREVIEW_PLUGIN_ERROR_NO_WORK_MEMORY	-2146420832	/* 0x801037A0 */
#define PHOTOREVIEW_PLUGIN_ERROR_API_NAME	-2146420831	/* 0x801037A1 */

/* the decode heap the plugin makes at the start of the caller's work memory */
#define PHOTOREVIEW_DECODE_HEAP_SIZE	0x400000

/* what the view tells its listeners (names ours) */
enum {
	PHOTOREVIEW_EVENT_RESULT = 1,	/* the dialog ends with the result its argument points to */
	PHOTOREVIEW_EVENT_OK = 2,	/* the dialog ends with SCE_COMMON_DIALOG_RESULT_OK */
	PHOTOREVIEW_EVENT_CLOSED = 3	/* the page has closed */
};

/*
 * The application a plugin's object is: the plugin's Init, Start, Stop and Exit, and the plugin it
 * belongs to, whose notify callbacks it may call.
 */
class PluginApp {
public:
	PluginApp(paf::Plugin *plugin);
	virtual ~PluginApp();

	virtual int Init(paf::Plugin *plugin) = 0;
	virtual int Exit(paf::Plugin *plugin) = 0;
	virtual int Start(paf::Plugin *plugin) = 0;
	virtual int Stop(paf::Plugin *plugin) = 0;
	virtual void Notify(int32_t status);

	paf::Plugin *m_plugin;
};

/* the dialog's page, its photo and its buttons (photoreview_dialog_view.cpp) */
class PhotoReviewView : public EventDispatcher, public DialogPage, public GestureHandler,
			public EventListener, public InputHandler {
public:
	PhotoReviewView(paf::Plugin *plugin);
	virtual ~PhotoReviewView();

	void SetBgColor(SceInt32 infobarShown, const paf::math::v4 &color);
	int SetPhoto(const paf::string &path, SceInt32 mode, paf::memory::HeapAllocator *decodeHeap,
		     void *workMemory);
	int SetExportParam(const paf::string &photoTitle, const paf::string &gameTitle,
			   const paf::string &gameComment);
	void StartDecode(void);

	virtual int Open();
	virtual int Close();
	virtual void OnInit(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnButton(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnHidden();

	virtual void OnGesture01(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture13(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture14(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture15(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);
	virtual void OnGesture16(int32_t type, paf::ui::Handler *self, paf::ui::Event *e);

	virtual void OnEvent(EventDispatcher *sender, int32_t event, void *arg);
	virtual void OnPadInput(const paf::inputdevice::pad::Data *data,
				const paf::inputdevice::pad::Data *pre);

private:
	void ShowUi(void);
	void HideUi(void);
	void OpenSaveConfirmDialog(void);
	void OpenSaveWaitDialog(void);
	void OpenErrorDialog(SceInt32 error);
	void DisablePhotoGestures(void);
	void EnablePhotoGestures(void);
	void UpdatePhotoLimits(const paf::math::v4 &fitSize, float aspect);
	void UpdatePhotoDragLimit(paf::ui::Widget *widget);
	static int ClampFocus(paf::math::v2 *focus, const paf::math::rect &viewRect,
			      const paf::math::rect &photoRect);
	void OnJobRemoved(uint32_t index, paf::common::SharedPtr<paf::job::JobItem> item);
	void OnZoomAnimEnd(paf::Timer *t, paf::ui::AnimListener *listener);
	void OnStoreButtonDecide(int32_t type, paf::ui::Handler *self);
	void HandleZoomInput(const paf::inputdevice::pad::Data *data);

	static void JobRemovedCB(uint32_t index, paf::common::SharedPtr<paf::job::JobItem> item,
				 void *userdata);
	static void ZoomAnimEndCB(paf::Timer *t, paf::ui::AnimListener *listener, void *data1,
				  void *data2, void *data3);
	static void StoreButtonCB(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				  void *userdata);

	paf::common::JobSlots m_jobSlots;
	paf::string m_path;
	SceInt32 m_mode;
	bool m_uiShown;
	bool m_hasFatalError;
	SceInt32 m_fatalError;
	GuiConfirmDialog m_saveConfirmDialog;
	GuiWaitDialog m_saveWaitDialog;
	GuiMessageDialog m_errorDialog;
	paf::math::v4 m_photoSize;
	paf::math::v4 m_fitSize;
	paf::math::v4 m_reserved624;
	paf::math::v2 m_reserved640;
	SceUInt8 m_reserved648[32];
	paf::memory::HeapAllocator *m_decodeHeap;
	void *m_workMemory;
	SceInt32 m_infobarShown;
	paf::math::v4 m_bgColor;
	paf::math::v4 m_viewSize;
	paf::string m_photoTitle;
	paf::string m_gameTitle;
	paf::string m_gameComment;
	paf::string m_storeContentId;
};

class PhotoReviewDialog : public EventListener, public PluginApp {
public:
	typedef void (*Callback)(const ScePhotoReviewDialogResult *result, void *userdata);

	PhotoReviewDialog(paf::Plugin *plugin);
	virtual ~PhotoReviewDialog();

	virtual void OnEvent(EventDispatcher *sender, int32_t event, void *arg);
	virtual int Init(paf::Plugin *plugin);
	virtual int Start(paf::Plugin *plugin);
	virtual int Stop(paf::Plugin *plugin);
	virtual int Exit(paf::Plugin *plugin);

	int SetBgColor(bool infobarShown, const SceCommonDialogColor *color);
	int SetParam(const ScePhotoReviewDialogParam *param, Callback callback, void *userdata);

	PhotoReviewView m_view;
	ScePhotoReviewDialogParam m_param;
	ScePhotoReviewDialogResult m_result;
	Callback m_callback;
	void *m_userdata;
	/* the heap the view decodes the photo in, in the caller's work memory */
	paf::memory::HeapAllocator *m_decodeHeap;
};

#endif
