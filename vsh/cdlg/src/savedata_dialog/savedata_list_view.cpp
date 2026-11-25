/*
 * SceSaveDataDialogPlugin: SaveDataItemVisualizer and SaveDataListView implementation.
 */
#include <string.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include "../msg_dialog/msg_dialog_util.h"
#include "savedata_list_view.h"

using namespace paf;

static const char kSaveDataItemVisualizerTypeName[] = "savedata_item_visualizer";

static const SceUInt32 kCellTitleWidgetHash = 0xDEB0EA64u;
static const SceUInt32 kCellEmptyTitleWidgetHash = 0x5544716Du;
static const SceUInt32 kCellDateWidgetHash = 0xA1D2927Eu;
static const SceUInt32 kCellSubTitleWidgetHash = 0x99FA32BEu;
static const SceUInt32 kCellIconWidgetHash = 0xB5286248u;
static const SceUInt32 kEmptySlotStringHash = 0xB2B1575Fu;
static const SceUInt32 kDefaultIconTextureHash = 0x850A57C4u;
static const SceUInt32 kListContainerHash = 0x60634ECBu;

SaveDataItemVisualizer::SaveDataItemVisualizer(Plugin *plugin,
	const common::SharedPtr<SaveDataSlotList> &slotList)
	: m_plugin(plugin)
	, m_slotList(slotList)
	, m_lineCount(1)
{
}

SaveDataItemVisualizer::~SaveDataItemVisualizer()
{
}

const char *SaveDataItemVisualizer::TypeName(void)
{
	return kSaveDataItemVisualizerTypeName;
}

bool SaveDataItemVisualizer::IsInherit(const char *typeName) const
{
	if (typeName == kSaveDataItemVisualizerTypeName ||
	    (typeName != NULL && sce_paf_strcmp(typeName, kSaveDataItemVisualizerTypeName) == 0))
		return true;
	return FileListVisualizer::IsInherit(typeName);
}

const char *SaveDataItemVisualizer::GetTypeName(void) const
{
	return kSaveDataItemVisualizerTypeName;
}

void SaveDataItemVisualizer::PositionTitleWidget(ui::Widget *titleWidget, bool hasDetails)
{
	if (titleWidget == NULL)
		return;
	math::v4 pos;
	if (hasDetails) {
		float posY = (m_lineCount > 1u) ? 11.0f : 25.0f;
		pos = math::v4(-150.0f, posY, 0.0f, 0.0f);
	} else {
		pos = math::v4(-150.0f, 0.0f, 0.0f, 0.0f);
	}
	titleWidget->SetPos(pos);
}

void SaveDataItemVisualizer::RequestCellIcon(ui::Widget *iconWidget,
	const SaveDataListEntry &entry)
{
	if (iconWidget == NULL || m_plugin == NULL)
		return;

	common::SharedPtr<SaveDataImageRequest> req(new SaveDataImageRequest());
	req->path = entry.iconPath;
	req->buffer = entry.iconBuf;
	req->bufferSize = entry.iconBufSize;
	req->state = 0;

	IDParam fallbackId(kDefaultIconTextureHash);
	intrusive_ptr<graph::Surface> fallback = m_plugin->GetTexture(fallbackId);

	Job_ImageDecode *job = new Job_ImageDecode(req, iconWidget, -1, fallback,
		PluginJobQueue::Instance());
	if (job != NULL) {
		SaveDataImageDecodeOption opt;
		sce_paf_memset(&opt, 0, sizeof(opt));
		opt.maxWidth = 160;
		opt.maxHeight = 90;
		opt.scaleMode = (MsgDialogGetSdkLevel() >= 5) ? 1 : 2;
		job->SetOption(opt);
		job->Enqueue();
	}
}

void SaveDataItemVisualizer::UpdateCell(ui::Widget *cellWidget, SceSize index)
{
	if (cellWidget == NULL || m_slotList.get() == NULL)
		return;
	const SaveDataListEntry *entry = m_slotList->GetEntry(index);
	if (entry == NULL)
		return;

	SetRootWidget(cellWidget);
	IDParam titleId(kCellTitleWidgetHash);
	IDParam emptyTitleId(kCellEmptyTitleWidgetHash);
	IDParam dateId(kCellDateWidgetHash);
	IDParam subTitleId(kCellSubTitleWidgetHash);
	IDParam iconId(kCellIconWidgetHash);

	if (entry->existState == 1) {
		SetItemText(titleId, entry->title, -1);
		SetItemText(emptyTitleId, L"", -1);
		wstring dateText;
		SaveDataFormatModifiedTime(&dateText, entry->modifiedTime);
		SetItemText(dateId, dateText, -1);
		SetItemText(subTitleId, entry->subTitle, -1);
	} else {
		wstring emptyTitle = entry->title;
		if (emptyTitle.empty() && m_plugin != NULL) {
			const wchar_t *resStr = m_plugin->GetString(IDParam(kEmptySlotStringHash));
			if (resStr != NULL)
				emptyTitle = resStr;
		}
		SetItemText(emptyTitleId, emptyTitle, -1);
		SetItemText(titleId, L"", -1);
		SetItemText(dateId, L"", -1);
		SetItemText(subTitleId, L"", -1);
	}

	ui::Widget *titleWidget = cellWidget->FindChild(titleId, 0);
	PositionTitleWidget(titleWidget, entry->existState == 1);

	ui::Widget *iconWidget = cellWidget->FindChild(iconId, 0);
	RequestCellIcon(iconWidget, *entry);
}

