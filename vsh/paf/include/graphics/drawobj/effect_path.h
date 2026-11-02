/*
 * The folders of the effects' resources: the firmware's, and those of a devkit's host tree.
 * Every file that includes this has its own copy (names ours; Sony's object files of the
 * draw objects and effects each construct these eight strings).
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_EFFECT_PATH_H
#define PAF_GRAPHICS_DRAWOBJ_EFFECT_PATH_H

#include <std/string>

namespace paf {
namespace graph {

/* a path in the devkit's host tree */
inline string HostPath(const string &path)
{
	return string("host0:") + path;
}

static const string s_effects_path("vs0:data/internal/effects");
static const string s_background_path = HostPath("../resource2/background");
static const string s_bubble_path = HostPath("../resource2/bubble");
static const string s_dosch_path = HostPath("../resource2/dosch");
static const string s_panel_path = HostPath("../resource2/panel");
static const string s_particles_path = HostPath("../resource2/particles");
static const string s_shl_path = HostPath("../resource2/shl");
static const string s_earth_path = HostPath("../resource2/earth");

}
}

#endif
