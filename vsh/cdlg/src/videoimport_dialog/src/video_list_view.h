/*
 * SceVideoImportDialogPlugin: video list view, top/item/content views,
 * and visualizers (video_top_view, video_item_view, video_content_view,
 * video_list_view).
 */
#ifndef VIDEOIMPORT_VIDEO_LIST_VIEW_H
#define VIDEOIMPORT_VIDEO_LIST_VIEW_H

#include <scetypes.h>
#include <std/string.h>
#include <std/vector>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <common/job_slots.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include "../../savedata_dialog/filelist_view.h"
#include "video_db.h"

typedef void (*VideoSelectNotify)(SceInt64 mrid, void *userData);

enum VideoViewKind {
	VIDEO_VIEW_TOP = 0,
	VIDEO_VIEW_ITEM,
	VIDEO_VIEW_CONTENT,
	VIDEO_VIEW_LIST
};

class VideoCellSource : public BasicCellSource {
public:
	explicit VideoCellSource(VideoDBManager *dbManager);
	virtual ~VideoCellSource();

	void SetRecords(const paf::vector<VideoRecordInfo> &records);
	const VideoRecordInfo *GetRecord(SceUInt32 index) const;
	SceUInt32 GetCount() const;

private:
	VideoDBManager *m_dbManager;
	paf::vector<VideoRecordInfo> m_records;
};

class VideoItemVisualizer : public FileListVisualizer {
public:
	VideoItemVisualizer(paf::Plugin *plugin, paf::common::JobSlots *thumbSlots);
	virtual ~VideoItemVisualizer();

	virtual paf::ui::Widget *CreateCellWidget(paf::ui::Widget *parent, SceUInt32 index);
	virtual void BindCellWidget(paf::ui::Widget *cell, SceUInt32 index);

	void SetSource(VideoCellSource *source);

private:
	paf::Plugin *m_plugin;
	paf::common::JobSlots *m_thumbSlots;
	VideoCellSource *m_source;
};

class VideoListView {
public:
	explicit VideoListView(paf::Plugin *plugin);
	~VideoListView();

	void SetDisplayOption(SceInt32 drawPriority, const paf::math::v4 &bgColor);
	void SetSelectionOption(SceInt32 selectMode, SceUInt32 visibleCategory,
				SceUInt32 filterType);
	void SetSelectCallback(VideoSelectNotify callback, void *userData);
	void SetDBManager(VideoDBManager *dbManager);

	SceInt32 Open();
	void Close();
	void ShowCategory(VideoViewKind kind, const paf::string &titleIdFilter);

	SceInt64 GetSelectedMrid() const;
	static const char *ViewKindName(VideoViewKind kind);

private:
	paf::Plugin *m_plugin;
	VideoDBManager *m_dbManager;
	paf::ui::Scene *m_scene;
	paf::ui::Widget *m_listWidget;
	paf::common::JobSlots m_thumbSlots;
	VideoCellSource *m_cellSource;
	VideoItemVisualizer *m_visualizer;
	VideoSelectNotify m_selectCallback;
	void *m_selectUserData;
	SceInt32 m_drawPriority;
	paf::math::v4 m_bgColor;
	SceInt32 m_selectMode;
	SceUInt32 m_visibleCategory;
	SceUInt32 m_filterType;
	VideoViewKind m_currentView;
	SceInt64 m_selectedMrid;
	bool m_opened;
};

#endif /* VIDEOIMPORT_VIDEO_LIST_VIEW_H */
