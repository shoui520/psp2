/*
 * ScePhotoImportDialogPlugin: photo list view and category view hierarchy
 * (photo_top_view, photo_year_view, photo_month_view, photo_grid_view,
 *  photo_playlist_view, photo_playlist_grid_view, photo_screenshot_view).
 */
#ifndef PHOTOIMPORT_PHOTO_LIST_VIEW_H
#define PHOTOIMPORT_PHOTO_LIST_VIEW_H

#include <scetypes.h>
#include <std/string.h>
#include <std/vector>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <common/job_slots.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include "../../savedata_dialog/filelist_view.h"
#include "photo_db.h"

typedef void (*PhotoSelectNotify)(SceInt64 mrid, void *userData);

enum PhotoViewKind {
	PHOTO_VIEW_TOP = 0,
	PHOTO_VIEW_YEAR,
	PHOTO_VIEW_MONTH,
	PHOTO_VIEW_GRID,
	PHOTO_VIEW_PLAYLIST,
	PHOTO_VIEW_PLAYLIST_GRID,
	PHOTO_VIEW_SCREENSHOT
};

class PhotoCellSource : public BasicCellSource {
public:
	PhotoCellSource(PhotoDBManager *dbManager);
	virtual ~PhotoCellSource();

	void SetRecords(const paf::vector<PhotoRecordInfo> &records);
	const PhotoRecordInfo *GetRecord(SceUInt32 index) const;
	SceUInt32 GetCount() const;

private:
	PhotoDBManager *m_dbManager;
	paf::vector<PhotoRecordInfo> m_records;
};

class PhotoItemVisualizer : public FileListVisualizer {
public:
	PhotoItemVisualizer(paf::Plugin *plugin, PhotoDBManager *dbManager,
			    paf::common::JobSlots *thumbSlots);
	virtual ~PhotoItemVisualizer();

	virtual paf::ui::Widget *CreateCellWidget(paf::ui::Widget *parent, SceUInt32 index);
	virtual void BindCellWidget(paf::ui::Widget *cell, SceUInt32 index);

	void SetSource(PhotoCellSource *source);

private:
	paf::Plugin *m_plugin;
	PhotoDBManager *m_dbManager;
	paf::common::JobSlots *m_thumbSlots;
	PhotoCellSource *m_source;
};

class PhotoListView {
public:
	PhotoListView(paf::Plugin *plugin);
	~PhotoListView();

	void SetDisplayOption(SceInt32 drawPriority, const paf::math::v4 &bgColor);
	void SetSelectionOption(SceInt32 selectMode, SceUInt32 visibleCategory);
	void SetSelectCallback(PhotoSelectNotify callback, void *userData);
	void SetDBManager(PhotoDBManager *dbManager);

	SceInt32 Open();
	void Close();
	void ShowCategory(PhotoViewKind kind, const paf::string &filterValue);

	SceInt64 GetSelectedMrid() const;
	static const char *ViewKindName(PhotoViewKind kind);

private:
	paf::Plugin *m_plugin;
	PhotoDBManager *m_dbManager;
	paf::ui::Scene *m_scene;
	paf::ui::Widget *m_listWidget;
	paf::common::JobSlots m_thumbSlots;
	PhotoCellSource *m_cellSource;
	PhotoItemVisualizer *m_visualizer;
	PhotoSelectNotify m_selectCallback;
	void *m_selectUserData;
	SceInt32 m_drawPriority;
	paf::math::v4 m_bgColor;
	SceInt32 m_selectMode;
	SceUInt32 m_visibleCategory;
	PhotoViewKind m_currentView;
	SceInt64 m_selectedMrid;
	bool m_opened;
};

#endif /* PHOTOIMPORT_PHOTO_LIST_VIEW_H */
