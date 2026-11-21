/*
 * The tile backend: the page's process (SceWebKitProcess) draws the page's tiles into a pool of
 * memory this module allocates for it, and queues the drawing of them into the view's render
 * target as commands in a buffer both share ("SceTilebackend..." are the module's memory
 * blocks). The exports that set it up and take it down have no Sony name
 * (ScePafWebMapView_<NID>); the other names are ours.
 */
#ifndef WEBVIEW_TILEBACKEND_H
#define WEBVIEW_TILEBACKEND_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/sysmem/memblock.h>
#include <gxm.h>

/* the size of the tiles, in pixels */
struct TilebackendTileSize {
	TilebackendTileSize() : m_width(0), m_height(0)
	{
	}

	TilebackendTileSize(int32_t width, int32_t height) : m_width(width), m_height(height)
	{
	}

	int32_t m_width;
	int32_t m_height;
};

/* the memory the page's process shares with the view, named after the process: the pool its
   tiles are drawn in ("<name><process ID>Pool") and the command buffer ("...Svr") */
class TilebackendParam {
public:
	TilebackendParam(const char *name, SceSize poolSize, SceKernelMemBlockType poolType,
		TilebackendTileSize tileSize);
	~TilebackendParam();

	char m_poolName[32];
	SceSize m_poolSize;
	char m_serverName[32];
	SceKernelMemBlockType m_poolType;
	TilebackendTileSize m_tileSize;
};

/* a texture the page's process drew: its size, its format (1: RGB, else ARGB) and where it is
   (in a command: its offset in the pool plus 0x1000, 0: none) */
struct TilebackendTexture {
	SceUInt32 m_width;
	SceUInt32 m_height;
	SceUInt32 m_stride;
	SceUInt32 m_format;
	void *m_base;
	SceUInt32 m_reserved;
};

/* the commands the page's process queues: a header, then the command's own fields */
struct TilebackendCommandHeader {
	SceUInt32 m_size;			/* the whole command's, 8 to 0x8000 bytes */
	SceUInt32 m_type;			/* TILEBACKEND_COMMAND_* */
};

enum {
	TILEBACKEND_COMMAND_BEGIN_SCENE = 1,	/* a WebViewRenderTarget */
	TILEBACKEND_COMMAND_END_SCENE = 2,
	TILEBACKEND_COMMAND_DRAW_TEXTURE = 3,	/* TilebackendDrawTexture */
	TILEBACKEND_COMMAND_FILL = 4,		/* TilebackendDrawColor */
	TILEBACKEND_COMMAND_OUTLINE = 5,	/* TilebackendDrawColor, drawn as lines */
	TILEBACKEND_COMMAND_CLIP = 6		/* TilebackendClip */
};

struct TilebackendDrawTexture {
	TilebackendCommandHeader m_header;
	TilebackendTexture m_texture;
	float m_world[16];
	float m_opacity;
};

struct TilebackendDrawColor {
	TilebackendCommandHeader m_header;
	float m_world[16];
	float m_color[4];
};

/* the rectangle drawn in (x, y, width, height), when m_enabled */
struct TilebackendClip {
	TilebackendCommandHeader m_header;
	float m_rect[4];
	bool m_enabled;
};

/* the command buffer the page's process writes */
#define TILEBACKEND_COMMAND_BUFFER_SIZE	0x8000

/* the parameter of the pool (tilebackend_pool.cpp), which the view sends the page with its
   create message */
extern "C" TilebackendParam *ScePafWebMapView_C2581D7F(void);
/* the parameter set from the process's name, the pool's size and type and the tile size */
extern "C" void ScePafWebMapView_DE7C5CBE(const char *name, SceSize poolSize,
	SceKernelMemBlockType poolType, TilebackendTileSize tileSize);
/* the pool allocated and mapped for the GPU, and freed */
extern "C" int ScePafWebMapView_6E0719FB(TilebackendParam *param);
extern "C" int ScePafWebMapView_047488BB(TilebackendParam *param);
/* where the parameter's pool is, NULL: none */
void *GetTilebackendPoolBase(TilebackendParam *param);

/* the command buffer opened for the parameter's server, and closed (tilebackend.cpp) */
extern "C" int ScePafWebMapView_273F200C(TilebackendParam *param);
extern "C" int ScePafWebMapView_46215CAD(void);
/* the renderer set up on the GXM context, and taken down */
extern "C" int ScePafWebMapView_EE99BD02(SceGxmContext **context);
extern "C" int ScePafWebMapView_DF9BB931(void);

#endif
