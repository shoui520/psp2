/*
 * applauncher.h: what the applauncher plugin's files give each other. The plugin's parts are
 * made at its start and taken down at its end; each part's functions live in its own file. Names
 * ours (PLACEHOLDER): no record or string of the 3.6x shell names them.
 */
#ifndef _APPLAUNCHER_H
#define _APPLAUNCHER_H

#include <scetypes.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <graphics/surface/surface.h>
#include <math/ngp/v4.h>

/* the named plugin's GetInterface(id), NULL without the plugin (0x810C9D14) */
void *applauncherPluginGetInterface(const char *name, int id);

/* the plugin's function table set as its GetInterface(1), the plugin kept (0x810C9C7C) */
void applauncherInterfaceRegister(paf::Plugin *plugin);
/* PLACEHOLDER names: the plugin's private heap kept (0x810C9BF0, applauncher_heap.cpp); nothing
   in this build reads it (0x81555B18) */
void applauncherHeapSet(void);
extern paf::memory::HeapAllocator *g_applauncherHeap;
/* the plugin's function table, once registered (0x81555B40); the home fills it */
struct ApplauncherInterface {
	void (*func[28])(void);
};
extern ApplauncherInterface *g_applauncherInterface;
/* the plugin, as registered (0x81555B44) */
extern paf::Plugin *g_applauncherPlugin;
/* the plugin's string of the ID (0x810C9D9C) */
const wchar_t *applauncherGetString(const paf::IDParam &id);

/* the DataCacheManager made and taken down (0x810BA558, 0x810BA56E) */
void applauncherDataCacheCreate(paf::Plugin *plugin);
void applauncherDataCacheDestroy(void);
/* PLACEHOLDER: room made in the DataCacheManager's surface pool (0x810BA678) */
void applauncherDataCacheMakeSpace(void);
/* the IconCacheManager made and taken down (0x810BD35C, 0x810BD372) */
void applauncherIconCacheCreate(paf::Plugin *plugin);
void applauncherIconCacheDestroy(void);
/* empty in this build: called at the plugin's end after the caches (folded into one empty
   function) */
void applauncherExitReserved0(void);
void applauncherExitReserved1(void);

/* the colours of the user's own wave (preset 31), from waveparam.bin (0x810C6DF8) */
struct ApplauncherWaveColors {
	paf::math::v4 color[2];
};
void applauncherWaveSetColors(SceUInt32 index, ApplauncherWaveColors colors);
/* PLACEHOLDER: whether the user's own wave colours were given (0x810C6DA0), and a wave's two
   colours by its preset, false for a preset with none (0x810C7D5A) */
bool applauncherWaveColorsSet(void);
bool applauncherWaveGetColors(int preset, paf::math::v4 *color0, paf::math::v4 *color1);

/* PLACEHOLDER: the photo export library's conversion of an image file into a page's texture
   (0x8136D050): what it reads and writes, and the sizes of what it wrote */
#define PHOTO_EXPORT_CONVERT_JPEG	1
#define PHOTO_EXPORT_CONVERT_PNG	2

struct PhotoExportConvertParam {
	const char *srcPath;
	const char *dstPath;
	int reserved;
	int type;			/* PHOTO_EXPORT_CONVERT_* */
};

struct PhotoExportConvertResult {
	int imageWidth;
	int imageHeight;
	int texWidth;
	int texHeight;
};

int photoExportConvertFile(const PhotoExportConvertParam *param, void *workMemory,
			   SceBool (*cancelFunc)(void *), void *userdata,
			   PhotoExportConvertResult *result);

/*
 * pd0:wave/waveparam.bin: a paf::graph::WavePreset as the wave editor wrote it (672 bytes), its
 * head read here: the preset's index and the eight words paf's header calls m_reserved1, the
 * user's wave colours. Read as the file, so the file does not take wave_renderer.h's headers
 * (and color.h's static colours) with it.
 */
struct __attribute__((aligned(16))) ApplauncherWaveParamFile {
	SceUInt32 magic;
	SceUInt32 version;
	SceUInt32 index;
	SceUInt32 reserved;
	ApplauncherWaveColors colors;
	SceUInt8 preset[672 - 48];
};

/* the icon capture (SceShellIconCaptureManager) made on the pool and taken down (0x813C34FC,
   0x813C3514) */
void applauncherIconCaptureCreate(paf::graph::SurfacePool *pool);
void applauncherIconCaptureDestroy(void);
/* the page made and taken down (0x810A96EE, 0x810A9706) */
void applauncherPageCreate(paf::Plugin *plugin);
void applauncherPageDestroy(void);
/* PLACEHOLDER: the pool of page views (three, and one for a folder) made and taken down
   (0x810C480E, 0x810C49B4) */
void applauncherPageViewPoolCreate(void);
void applauncherPageViewPoolDestroy(void);
/* the icons' pool of 42 made and taken down (0x810BBBC8, 0x810BBCD4) */
void applauncherIconPoolCreate(void);
void applauncherIconPoolDestroy(void);
/* the applauncher's mode manager made, started in its launcher mode, and deleted (0x810C1FE6,
   0x810C2024; applauncher_modemanager.cpp) */
void applauncherModeManagerCreate(void);
void applauncherModeManagerDestroy(void);

/* a theme applied: the icons reloaded at the plugin's init (0x810AF256), and refreshed at its
   start (0x810B17CE) */
int applauncherThemeReload(void);
int applauncherThemeRefresh(void);

#endif
