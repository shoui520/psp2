/*
 * SceFriendList2DialogPlugin: PopupListView ("page_popup_list_view",
 * "popup_list_view_list_item", "popup_list_view_sub_menu_mark").
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_scene.h>
#include "popup_list_view.h"
#include "friend_lib.h"

using namespace paf;
using namespace paf::ui;

struct PopupListSession {
	PopupListSession()
		: depth(-1), plugin(NULL), page(NULL), listView(NULL), callback(NULL), items()
	{
	}

	~PopupListSession()
	{
		if (callback != NULL) {
			delete callback;
			callback = NULL;
		}
	}

	int32_t depth;
	Plugin *plugin;
	Scene *page;
	ListView *listView;
	PopupListCallback *callback;
	vector<PopupListEntry> items;
};

static PopupListSession *s_popupSession = NULL;

void PopupListInit(void)
{
	s_popupSession = NULL;
}

void PopupListClose(void)
{
	if (s_popupSession != NULL) {
		if (s_popupSession->plugin != NULL && s_popupSession->page != NULL) {
			Plugin::PageCloseParam closeParam;

			s_popupSession->plugin->PageClose(IDParam("page_popup_list_view"),
							  closeParam);
			s_popupSession->page = NULL;
		}
		delete s_popupSession;
		s_popupSession = NULL;
	}
}

bool PopupListIsOpen(void)
{
	return s_popupSession != NULL && s_popupSession->page != NULL;
}

static void OnPopupItemDecide(int32_t eventType, Handler *self, Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	int32_t chosenIndex = (int32_t)(intptr_t)userData;

	if (s_popupSession == NULL)
		return;
	PopupListCallback *cb = s_popupSession->callback;

	s_popupSession->callback = NULL;
	PopupListClose();
	if (cb != NULL) {
		cb->OnSelect(chosenIndex);
		delete cb;
	}
}

static void OnPopupCancel(int32_t eventType, Handler *self, Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;
	if (s_popupSession == NULL)
		return;
	PopupListCallback *cb = s_popupSession->callback;

	s_popupSession->callback = NULL;
	PopupListClose();
	if (cb != NULL) {
		cb->OnSelect(-1);
		delete cb;
	}
}

int PopupListOpen(const PopupListOpenParam &param)
{
	if (param.plugin == NULL || param.items.empty()) {
		if (param.callback != NULL) {
			param.callback->OnSelect(-1);
			delete param.callback;
		}
		return -1;
	}
	PopupListClose();

	s_popupSession = new PopupListSession();
	s_popupSession->depth = param.depth;
	s_popupSession->plugin = param.plugin;
	s_popupSession->items = param.items;
	s_popupSession->callback = param.callback;

	Plugin::PageOpenParam openParam;

	openParam.option = Plugin::PageOption_Create;
	s_popupSession->page =
		param.plugin->PageOpen(IDParam("page_popup_list_view"), openParam);
	if (s_popupSession->page == NULL) {
		PopupListClose();
		return -1;
	}

	s_popupSession->page->AddEventCallback(
		ButtonBase::CB_BTN_DECIDE, OnPopupCancel, NULL);

	for (size_t i = 0; i < s_popupSession->items.size(); ++i) {
		Widget *itemWidget = param.plugin->CreateWidget(
			s_popupSession->page, "list_item", IDParam("popup_list_view_list_item"),
			IDParam());

		if (itemWidget != NULL) {
			itemWidget->SetString(s_popupSession->items[i].label);
			itemWidget->AddEventCallback(ButtonBase::CB_BTN_DECIDE, OnPopupItemDecide,
						     (void *)(intptr_t)i);
			if (s_popupSession->items[i].hasSubMenu) {
				Widget *mark = itemWidget->FindChild(
					IDParam("popup_list_view_sub_menu_mark"), 0);
				if (mark != NULL)
					mark->Show(0, NULL, NULL, NULL);
			}
		}
	}
	return 0;
}
