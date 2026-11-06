/*
 * The text system (the file's name ours): the PVF library, the text queue, the surface cache
 * the glyphs are drawn into, and a text thread with its font rasterizers, made by InitText
 * and torn down by TermText. The threads stop while the locale changes.
 */
#include <stddef.h>
#include <stdint.h>
#include <framework.h>
#include <kernel/threadmgr.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/text/font_rasterizer.h>
#include <graphics/text/text_init.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_surface_cache.h>
#include <graphics/text/text_thread.h>
#include <std/stdio.h>
#include <std/string>
#include <std/string.h>
#include <thread/thread.h>
#include "../../paf_types.h"

extern "C" int sceAppMgrConvertVs0UserDrivePath(const char *path, char *out, int out_size);

namespace paf {
namespace graph {

/* the text threads, each with its rasterizers (names ours) */
#define TEXT_THREAD_NUM		1
#define TEXT_THREAD_PRIORITY	(SCE_KERNEL_DEFAULT_PRIORITY_USER - 14)
#define TEXT_THREAD_STACK_SIZE	0x2000
#define TEXT_SURFACE_CACHE_SIZE	0x100000

static TextQueue *s_queue = NULL;
static TextSurfaceCache *s_cache = NULL;
static FontRasterizer *s_rasterizers[TEXT_THREAD_NUM] = { NULL };
static FontRasterizer *s_emoji_rasterizers[TEXT_THREAD_NUM] = { NULL };
static TextThread *s_threads[TEXT_THREAD_NUM] = { NULL };
static void *s_mem_emoji_font_addr = NULL;
static uint32_t s_mem_emoji_font_size = 0;
static void *s_cache_memory = NULL;
static GraphMem::DeviceType s_device_type = GraphMem::DeviceType_SystemMemory;

static thread::RMutex s_graphics_mutex("ScePafTextGraphicsMutex", 0);
static thread::RMutex s_layout_mutex("ScePafTextLayoutMutex", 0);
static string s_emoji_font_path("");

TextInitParam::TextInitParam()
{
	text_surface_pool_size = 0;
	pvf_heap_size = 0;
	pvf_option = 0;
	locale = -1;
	mem_pvf_japanese_addr = NULL;
	mem_pvf_japanese_size = 0;
	mem_pvf_latin_addr = NULL;
	mem_pvf_latin_size = 0;
	mem_pvf_hangul_addr = NULL;
	mem_pvf_hangul_size = 0;
	mem_pvf_chinese_addr = NULL;
	mem_pvf_chinese_size = 0;
	mem_pvf_arib_addr = NULL;
	mem_pvf_arib_size = 0;
	path_pvf_japanese = NULL;
	path_pvf_latin = NULL;
	path_pvf_hangul = NULL;
	path_pvf_chinese = NULL;
	path_pvf_arib = NULL;
	mem_emoji_font_addr = NULL;
	path_emoji_font = NULL;
	mem_emoji_font_size = 0;
}

void StopTextThreads()
{
	int32_t i;

	for (i = 0; i < TEXT_THREAD_NUM; i++) {
		if (s_threads[i] != NULL)
			s_threads[i]->Cancel();
	}
	if (s_queue != NULL)
		s_queue->Abort();
	for (i = 0; i < TEXT_THREAD_NUM; i++) {
		if (s_threads[i] != NULL) {
			s_threads[i]->Join();
			delete s_threads[i];
			s_threads[i] = NULL;
		}
		if (s_rasterizers[i] != NULL)
			s_rasterizers[i]->ResetStyle();
	}
	if (s_cache != NULL)
		s_cache->Clear();
}

void TermText()
{
	StopTextThreads();
	for (int32_t i = 0; i < TEXT_THREAD_NUM; i++) {
		if (s_emoji_rasterizers[i] != NULL) {
			delete s_emoji_rasterizers[i];
			s_emoji_rasterizers[i] = NULL;
		}
		if (s_rasterizers[i] != NULL) {
			delete s_rasterizers[i];
			s_rasterizers[i] = NULL;
		}
	}
	if (s_cache != NULL) {
		delete s_cache;
		s_cache = NULL;
	}
	if (s_cache_memory != NULL) {
		GraphMem::FreeOSMemory(s_device_type, s_cache_memory);
		s_cache_memory = NULL;
	}
	if (s_queue != NULL) {
		delete s_queue;
		s_queue = NULL;
	}
	FontRasterizerPvf::Term();
	s_mem_emoji_font_addr = NULL;
	s_mem_emoji_font_size = 0;
	s_emoji_font_path = "";
	s_device_type = GraphMem::DeviceType_SystemMemory;
}

int32_t StartTextThreads()
{
	int32_t ret;

	for (int32_t i = 0; i < TEXT_THREAD_NUM; i++) {
		ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
		if (s_queue != NULL && s_cache != NULL && s_rasterizers[i] != NULL) {
			char name[32];

			sce_paf_snprintf(name, sizeof(name), "ScePafTextThread%d", i);
			s_threads[i] = new TextThread(TEXT_THREAD_PRIORITY, TEXT_THREAD_STACK_SIZE, name,
						      NULL, s_queue, s_rasterizers[i],
						      s_emoji_rasterizers[i], s_cache);
			if (s_threads[i] != NULL)
				ret = s_threads[i]->Start();
		}
		if (ret < 0) {
			StopTextThreads();
			break;
		}
	}
	return ret;
}

FontRasterizer *CreateEmojiFontRasterizer()
{
	if (s_mem_emoji_font_addr != NULL)
		return new FontRasterizerImage(s_mem_emoji_font_addr, s_mem_emoji_font_size);
	if (s_emoji_font_path != "")
		return new FontRasterizerImage(s_emoji_font_path, false);
	return NULL;
}

FontRasterizer *CreateFontRasterizer()
{
	return new FontRasterizerPvf();
}

int32_t InitText(const TextInitParam &param)
{
	int32_t ret;

	s_mem_emoji_font_addr = param.mem_emoji_font_addr;
	s_mem_emoji_font_size = param.mem_emoji_font_size;
	ret = SCE_PAF_ERROR_TEXT_NO_MEMORY;
	if (param.path_emoji_font != NULL && *param.path_emoji_font != '\0') {
		s_emoji_font_path = param.path_emoji_font;
	} else {
		char path[256];

		sceAppMgrConvertVs0UserDrivePath("vs0:data/external/font/emoji/imagefont.bin", path,
						 sizeof(path));
		s_emoji_font_path = path;
	}

	bool shell = false;

	if (Framework::Instance() != NULL)
		shell = Framework::Instance()->GetMode() == Framework::Mode_Shell;
	s_device_type = (GraphMem::DeviceType)shell;

	FontRasterizerPvf::InitParam pvf_param;

	pvf_param.heap_size = param.pvf_heap_size;
	pvf_param.option = param.pvf_option;
	pvf_param.locale = param.locale;
	pvf_param.font.mem_japanese_addr = param.mem_pvf_japanese_addr;
	pvf_param.font.mem_japanese_size = param.mem_pvf_japanese_size;
	pvf_param.font.mem_latin_addr = param.mem_pvf_latin_addr;
	pvf_param.font.mem_latin_size = param.mem_pvf_latin_size;
	pvf_param.font.mem_hangul_addr = param.mem_pvf_hangul_addr;
	pvf_param.font.mem_hangul_size = param.mem_pvf_hangul_size;
	pvf_param.font.mem_chinese_addr = param.mem_pvf_chinese_addr;
	pvf_param.font.mem_chinese_size = param.mem_pvf_chinese_size;
	pvf_param.font.mem_arib_addr = param.mem_pvf_arib_addr;
	pvf_param.font.mem_arib_size = param.mem_pvf_arib_size;
	pvf_param.font.path_japanese = param.path_pvf_japanese;
	pvf_param.font.path_latin = param.path_pvf_latin;
	pvf_param.font.path_hangul = param.path_pvf_hangul;
	pvf_param.font.path_chinese = param.path_pvf_chinese;
	pvf_param.font.path_arib = param.path_pvf_arib;
	FontRasterizerPvf::Init(pvf_param);

	s_queue = new TextQueue();
	if (s_queue != NULL) {
		uint32_t size = param.text_surface_pool_size;

		if (size == 0)
			size = TEXT_SURFACE_CACHE_SIZE;
		s_cache_memory = GraphMem::AllocOSMemory(s_device_type, size, "ScePafTextSurfaceCache",
							 NULL);
		if (s_cache_memory != NULL) {
			s_cache = new TextSurfaceCache(s_cache_memory, size, "ScePafTextSurfaceCache");
			if (s_cache != NULL) {
				for (int32_t i = 0; i < TEXT_THREAD_NUM; i++) {
					s_rasterizers[i] = CreateFontRasterizer();
					s_emoji_rasterizers[i] = CreateEmojiFontRasterizer();
				}
				ret = StartTextThreads();
				if (ret >= 0)
					return ret;
			}
		}
	}
	TermText();
	return ret;
}

void SetTextLocale(int32_t locale)
{
	StopTextThreads();
	FontRasterizerPvf::SetLocale(locale);
	StartTextThreads();
}

thread::RMutex *GetTextGraphicsMutex()
{
	return &s_graphics_mutex;
}

thread::RMutex *GetTextLayoutMutex()
{
	return &s_layout_mutex;
}

TextQueue *GetTextQueue()
{
	return s_queue;
}

}
}
