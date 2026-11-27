/*
 * paf::ui::MapBackgroundLoader implementation for 2014 PAF JsDocument.
 */
#include <widget/map_bg_loader.h>
#include <widget/wutil.h>
#include <std/string>

namespace paf {
namespace ui {

MapBackgroundLoader::MapBackgroundLoader()
	: m_surface(), m_status(0)
{
}

int32_t MapBackgroundLoader::Load()
{
	string vs0_path("vs0:app/NPXS10017/etc/contents/mapview/image/bg/map_background.png");

	if (wutil::debug::LoadPngTexture(vs0_path, m_surface) != -1)
		return 0;

	string host_path("host0:vs0/app/NPXS10017/etc/contents/mapview/image/bg/map_background.png");

	if (wutil::debug::LoadPngTexture(host_path, m_surface) != -1)
		return 0;

	return 17;
}

intrusive_ptr<graph::Surface> MapBackgroundLoader::GetSurface()
{
	if (m_surface.get() != NULL)
		return m_surface;
	return intrusive_ptr<graph::Surface>();
}

}
}
