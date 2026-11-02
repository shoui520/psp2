/*
 * The text threads: each takes the layouts off the text queue and lays them out with its font
 * rasterizers into the text surface cache (names ours, after Sony's thread name
 * ScePafTextThread%d). Not in C yet.
 */
#ifndef PAF_GRAPHICS_TEXT_TEXT_THREAD_H
#define PAF_GRAPHICS_TEXT_TEXT_THREAD_H

#include <stddef.h>
#include <thread/thread.h>

namespace paf {
namespace graph {

class FontRasterizer;
class TextQueue;
class TextSurfaceCache;

class TextThread : public thread::Thread {
public:
	TextThread(int priority, size_t stack_size, const char *name, const Option *option,
		   TextQueue *queue, FontRasterizer *rasterizer, FontRasterizer *emoji_rasterizer,
		   TextSurfaceCache *cache);
	virtual ~TextThread();
	virtual void EntryFunction();

	TextQueue *m_queue;
	FontRasterizer *m_rasterizer;
	FontRasterizer *m_emoji_rasterizer;
	TextSurfaceCache *m_cache;
};

}
}

#endif
