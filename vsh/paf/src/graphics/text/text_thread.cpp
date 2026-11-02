/*
 * paf::graph::TextThread (the file's name ours): a text thread lays out the layouts it takes off
 * the text queue, with its rasterizers, into the text surface cache, until it is cancelled.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/text/font_rasterizer.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_thread.h>
#include <kernel/threadmgr.h>
#include <thread/thread.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

/* the priority a text thread lays out at, once its rasterizer is ready (name ours) */
#define TEXT_THREAD_LAYOUT_PRIORITY	(SCE_KERNEL_DEFAULT_PRIORITY_USER - 10)

TextThread::TextThread(int priority, size_t stack_size, const char *name, const Option *option,
		       TextQueue *queue, FontRasterizer *rasterizer,
		       FontRasterizer *emoji_rasterizer, TextSurfaceCache *cache)
	: thread::Thread(priority, stack_size, name, option), m_queue(queue),
	  m_rasterizer(rasterizer), m_emoji_rasterizer(emoji_rasterizer), m_cache(cache)
{
}

TextThread::~TextThread()
{
}

void TextThread::EntryFunction()
{
	if (m_queue == NULL)
		return;

	m_rasterizer->Update();
	ChangePriority(TEXT_THREAD_LAYOUT_PRIORITY);
	while (!IsCanceled()) {
		common::SharedPtr<TextLayout> layout;

		if (m_queue->Pop(layout) == SCE_PAF_OK)
			layout->Update(m_rasterizer, m_emoji_rasterizer, m_cache);
	}
}

}
}
