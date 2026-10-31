/*
 * paf::Plugin, as far as converted C++ uses it: found by name, a texture of
 * its resource. The full class (src/plugin.h's C struct) moves here when the
 * plugin code is converted.
 */
#ifndef PAF_PLUGIN_H_CXX
#define PAF_PLUGIN_H_CXX

#include <std/string>
#include <std/memory>
#include <versatileparam/idparam.h>

namespace paf {

namespace graph {
class Surface;
class SurfacePool;
}

class Plugin {
public:
	static Plugin *Find(const char *name);

	const char *GetName() const
	{
		return m_name.c_str();
	}

	/* The resource's texture id: the plugin's surface, its texture table, the common ones. */
	intrusive_ptr<graph::Surface> GetTexture(const IDParam &id, graph::SurfacePool *pool);

private:
	bool m_initialized;
	string m_name;
};

}

#endif
