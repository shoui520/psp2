/*
 * SceSaveDataDialogPlugin: SaveDataItemVisualizer and SaveDataListView.
 */
#ifndef SAVEDATA_LIST_VIEW_H
#define SAVEDATA_LIST_VIEW_H

#include <scetypes.h>
#include <std/memory>
#include <plugin.h>
#include <widget/core/widget.h>
#include "../companion_dialog/src/furl_stack.h"
#include "filelist_view.h"
#include "savedata_job.h"

typedef void (*SaveDataListSelectCallback)(SceSize selectedIndex, void *userData);

class SaveDataItemVisualizer : public FileListVisualizer {
public:
	SaveDataItemVisualizer(paf::Plugin *plugin,
		const paf::common::SharedPtr<SaveDataSlotList> &slotList);
	virtual ~SaveDataItemVisualizer();

	virtual bool IsInherit(const char *typeName) const;
	virtual const char *GetTypeName(void) const;

	void UpdateCell(paf::ui::Widget *cellWidget, SceSize index);
	void PositionTitleWidget(paf::ui::Widget *titleWidget, bool hasDetails);
	void RequestCellIcon(paf::ui::Widget *iconWidget, const SaveDataListEntry &entry);

	static const char *TypeName(void);

private:
	paf::Plugin *m_plugin;
	paf::common::SharedPtr<SaveDataSlotList> m_slotList;
	SceUInt32 m_lineCount;
};

class SaveDataListView {
public:
	SaveDataListView(paf::Plugin *plugin, paf::ui::Widget *parentPage);
	~SaveDataListView();

	SceInt32 BindSlotList(const paf::common::SharedPtr<SaveDataSlotList> &slotList,
		SaveDataListSelectCallback selectCb, void *userData);
	void Show(void);
	void Hide(void);
	void FocusIndex(SceInt32 index);
	void Refresh(void);
	void Close(void);

	paf::ui::Widget *GetListWidget(void) const { return m_listWidget; }

private:
	paf::string BuildViewWidgetName(void);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_parentPage;
	paf::ui::Widget *m_listWidget;
	paf::common::SharedPtr<SaveDataSlotList> m_slotList;
	paf::common::SharedPtr<BasicSegmentSource> m_segmentSource;
	paf::common::SharedPtr<SaveDataItemVisualizer> m_visualizer;
	FurlStack m_furlStack;
	SaveDataListSelectCallback m_selectCb;
	void *m_selectUserData;
	SceUInt32 m_viewSerial;
	SceBool m_initialized;
};

#endif /* SAVEDATA_LIST_VIEW_H */
