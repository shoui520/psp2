/*
 * paf::graph::arl's GXM render targets, kept by size and multisample mode and reused
 * across frames: a target serves as many scenes in a frame as it was made for (the
 * memory block's name is Sony's; the file's, the classes' and the members' names ours).
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_RENDER_TARGET_POOL_H
#define PAF_GRAPHICS_ARL_GXM_GXM_RENDER_TARGET_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <std/map>
#include <std/string.h>
#include <std/vector>

namespace paf {
namespace graph {
namespace arl {

class RenderTargetPool {
public:
	/* ordered by its fields packed into one word */
	class Key {
	public:
		uint32_t Pack() const
		{
			return m_width | (m_height << 13) | (m_multisample_mode << 26);
		}

		bool operator<(const Key &other) const
		{
			return Pack() < other.Pack();
		}

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_multisample_mode;
	};

	/* a render target and the frame it was last used in */
	class Target {
	public:
		Target() : m_target(NULL), m_frame(-1), m_uses(0)
		{
			sce_paf_memset(&m_params, 0, sizeof(m_params));
		}

		SceGxmRenderTarget *m_target;
		SceGxmRenderTargetParams m_params;
		uint32_t m_frame;
		int32_t m_uses;
	};

	typedef vector<Target *> TargetList;

	RenderTargetPool();
	~RenderTargetPool();

	void Clear();
	SceGxmRenderTarget *GetRenderTarget(Key key);
	void AddScenes(Key key, int32_t scenes);

	uint32_t m_reserved;
	uint32_t m_mem_size;
	map<Key, TargetList *> m_targets;
	map<Key, int32_t> m_scenes;
};

}
}
}

#endif
