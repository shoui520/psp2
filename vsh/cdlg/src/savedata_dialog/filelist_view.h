/*
 * SceSaveDataDialogPlugin: Sony's shared file_list / basic visualizer / cell source classes.
 */
#ifndef SAVEDATA_FILELIST_VIEW_H
#define SAVEDATA_FILELIST_VIEW_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/memory>
#include <math/ngp/v4.h>
#include <graphics/surface/surface.h>
#include <job/job.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>

class FileListVisualizer {
public:
	FileListVisualizer();
	virtual ~FileListVisualizer();

	virtual bool IsInherit(const char *typeName) const;
	virtual const char *GetTypeName(void) const;
	virtual void SetItemText(const paf::IDParam &partId, const paf::wstring &text,
		SceInt32 lineLimit);
	virtual paf::ui::Widget *GetRootWidget(void) const { return m_rootWidget; }
	virtual void SetRootWidget(paf::ui::Widget *widget) { m_rootWidget = widget; }
	virtual void ApplyCommonShadow(paf::Plugin *plugin, paf::ui::Widget *parent);
	virtual void ApplyCommonCheckBox(paf::Plugin *plugin, paf::ui::Widget *parent);

	static const char *TypeName(void);

protected:
	paf::ui::Widget *m_rootWidget;
};

class BasicCellSource {
public:
	BasicCellSource();
	virtual ~BasicCellSource();

	virtual bool IsInherit(const char *typeName) const;
	virtual const char *GetTypeName(void) const;
	virtual SceUInt32 GetCellCount(void) const { return m_cellCount; }
	virtual void SetCellCount(SceUInt32 count, SceInt32 mode);

	static const char *TypeName(void);

protected:
	SceUInt32 m_cellCount;
	SceInt32 m_displayMode;
};

class BasicSegmentSource {
public:
	BasicSegmentSource();
	virtual ~BasicSegmentSource();

	virtual bool IsInherit(const char *typeName) const;
	virtual const char *GetTypeName(void) const;
	virtual void SetSegment(SceInt32 segmentIndex,
		const paf::common::SharedPtr<BasicCellSource> &source);
	virtual SceUInt32 GetSegmentCount(void) const;

	static const char *TypeName(void);

protected:
	paf::vector<paf::common::SharedPtr<BasicCellSource> > m_segments;
};

class BasicImageJob : public paf::job::JobItem {
public:
	BasicImageJob(const paf::string &path, paf::ui::Widget *targetWidget,
		paf::graph::SurfacePool *pool);
	virtual ~BasicImageJob();

	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	paf::string m_path;
	paf::ui::Widget *m_targetWidget;
	paf::graph::SurfacePool *m_surfacePool;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
};

#endif /* SAVEDATA_FILELIST_VIEW_H */
