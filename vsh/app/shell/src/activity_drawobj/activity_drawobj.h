/*
 * ActivityDrawObj and Activity ARL shaders (Sony path: app/common/src/activity_drawobj/).
 * Renders LiveArea activity cards, partial/full masked background panels, link icons, and avatars.
 */
#ifndef SHELL_ACTIVITY_DRAWOBJ_H
#define SHELL_ACTIVITY_DRAWOBJ_H

#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/memory>

namespace paf {
namespace graph {
namespace arl {
class VertexArray;
class IndexArray;
}
}
}

enum ActivityDrawMode {
	ACTIVITY_DRAW_MODE_COLOR_ONLY = 0,
	ACTIVITY_DRAW_MODE_PARTIAL_MASK = 1,
	ACTIVITY_DRAW_MODE_FULL_MASK = 2
};

enum ActivityMeshIndexSlot {
	ACTIVITY_MESH_COLOR_ONLY_BORDER = 0,
	ACTIVITY_MESH_COLOR_ONLY_INTERIOR = 1,
	ACTIVITY_MESH_PARTIAL_MASKED = 2,
	ACTIVITY_MESH_PARTIAL_BORDER = 3,
	ACTIVITY_MESH_PARTIAL_INTERIOR = 4,
	ACTIVITY_MESH_FULL_MASKED = 5,
	ACTIVITY_MESH_FULL_LOWER_BORDER = 6,
	ACTIVITY_MESH_FULL_LOWER_INTERIOR = 7,
	ACTIVITY_MESH_SLOT_COUNT = 8
};

struct ActivityMeshIndexBuffer {
	paf::graph::arl::IndexArray *indices;
	uint32_t count;
};

struct ActivityDrawObjFlags {
	bool modeDirty : 1;
	bool layoutDirty : 1;
	bool verticesDirty : 1;
	bool compactVariant : 1;
	uint8_t unusedBits : 4;
};

class ActivityDrawObj : public paf::graph::DrawObj {
public:
	static const char *DrawObjType();
	static void DrawCallback(const DrawCallbackArg &arg);

	ActivityDrawObj();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~ActivityDrawObj();
	virtual void OnDraw(const paf::math::matrix &parent, uint32_t color, float alpha,
			    paf::graph::DrawInfo &info);
	virtual void Draw(const paf::math::matrix &world, uint32_t color, float alpha);
	virtual void ReleaseAll();
	virtual paf::graph::DrawObjBounds GetBounds() const
	{
		return paf::graph::DrawObjBounds(m_boundsMin, m_boundsMax);
	}
	virtual void SetTexture(const paf::intrusive_ptr<paf::graph::Surface> &texture,
				uint32_t index = 0);
	virtual void GetTexture(paf::intrusive_ptr<paf::graph::Surface> &texture,
				uint32_t index = 0);
	virtual void SetPos(paf::math::v3 pos);
	virtual void SetSize(paf::math::v3 size);
	virtual void Update();
	virtual void CalcWorldMatrix(paf::math::matrix &worldMat,
				     const paf::math::matrix &parent, bool unscaled);
	virtual void UpdateLayout();
	virtual void UpdateMeshVerticesPosition();
	virtual void UpdateMeshVerticesTexcoord();

	void SetMode(int mode);
	void SetVariant(int variant);

private:
	void ReleaseMesh();
	void CreateMesh();
	void UpdateLayoutAxis(uint32_t axis, paf::math::v2 texSize, const float *margins);

	static const char s_drawObjType[];

	ActivityDrawObjFlags m_activityFlags;
	int8_t m_activeMode;
	int8_t m_pendingMode;
	uint8_t m_leftEdgeClamp;
	uint8_t m_rightEdgeClamp;
	uint8_t m_topEdgeClamp;
	uint8_t m_bottomEdgeClamp;
	uint8_t m_pad;
	paf::math::v3 m_planeSize;
	paf::math::v3 m_planePos;
	paf::math::v3 m_scaleValue;
	paf::math::v3 m_boundsMin;
	paf::math::v3 m_boundsMax;
	paf::intrusive_ptr<paf::graph::Surface> m_texSurface[3];
	paf::graph::arl::VertexArray *m_meshVertices;
};

/* Shared ARL mesh index buffers and activity shader accessors */
void arlGraphicsInit(int option);
void arlGraphicsTerm(void);
ActivityMeshIndexBuffer *activityDrawObjGetIndexBuffer(uint32_t slot);

void activityShaderInit(void);
void activityShaderTerm(void);

paf::common::SharedPtr<paf::graph::Shader> *activityDrawObjGetColorOnlyShader(void);
paf::common::SharedPtr<paf::graph::Shader> *activityDrawObjGetPartialMaskShader(void);
paf::common::SharedPtr<paf::graph::Shader> *activityDrawObjGetFullMaskShader(void);
paf::common::SharedPtr<paf::graph::Shader> *activityUiGetIconShader(void);
paf::common::SharedPtr<paf::graph::Shader> *activityUiGetPanelShader(void);

#endif /* SHELL_ACTIVITY_DRAWOBJ_H */
