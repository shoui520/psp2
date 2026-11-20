/*
 * applauncher_folderframe.h: FolderFrameDrawObj, the draw object of applauncher_folderframe.cpp.
 */
#ifndef _APPLAUNCHER_FOLDERFRAME_H
#define _APPLAUNCHER_FOLDERFRAME_H

#include <stdint.h>
#include <math/ngp/v3.h>
#include <math/ngp/matrix.h>
#include <graphics/surface/surface.h>
#include <graphics/drawobj/draw_obj.h>

/*
 * The ring drawn round a folder page's name (Sony's type name; applauncher_folderframe.cpp,
 * 0x810D808C-0x810D8ACC): one texture on a strip of 26 vertices, laid out like a plane's, the
 * scale times the object's size, without align modes. 176 bytes; its members' names are ours,
 * after PlaneObj's.
 */
class FolderFrameDrawObj : public paf::graph::DrawObj {
public:
	static void DrawCallback(const DrawCallbackArg &arg);

	FolderFrameDrawObj();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~FolderFrameDrawObj();
	virtual void OnDraw(const paf::math::matrix &parent, uint32_t color, float alpha,
			    paf::graph::DrawInfo &info);
	virtual void Draw(const paf::math::matrix &world, uint32_t color, float alpha);
	virtual void ReleaseAll();

	/* the box the layout drew the ring in */
	virtual paf::graph::DrawObjBounds GetBounds() const
	{
		return paf::graph::DrawObjBounds(m_bounds_min, m_bounds_max);
	}

	virtual void SetTexture(const paf::intrusive_ptr<paf::graph::Surface> &texture,
				uint32_t index = 0);
	virtual void GetTexture(paf::intrusive_ptr<paf::graph::Surface> &texture,
				uint32_t index = 0);
	virtual void SetPos(paf::math::v3 pos);
	virtual void SetSize(paf::math::v3 size);
	virtual void Update();
	virtual void CalcWorldMatrix(paf::math::matrix &world_mat, const paf::math::matrix &parent,
				     bool unscaled);
	virtual void UpdateLayout();

	static const char m_drawobj_type[];

	bool m_layout_dirty : 1;
	/* the ring's size and position in the object, which the layout works out */
	paf::math::v3 m_plane_size;
	paf::math::v3 m_plane_pos;
	paf::math::v3 m_scale_value;
	paf::math::v3 m_bounds_min;
	paf::math::v3 m_bounds_max;
	paf::intrusive_ptr<paf::graph::Surface> m_texture;
};

#endif
