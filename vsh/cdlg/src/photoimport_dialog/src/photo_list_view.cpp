/*
 * ScePhotoImportDialogPlugin: photo list view and category view hierarchy.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <versatileparam/idparam.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include <widget/w_button.h>
#include <common/encoded_string.h>
#include "photo_list_view.h"

using namespace paf;

static const char *const kViewKindNames[] = {
	"photo_top_view",
	"photo_year_view",
	"photo_month_view",
	"photo_grid_view",
	"photo_playlist_view",
	"photo_playlist_grid_view",
	"photo_screenshot_view"
};

PhotoCellSource::PhotoCellSource(PhotoDBManager *dbManager)
	: m_dbManager(dbManager),
	  m_records()
{
}

PhotoCellSource::~PhotoCellSource()
{
}

void PhotoCellSource::SetRecords(const paf::vector<PhotoRecordInfo> &records)
{
	m_records = records;
	SetCellCount(0, (SceUInt32)m_records.size());
}

const PhotoRecordInfo *PhotoCellSource::GetRecord(SceUInt32 index) const
{
	if (index >= (SceUInt32)m_records.size())
		return NULL;
	return &m_records[index];
}

SceUInt32 PhotoCellSource::GetCount() const
{
	return (SceUInt32)m_records.size();
}

PhotoItemVisualizer::PhotoItemVisualizer(Plugin *plugin, PhotoDBManager *dbManager,
					 common::JobSlots *thumbSlots)
	: m_plugin(plugin),
	  m_dbManager(dbManager),
	  m_thumbSlots(thumbSlots),
	  m_source(NULL)
{
}

PhotoItemVisualizer::~PhotoItemVisualizer()
{
}

void PhotoItemVisualizer::SetSource(PhotoCellSource *source)
{
	m_source = source;
}

ui::Widget *PhotoItemVisualizer::CreateCellWidget(ui::Widget *parent, SceUInt32 index)
{
	(void)index;
	if (m_plugin == NULL || parent == NULL)
		return NULL;
	Plugin::TemplateOpenParam openParam;
	m_plugin->TemplateOpen(parent, IDParam("filelist_template_photo_item"), openParam);
	return parent->FindChild(IDParam("filelist_widget_photo_item"), 0);
}

void PhotoItemVisualizer::BindCellWidget(ui::Widget *cell, SceUInt32 index)
{
	if (cell == NULL || m_source == NULL)
		return;
	const PhotoRecordInfo *rec = m_source->GetRecord(index);
	if (rec == NULL)
		return;

	ui::Widget *button = cell->FindChild(IDParam("filelist_widget_photo_item_button"), 0);
	if (button != NULL && m_dbManager != NULL && m_thumbSlots != NULL) {
		ScePhotoThumbJob *thumbJob =
			new ScePhotoThumbJob(m_dbManager, rec->mrid, 2, button);
		if (thumbJob != NULL) {
			common::SharedPtr<job::JobItem> item(thumbJob);
			m_thumbSlots->Enqueue(0, job::JobQueue::default_queue, item);
		}
	}
}

PhotoListView::PhotoListView(Plugin *plugin)
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
	  m_visibleCategory(0x7u),
	  m_currentView(PHOTO_VIEW_TOP),
	  m_selectedMrid(0),
	  m_opened(false)
{
	m_thumbSlots.Grow(1);
}

PhotoListView::~PhotoListView()
{
	Close();
}

const char *PhotoListView::ViewKindName(PhotoViewKind kind)
{
	SceUInt32 count = (SceUInt32)(sizeof(kViewKindNames) / sizeof(kViewKindNames[0]));
	if ((SceUInt32)kind >= count)
		return "photo_list_view";
	return kViewKindNames[kind];
}

void PhotoListView::SetDisplayOption(SceInt32 drawPriority, const math::v4 &bgColor)
{
	m_drawPriority = drawPriority;
	m_bgColor = bgColor;
}

void PhotoListView::SetSelectionOption(SceInt32 selectMode, SceUInt32 visibleCategory)
{
	m_selectMode = selectMode;
	m_visibleCategory = visibleCategory;
}

void PhotoListView::SetSelectCallback(PhotoSelectNotify callback, void *userData)
{
	m_selectCallback = callback;
	m_selectUserData = userData;
}

void PhotoListView::SetDBManager(PhotoDBManager *dbManager)
{
	m_dbManager = dbManager;
}

SceInt32 PhotoListView::Open()
{
	if (m_opened || m_plugin == NULL)
		return SCE_OK;

	m_cellSource = new PhotoCellSource(m_dbManager);
	m_visualizer = new PhotoItemVisualizer(m_plugin, m_dbManager, &m_thumbSlots);
	if (m_visualizer != NULL)
		m_visualizer->SetSource(m_cellSource);

	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = m_drawPriority;
	m_scene = m_plugin->PageOpen(IDParam("photo_list_base"), openParam);
	if (m_scene != NULL) {
		ui::Widget *bgPlane = m_scene->FindChild(IDParam("plane"), 0);
		if (bgPlane != NULL)
			bgPlane->SetColor(m_bgColor);
		m_listWidget = m_scene->FindChild(IDParam("file_list"), 0);
	}

	m_opened = true;
	ShowCategory(PHOTO_VIEW_TOP, paf::string());
	return SCE_OK;
}

void PhotoListView::Close()
{
	if (!m_opened)
		return;
	m_thumbSlots.Cancel(0);
	if (m_plugin != NULL && m_scene != NULL) {
		Plugin::PageCloseParam closeParam;
		m_plugin->PageClose(IDParam("photo_list_base"), closeParam);
		m_scene = NULL;
	}
	delete m_visualizer;
	m_visualizer = NULL;
	delete m_cellSource;
	m_cellSource = NULL;
	m_listWidget = NULL;
	m_opened = false;
}

void PhotoListView::ShowCategory(PhotoViewKind kind, const paf::string &filterValue)
{
	m_currentView = kind;
	if (m_dbManager == NULL || m_cellSource == NULL)
		return;

	paf::string whereClause = "content_type='image' AND supported_version < '1800'";
	if (kind == PHOTO_VIEW_SCREENSHOT) {
		whereClause += " AND genre_type='screenshot'";
		if (!filterValue.empty()) {
			whereClause += " AND appli_title='";
			whereClause += filterValue.c_str();
			whereClause += "'";
		}
	} else if (kind == PHOTO_VIEW_MONTH || kind == PHOTO_VIEW_YEAR) {
		if (!filterValue.empty()) {
			whereClause += " AND date(created_time) LIKE '";
			whereClause += filterValue.c_str();
			whereClause += "'";
		}
	}

	paf::vector<PhotoRecordInfo> records;
	m_dbManager->QueryPhotoList(whereClause, PHOTO_SORT_CREATED_TIME_DESC, 0, 0, records);
	m_cellSource->SetRecords(records);
}

SceInt64 PhotoListView::GetSelectedMrid() const
{
	return m_selectedMrid;
}
