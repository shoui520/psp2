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
#include <math/ngp/v4.h>
#include <memory/heap_allocator.h>
#include "event_dispatcher.h"
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
class PhotoReviewView : public EventDispatcher {
public:
	PhotoReviewView(paf::Plugin *plugin);
	virtual ~PhotoReviewView();

	void SetBgColor(bool infobarShown, const paf::math::v4 &color);
	int SetPhoto(const paf::string &path, SceInt32 mode, paf::memory::HeapAllocator *decodeHeap,
		     void *workMemory);
	int SetExportParam(const paf::string &photoTitle, const paf::string &gameTitle,
			   const paf::string &gameComment);
	int StartDecode(void);
	int Start(void);
	int Stop(void);
	int Close(int32_t transition, float fadeTimeMs);

	/* the view's members: written with photoreview_dialog_view.cpp */
	SceUInt64 m_layout[(776 - 32) / 8];
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
