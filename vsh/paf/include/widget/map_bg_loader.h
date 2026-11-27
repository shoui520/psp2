/*
 * paf::ui::MapBackgroundLoader: loads the tiled map background texture used by
 * 2014 PAF JsDocument.
 */
#ifndef PAF_WIDGET_MAP_BG_LOADER_H
#define PAF_WIDGET_MAP_BG_LOADER_H

#include <stdint.h>
#include <std/memory>
#include <graphics/surface/surface.h>

namespace paf {
namespace ui {

class MapBackgroundLoader {
public:
	MapBackgroundLoader();

	int32_t Load();
	intrusive_ptr<graph::Surface> GetSurface();

	intrusive_ptr<graph::Surface> m_surface;
	uint32_t m_status;
};

}
}

#endif
