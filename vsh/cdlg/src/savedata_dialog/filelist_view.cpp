/*
 * SceSaveDataDialogPlugin: Sony's file_list visualizer, segment source, cell source, and image job.
 */
#include <string.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <plugin.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include "filelist_view.h"

using namespace paf;

static const char kBasicVisualizerTypeName[] = "basic_visualizer";
static const char kNullCellSourceTypeName[] = "null_cell_source";
static const char kBasicSegmentSourceTypeName[] = "basic_segment_source";

FileListVisualizer::FileListVisualizer()
	: m_rootWidget(NULL)
{
}

FileListVisualizer::~FileListVisualizer()
{
}

const char *FileListVisualizer::TypeName(void)
{
	return kBasicVisualizerTypeName;
}

bool FileListVisualizer::IsInherit(const char *typeName) const
{
	return (typeName == kBasicVisualizerTypeName ||
		(typeName != NULL && sce_paf_strcmp(typeName, kBasicVisualizerTypeName) == 0));
}

const char *FileListVisualizer::GetTypeName(void) const
{
	return kBasicVisualizerTypeName;
}

void FileListVisualizer::SetItemText(const IDParam &partId, const wstring &text,
	SceInt32 lineLimit)
{
	(void)lineLimit;
	if (m_rootWidget == NULL)
		return;
	ui::Widget *child = m_rootWidget->FindChild(partId, 0);
	if (child != NULL)
		child->SetString(text);
}

void FileListVisualizer::ApplyCommonShadow(Plugin *plugin, ui::Widget *parent)
{
	if (plugin == NULL || parent == NULL)
		return;
	IDParam templateId("filelist_template_common_shadow");
	IDParam widgetId("filelist_widget_common_shadow");
	IDParam bodyId("filelist_widget_common_shadow_body");
	Plugin::TemplateOpenParam openParam;
	plugin->TemplateOpen(parent, templateId, openParam);
	ui::Widget *shadow = parent->FindChild(widgetId, 0);
	if (shadow != NULL)
		(void)shadow->FindChild(bodyId, 0);
}

void FileListVisualizer::ApplyCommonCheckBox(Plugin *plugin, ui::Widget *parent)
{
	if (plugin == NULL || parent == NULL)
		return;
	IDParam templateId("filelist_template_common_check_box");
	IDParam widgetId("filelist_widget_common_check_box");
	IDParam bodyId("filelist_widget_common_check_box_body");
	Plugin::TemplateOpenParam openParam;
	plugin->TemplateOpen(parent, templateId, openParam);
	ui::Widget *checkBox = parent->FindChild(widgetId, 0);
	if (checkBox != NULL)
		(void)checkBox->FindChild(bodyId, 0);
}

BasicCellSource::BasicCellSource()
	: m_cellCount(0)
	, m_displayMode(0)
{
}

BasicCellSource::~BasicCellSource()
{
}

const char *BasicCellSource::TypeName(void)
{
	return kNullCellSourceTypeName;
}

bool BasicCellSource::IsInherit(const char *typeName) const
{
	return (typeName == kNullCellSourceTypeName ||
		(typeName != NULL && sce_paf_strcmp(typeName, kNullCellSourceTypeName) == 0));
}

const char *BasicCellSource::GetTypeName(void) const
{
	return kNullCellSourceTypeName;
}

void BasicCellSource::SetCellCount(SceUInt32 count, SceInt32 mode)
{
	m_cellCount = count;
	m_displayMode = mode;
}

BasicSegmentSource::BasicSegmentSource()
{
}

BasicSegmentSource::~BasicSegmentSource()
{
	m_segments.clear();
}

const char *BasicSegmentSource::TypeName(void)
{
	return kBasicSegmentSourceTypeName;
}

bool BasicSegmentSource::IsInherit(const char *typeName) const
{
	return (typeName == kBasicSegmentSourceTypeName ||
		(typeName != NULL && sce_paf_strcmp(typeName, kBasicSegmentSourceTypeName) == 0));
}

const char *BasicSegmentSource::GetTypeName(void) const
{
	return kBasicSegmentSourceTypeName;
}

void BasicSegmentSource::SetSegment(SceInt32 segmentIndex,
	const common::SharedPtr<BasicCellSource> &source)
{
	if (segmentIndex < 0)
		return;
	SceSize needed = (SceSize)segmentIndex + 1;
	while (m_segments.size() < needed)
		m_segments.push_back(common::SharedPtr<BasicCellSource>());
	m_segments[(SceSize)segmentIndex] = source;
}

SceUInt32 BasicSegmentSource::GetSegmentCount(void) const
{
	return (SceUInt32)m_segments.size();
}

BasicImageJob::BasicImageJob(const string &path, ui::Widget *targetWidget,
	graph::SurfacePool *pool)
	: job::JobItem("BasicImageJob", NULL)
	, m_path(path)
	, m_targetWidget(targetWidget)
	, m_surfacePool(pool)
{
}

BasicImageJob::~BasicImageJob()
{
}

SceInt32 BasicImageJob::Run(void)
{
	if (IsCanceled() || m_path.empty())
		return -1;
	SceInt32 err = SCE_OK;
	common::SharedPtr<File> file = LocalFile::Open(m_path.c_str(), SCE_O_RDONLY, 0, &err);
	if (file.get() == NULL || err < 0)
		return -1;
	m_surface = graph::Surface::Load(m_surfacePool, file, NULL);
	return (m_surface.get() != NULL) ? SCE_OK : -1;
}

void BasicImageJob::Finish(SceInt32 result)
{
	if (!IsCanceled() && result == SCE_OK && m_targetWidget != NULL &&
	    m_surface.get() != NULL)
		m_targetWidget->SetTexture(m_surface);
}
