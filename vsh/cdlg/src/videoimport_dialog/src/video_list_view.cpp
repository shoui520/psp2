/*
 * SceVideoImportDialogPlugin: video list view, visualizers, and category queries.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <versatileparam/idparam.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include <widget/w_button.h>
#include "video_list_view.h"

using namespace paf;

static const char *const kVideoViewNames[] = {
	"video_top_view",
	"video_item_view",
	"video_content_view",
	"video_list_view"
};

VideoCellSource::VideoCellSource(VideoDBManager *dbManager)
	: m_dbManager(dbManager),
	  m_records()
{
}

VideoCellSource::~VideoCellSource()
{
}

void VideoCellSource::SetRecords(const paf::vector<VideoRecordInfo> &records)
{
	m_records = records;
	SetCellCount(0, (SceUInt32)m_records.size());
}

const VideoRecordInfo *VideoCellSource::GetRecord(SceUInt32 index) const
{
	if (index >= (SceUInt32)m_records.size())
		return NULL;
	return &m_records[index];
}

SceUInt32 VideoCellSource::GetCount() const
{
	return (SceUInt32)m_records.size();
}

VideoItemVisualizer::VideoItemVisualizer(Plugin *plugin, common::JobSlots *thumbSlots)
	: m_plugin(plugin),
	  m_thumbSlots(thumbSlots),
	  m_source(NULL)
{
}

VideoItemVisualizer::~VideoItemVisualizer()
{
}

void VideoItemVisualizer::SetSource(VideoCellSource *source)
{
	m_source = source;
}

ui::Widget *VideoItemVisualizer::CreateCellWidget(ui::Widget *parent, SceUInt32 index)
{
	(void)index;
	if (m_plugin == NULL || parent == NULL)
		return NULL;
	Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(parent, IDParam("filelist_template_video_item"), openParam);
	return parent->FindChild(IDParam("filelist_widget_video_item"), 0);
}

void VideoItemVisualizer::BindCellWidget(ui::Widget *cell, SceUInt32 index)
{
	if (cell == NULL || m_source == NULL)
		return;
	const VideoRecordInfo *rec = m_source->GetRecord(index);
	if (rec == NULL)
		return;

	ui::Widget *button = cell->FindChild(IDParam("filelist_widget_video_item_button"), 0);
	if (button != NULL && m_thumbSlots != NULL && !rec->iconPath.empty() && rec->iconSize > 0) {
		VideoThumbnailJob *thumbJob =
			new VideoThumbnailJob(rec->iconPath, rec->iconOffset, rec->iconSize, button);
		if (thumbJob != NULL) {
			common::SharedPtr<job::JobItem> item(thumbJob);
			m_thumbSlots->Enqueue(0, job::JobQueue::default_queue, item);
		}
	}
}

VideoListView::VideoListView(Plugin *plugin)
	: m_plugin(plugin),
	  m_dbManager(NULL),
	  m_scene(NULL),
	  m_listWidget(NULL),
	  m_thumbSlots(),
	  m_cellSource(NULL),
	  m_visualizer(NULL),
	  m_selectCallback(NULL),
	  m_selectUserData(NULL),
	  m_drawPriority(120),
	  m_bgColor(0.149f, 0.157f, 0.157f, 1.0f),
	  m_selectMode(0),
	  m_visibleCategory(0x3u),
	  m_filterType(0),
	  m_currentView(VIDEO_VIEW_TOP),
	  m_selectedMrid(0),
	  m_opened(false)
{
	m_thumbSlots.Grow(1);
}

VideoListView::~VideoListView()
{
	Close();
}

const char *VideoListView::ViewKindName(VideoViewKind kind)
{
	SceUInt32 count = (SceUInt32)(sizeof(kVideoViewNames) / sizeof(kVideoViewNames[0]));
	if ((SceUInt32)kind >= count)
		return "video_list_view";
	return kVideoViewNames[kind];
}

void VideoListView::SetDisplayOption(SceInt32 drawPriority, const math::v4 &bgColor)
{
	m_drawPriority = drawPriority;
	m_bgColor = bgColor;
}

void VideoListView::SetSelectionOption(SceInt32 selectMode, SceUInt32 visibleCategory,
				       SceUInt32 filterType)
{
	m_selectMode = selectMode;
	m_visibleCategory = visibleCategory;
	m_filterType = filterType;
}

void VideoListView::SetSelectCallback(VideoSelectNotify callback, void *userData)
{
	m_selectCallback = callback;
	m_selectUserData = userData;
}

void VideoListView::SetDBManager(VideoDBManager *dbManager)
{
	m_dbManager = dbManager;
}

SceInt32 VideoListView::Open()
{
	if (m_opened || m_plugin == NULL)
		return SCE_OK;

	m_cellSource = new VideoCellSource(m_dbManager);
	m_visualizer = new VideoItemVisualizer(m_plugin, &m_thumbSlots);
	if (m_visualizer != NULL)
		m_visualizer->SetSource(m_cellSource);

	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = m_drawPriority;
	m_scene = m_plugin->PageOpen(IDParam("video_list_base"), openParam);
	if (m_scene != NULL) {
		ui::Widget *bgPlane = m_scene->FindChild(IDParam("plane"), 0);
		if (bgPlane != NULL)
			bgPlane->SetColor(m_bgColor);
		m_listWidget = m_scene->FindChild(IDParam("file_list"), 0);
	}

	m_opened = true;
	ShowCategory(VIDEO_VIEW_TOP, paf::string());
	return SCE_OK;
}

void VideoListView::Close()
{
	if (!m_opened)
		return;
	m_thumbSlots.Cancel(0);
	if (m_plugin != NULL && m_scene != NULL) {
		Plugin::PageCloseParam closeParam;
		m_plugin->PageClose(IDParam("video_list_base"), closeParam);
		m_scene = NULL;
	}
	delete m_visualizer;
	m_visualizer = NULL;
	delete m_cellSource;
	m_cellSource = NULL;
	m_listWidget = NULL;
	m_opened = false;
}

void VideoListView::ShowCategory(VideoViewKind kind, const paf::string &titleIdFilter)
{
	m_currentView = kind;
	if (m_dbManager == NULL || m_cellSource == NULL)
		return;

	paf::string whereClause = "content_type='video' AND drm_string IS NULL";
	if (!titleIdFilter.empty()) {
		whereClause += " AND (appli_title_id ='";
		whereClause += titleIdFilter.c_str();
		whereClause += "')";
	}

	paf::vector<VideoRecordInfo> records;
	m_dbManager->QueryVideoList(whereClause, VIDEO_SORT_TITLE_ASC, 0, 0, records);
	m_cellSource->SetRecords(records);
}

SceInt64 VideoListView::GetSelectedMrid() const
{
	return m_selectedMrid;
}