SaveDataListView::SaveDataListView(Plugin *plugin, ui::Widget *parentPage)
	: m_plugin(plugin)
	, m_parentPage(parentPage)
	, m_listWidget(NULL)
	, m_furlStack(false)
	, m_selectCb(NULL)
	, m_selectUserData(NULL)
	, m_viewSerial(0)
	, m_initialized(SCE_FALSE)
{
}

SaveDataListView::~SaveDataListView()
{
	Close();
}

string SaveDataListView::BuildViewWidgetName(void)
{
	char nameBuf[64];
	sce_paf_snprintf(nameBuf, sizeof(nameBuf), "view_%p_%p_%p_%u",
		this, m_plugin, m_parentPage, m_viewSerial++);
	return string(nameBuf);
}

SceInt32 SaveDataListView::BindSlotList(
	const common::SharedPtr<SaveDataSlotList> &slotList,
	SaveDataListSelectCallback selectCb, void *userData)
{
	m_slotList = slotList;
	m_selectCb = selectCb;
	m_selectUserData = userData;

	if (m_plugin == NULL || m_parentPage == NULL || slotList.get() == NULL)
		return SAVEDATA_DIALOG_ERROR_INTERNAL;

	if (m_listWidget == NULL) {
		m_segmentSource = common::SharedPtr<BasicSegmentSource>(new BasicSegmentSource());
		m_visualizer = common::SharedPtr<SaveDataItemVisualizer>(
			new SaveDataItemVisualizer(m_plugin, slotList));

		common::SharedPtr<BasicCellSource> cellSource(new BasicCellSource());
		cellSource->SetCellCount((SceUInt32)slotList->GetCount(), 3);
		m_segmentSource->SetSegment(0, cellSource);

		ui::Widget *container = m_parentPage->FindChild(IDParam(kListContainerHash), 0);
		if (container == NULL)
			container = m_parentPage;

		string viewName = BuildViewWidgetName();
		IDParam widgetId(viewName.c_str());
		IDParam styleId((uint32_t)0);
		m_listWidget = m_plugin->CreateWidget(container, "file_list",
			widgetId, styleId);
		if (m_listWidget != NULL) {
			m_listWidget->SetSize(math::v4(960.0f, 448.0f, 0.0f, 0.0f));
			m_listWidget->SetPos(math::v4(0.0f, -48.0f, 0.0f, 0.0f));
			m_furlStack.Push(m_listWidget);
		}
		m_initialized = SCE_TRUE;
	}
	return SCE_OK;
}

void SaveDataListView::Show(void)
{
	if (!m_initialized || m_listWidget == NULL)
		return;
	if (!m_listWidget->m_transition_complete) {
		m_listWidget->m_transition = 0xff;
		m_listWidget->m_transition_complete = true;
		m_listWidget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(m_listWidget, NULL, 2, 0.0f, NULL);
}

void SaveDataListView::Hide(void)
{
	if (!m_initialized || m_listWidget == NULL)
		return;
	m_listWidget->m_transition_complete = false;
	m_listWidget->m_transition = 0xff;
	m_listWidget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(m_listWidget, NULL, 2, 0.0f, NULL);
}

void SaveDataListView::FocusIndex(SceInt32 index)
{
	(void)index;
}

void SaveDataListView::Refresh(void)
{
	if (!m_initialized || m_listWidget == NULL || m_visualizer.get() == NULL ||
	    m_slotList.get() == NULL)
		return;
	if (m_slotList->GetCount() > 0)
		m_visualizer->UpdateCell(m_listWidget, 0);
}

void SaveDataListView::Close(void)
{
	m_furlStack.Clear();
	m_listWidget = NULL;
	m_visualizer = common::SharedPtr<SaveDataItemVisualizer>();
	m_segmentSource = common::SharedPtr<BasicSegmentSource>();
	m_slotList = common::SharedPtr<SaveDataSlotList>();
	m_initialized = SCE_FALSE;
}
