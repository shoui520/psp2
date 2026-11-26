/*
 * ActivityDrawObj and ARL index-buffer management (0x8138B160..0x8138D827).
 * Sony path: app/common/src/activity_drawobj/.
 */
#include "activity_drawobj.h"

#include <arm_neon.h>
#include <std/math.h>
#include <std/string.h>
#include <vectormath.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/draw_info.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/surface/surface.h>
#include <math/ngp/quaternion.h>

using paf::graph::DrawInfo;
using paf::graph::DrawObj;
using paf::graph::DrawObjBounds;
using paf::graph::DrawStyle;
using paf::graph::PlaneObjDrawParams;
using paf::graph::Surface;
using paf::graph::arl::ArlInstance;
using paf::graph::arl::IndexArray;
using paf::graph::arl::VertexArray;
using paf::graph::arl::VertexArraySpec;

/* Vertex formats for the 3 ActivityDrawObj mesh layouts */
struct ActivityColorOnlyVertex {
	float posX;
	float posY;
	float posZ;
	float frameU;
	float frameV;
};

struct ActivityPartialMaskVertex {
	float posX;
	float posY;
	float posZ;
	float frameU;
	float frameV;
	float imageU;
	float imageV;
};

struct ActivityFullMaskVertex {
	float posX;
	float posY;
	float posZ;
	float frameU;
	float frameV;
	float imageU;
	float imageV;
	float maskU;
	float maskV;
};

struct ActivityMeshDescriptor {
	uint32_t vertexCount;
	uint32_t indexCount;
	const uint16_t *indices;
};

/* Initial zeroed vertex templates uploaded on mesh creation (0x815242AC, 0x815244F8, 0x8152488C) */
static const ActivityColorOnlyVertex s_colorOnlyInitVertices[16] = {};
static const ActivityPartialMaskVertex s_partialMaskInitVertices[25] = {};
static const ActivityFullMaskVertex s_fullMaskInitVertices[64] = {};

/* Mode 0 (4x4 grid): 9-slice outer border (48 indices) and center quad (6 indices) */
static const uint16_t s_colorOnlyBorderIndices[48] = {
	0, 1, 4, 1, 4, 5, 1, 2, 5, 2, 5, 6, 2, 3, 6, 3, 6, 7,
	4, 5, 8, 5, 8, 9, 6, 7, 10, 7, 10, 11,
	8, 9, 12, 9, 12, 13, 9, 10, 13, 10, 13, 14, 10, 11, 14, 11, 14, 15,
};

static const uint16_t s_colorOnlyInteriorIndices[6] = {
	5, 6, 9, 6, 9, 10,
};

/* Mode 1 (5x5 grid): partial-mask thumbnail area (36), unmasked border (54), interior (18) */
static const uint16_t s_partialMaskRegionIndices[36] = {
	2, 3, 7, 3, 7, 8, 3, 4, 8, 4, 8, 9,
	7, 8, 12, 8, 12, 13, 8, 9, 13, 9, 13, 14,
	12, 13, 17, 13, 17, 18, 13, 14, 18, 14, 18, 19,
};

static const uint16_t s_partialBorderIndices[54] = {
	0, 1, 5, 1, 5, 6, 1, 2, 6, 2, 6, 7,
	5, 6, 10, 6, 10, 11,
	10, 11, 15, 11, 15, 16,
	15, 16, 20, 16, 20, 21, 16, 17, 21, 17, 21, 22,
	17, 18, 22, 18, 22, 23, 18, 19, 23, 19, 23, 24,
	13, 14, 18, 14, 18, 19,
};

static const uint16_t s_partialInteriorIndices[18] = {
	6, 7, 11, 7, 11, 12,
	11, 12, 16, 12, 16, 17,
	12, 13, 17, 13, 17, 18,
};

/* Mode 2 (8x8 grid): full-masked area (294), lower border (78), lower interior (6) */
static const uint16_t s_fullMaskRegionIndices[294] = {
	0, 1, 8, 1, 8, 9, 1, 2, 9, 2, 9, 10, 2, 3, 10, 3, 10, 11, 3, 4, 11, 4, 11, 12,
	4, 5, 12, 5, 12, 13, 5, 6, 13, 6, 13, 14, 6, 7, 14, 7, 14, 15,
	8, 9, 16, 9, 16, 17, 9, 10, 17, 10, 17, 18, 10, 11, 18, 11, 18, 19, 11, 12, 19, 12, 19, 20,
	12, 13, 20, 13, 20, 21, 13, 14, 21, 14, 21, 22, 14, 15, 22, 15, 22, 23,
	16, 17, 24, 17, 24, 25, 17, 18, 25, 18, 25, 26, 18, 19, 26, 19, 26, 27, 19, 20, 27, 20, 27, 28,
	20, 21, 28, 21, 28, 29, 21, 22, 29, 22, 29, 30, 22, 23, 30, 23, 30, 31,
	24, 25, 32, 25, 32, 33, 25, 26, 33, 26, 33, 34, 26, 27, 34, 27, 34, 35, 27, 28, 35, 28, 35, 36,
	28, 29, 36, 29, 36, 37, 29, 30, 37, 30, 37, 38, 30, 31, 38, 31, 38, 39,
	32, 33, 40, 33, 40, 41, 33, 34, 41, 34, 41, 42, 34, 35, 42, 35, 42, 43, 35, 36, 43, 36, 43, 44,
	36, 37, 44, 37, 44, 45, 37, 38, 45, 38, 45, 46, 38, 39, 46, 39, 46, 47,
	43, 44, 51, 44, 51, 52,
	40, 41, 48, 41, 48, 49, 41, 42, 49, 42, 49, 50, 42, 43, 50, 43, 50, 51,
	44, 45, 52, 45, 52, 53, 45, 46, 53, 46, 53, 54, 46, 47, 54, 47, 54, 55,
	48, 49, 56, 49, 56, 57, 49, 50, 57, 50, 57, 58, 50, 51, 58, 51, 58, 59,
	51, 52, 59, 52, 59, 60, 52, 53, 60, 53, 60, 61, 53, 54, 61, 54, 61, 62, 54, 55, 62, 55, 62, 63,
};

static const uint16_t s_fullLowerBorderIndices[78] = {
	40, 41, 48, 41, 48, 49, 41, 42, 49, 42, 49, 50, 42, 43, 50, 43, 50, 51,
	44, 45, 52, 45, 52, 53, 45, 46, 53, 46, 53, 54, 46, 47, 54, 47, 54, 55,
	48, 49, 56, 49, 56, 57, 49, 50, 57, 50, 57, 58, 50, 51, 58, 51, 58, 59,
	51, 52, 59, 52, 59, 60, 52, 53, 60, 53, 60, 61, 53, 54, 61, 54, 61, 62, 54, 55, 62, 55, 62, 63,
};

static const uint16_t s_fullLowerInteriorIndices[6] = {
	43, 44, 51, 44, 51, 52,
};

/* Descriptor table for the 8 shared index buffers (0x81525480) */
static const ActivityMeshDescriptor s_activityMeshDescriptors[ACTIVITY_MESH_SLOT_COUNT] = {
	{ 16, 48, s_colorOnlyBorderIndices },
	{ 0, 6, s_colorOnlyInteriorIndices },
	{ 25, 36, s_partialMaskRegionIndices },
	{ 0, 54, s_partialBorderIndices },
	{ 0, 18, s_partialInteriorIndices },
	{ 64, 294, s_fullMaskRegionIndices },
	{ 0, 78, s_fullLowerBorderIndices },
	{ 0, 6, s_fullLowerInteriorIndices },
};

/* Runtime GPU index buffers (0x81548D10) */
static ActivityMeshIndexBuffer s_activityIndexBuffers[ACTIVITY_MESH_SLOT_COUNT];

const char ActivityDrawObj::s_drawObjType[] = "ActivityDrawObj";

static inline uint32_t modeToMeshSlot(int8_t mode)
{
	if (mode == ACTIVITY_DRAW_MODE_PARTIAL_MASK)
		return ACTIVITY_MESH_PARTIAL_MASKED;
	if (mode == ACTIVITY_DRAW_MODE_FULL_MASK)
		return ACTIVITY_MESH_FULL_MASKED;
	return ACTIVITY_MESH_COLOR_ONLY_BORDER;
}

static inline paf::math::v2 computeNormalizedBorder(paf::math::v2 borderRatio,
						    int16_t surfaceDim, float planeDim)
{
	paf::math::v2 result(0.0f, 0.0f);
	if (planeDim > 1.0f) {
		float leftBorder = (borderRatio.extract_x() * static_cast<float>(surfaceDim)) / planeDim;
		float rightBorder = (borderRatio.extract_y() * static_cast<float>(surfaceDim)) / planeDim;
		float sum = leftBorder + rightBorder;
		if (sum > 1.0f) {
			leftBorder /= sum;
			rightBorder /= sum;
		}
		result = paf::math::v2(leftBorder, rightBorder);
	}
	return result;
}

/* 0x8138B160 */
bool ActivityDrawObj::IsInherit(const char *type) const
{
	if (type == s_drawObjType)
		return true;
	return DrawObj::IsInherit(type);
}

/* 0x8138B178 */
const char *ActivityDrawObj::GetDrawObjType() const
{
	return s_drawObjType;
}

const char *ActivityDrawObj::DrawObjType()
{
	return s_drawObjType;
}

/* 0x8138B184 */
ActivityDrawObj::ActivityDrawObj()
	: DrawObj(DrawStyle(0x41)),
	  m_activeMode(-1),
	  m_pendingMode(-1),
	  m_leftEdgeClamp(0),
	  m_rightEdgeClamp(0),
	  m_topEdgeClamp(0),
	  m_bottomEdgeClamp(0),
	  m_pad(0),
	  m_planeSize(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_planePos(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_scaleValue(sce::Vectormath::Simd::Aos::Vector3(1.0f)),
	  m_boundsMin(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_boundsMax(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_meshVertices(NULL)
{
	m_activityFlags.modeDirty = false;
	m_activityFlags.layoutDirty = false;
	m_activityFlags.verticesDirty = false;
	m_activityFlags.compactVariant = false;
	m_activityFlags.unusedBits = 0;
}

/* 0x8138B20A */
void ActivityDrawObj::ReleaseMesh()
{
	if (m_meshVertices != NULL) {
		delete m_meshVertices;
		m_meshVertices = NULL;
	}
}

/* 0x8138B2C8 / 0x8138B302 */
ActivityDrawObj::~ActivityDrawObj()
{
	ReleaseMesh();
}

/* 0x8138B342 */
void ActivityDrawObj::Update()
{
	if (m_activityFlags.layoutDirty) {
		UpdateLayout();
		m_activityFlags.layoutDirty = false;
	}
}

/* 0x8138B366 */
void ActivityDrawObj::CreateMesh()
{
	ArlInstance *gl = paf::graph::arl::Gl();
	if (m_activeMode < 0 || m_activeMode > 3)
		return;

	uint32_t meshSlot = 0;
	VertexArraySpec::Predefined layout = VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2;
	const void *initVertices = NULL;

	if (m_activeMode == ACTIVITY_DRAW_MODE_COLOR_ONLY) {
		meshSlot = ACTIVITY_MESH_COLOR_ONLY_BORDER;
		layout = VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2;
		SetShader(*activityDrawObjGetColorOnlyShader());
		initVertices = s_colorOnlyInitVertices;
	} else if (m_activeMode == ACTIVITY_DRAW_MODE_PARTIAL_MASK) {
		meshSlot = ACTIVITY_MESH_PARTIAL_MASKED;
		layout = VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4;
		SetShader(*activityDrawObjGetPartialMaskShader());
		initVertices = s_partialMaskInitVertices;
	} else if (m_activeMode == ACTIVITY_DRAW_MODE_FULL_MASK) {
		meshSlot = ACTIVITY_MESH_FULL_MASKED;
		layout = VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4_ATTRIB4_F2;
		SetShader(*activityDrawObjGetFullMaskShader());
		initVertices = s_fullMaskInitVertices;
	} else {
		return;
	}

	if (initVertices != NULL) {
		uint32_t vertexCount = s_activityMeshDescriptors[meshSlot].vertexCount;
		m_meshVertices = new VertexArray(
			gl->m_default_allocator,
			ScePafGraphics_4D1E289A(gl, layout, SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
			initVertices, vertexCount);
	}
}

void ActivityDrawObj::DrawCallback(const DrawCallbackArg &arg)
{
	arg.self->Draw(*arg.world_mat, arg.color, arg.alpha);
}

/* 0x8138B688 */
void ActivityDrawObj::OnDraw(const paf::math::matrix &parent, uint32_t color, float alpha,
			     DrawInfo &info)
{
	if (!m_flags.m_visible)
		return;

	ScePafGraphics_B7EB9246(this, &color, &alpha);
	if (!(alpha > 0.0f))
		return;

	if (m_activityFlags.layoutDirty) {
		UpdateLayout();
		m_activityFlags.layoutDirty = false;
	}
	if (m_activityFlags.modeDirty) {
		m_activeMode = m_pendingMode;
		ReleaseMesh();
		CreateMesh();
		m_activityFlags.modeDirty = false;
		m_activityFlags.verticesDirty = true;
	}
	if (m_activityFlags.verticesDirty) {
		UpdateMeshVerticesPosition();
		UpdateMeshVerticesTexcoord();
		m_activityFlags.verticesDirty = false;
	}

	if (paf::graph::g_scene_draw_layer.m_ptr != NULL ||
	    paf::graph::g_scene_draw_layer2.m_ptr != NULL) {
		uint32_t key = (info.m_layer << 16) + (info.m_sub_layer << 8) + info.m_order;
		DrawCallbackArg arg;
		arg.self = this;
		arg.world_mat = &parent;
		arg.color = color;
		arg.alpha = alpha;
		if (m_flags.m_draw_list == 0)
			paf::graph::g_scene_draw_layer.m_ptr->Push(key, DrawCallback, arg);
		else if (m_flags.m_draw_list == 1 &&
			 paf::graph::g_scene_draw_layer2.m_ptr != NULL)
			paf::graph::g_scene_draw_layer2.m_ptr->Push(key, DrawCallback, arg);
		info.m_order++;
	}
}

/* 0x8138B928 */
void ActivityDrawObj::Draw(const paf::math::matrix &world, uint32_t color, float alpha)
{
	if (static_cast<uint32_t>(m_activeMode) >= 4 || m_meshVertices == NULL)
		return;

	BindDrawStyle();
	ArlInstance *gl = paf::graph::arl::Gl();

	PlaneObjDrawParams params(m_texSurface, NULL);
	params.m_num_textures = 3;

	paf::math::matrix worldMat;
	CalcWorldMatrix(worldMat, world, false);

	uint32_t meshSlot = modeToMeshSlot(m_activeMode);
	params.m_vertices = m_meshVertices;
	params.m_indices = s_activityIndexBuffers[meshSlot].indices;
	params.m_count = s_activityMeshDescriptors[meshSlot].indexCount;
	params.m_instances = 1;

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));
	paf::math::v4 drawColor(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));
	drawColor.set_w(alpha);

	gl->Color(drawColor);
	gl->m_model.Load(worldMat);

	m_shader->Draw(params);
}

/* 0x8138BAA2 */
void ActivityDrawObj::ReleaseAll()
{
	for (uint32_t i = 0; i < 3; ++i)
		m_texSurface[i].clear();
}

/* 0x81413130 */
DrawObjBounds ActivityDrawObj::GetBounds() const
{
	return DrawObjBounds(m_boundsMin, m_boundsMax);
}

/* 0x8138BAC4 */
void ActivityDrawObj::SetTexture(const paf::intrusive_ptr<Surface> &texture, uint32_t index)
{
	if (index >= 3)
		return;
	if (m_texSurface[index].get() != texture.get()) {
		m_texSurface[index] = texture;
		m_activityFlags.verticesDirty = true;
	}
}

/* 0x8138BB00 */
void ActivityDrawObj::GetTexture(paf::intrusive_ptr<Surface> &texture, uint32_t index)
{
	if (index >= 3)
		return;
	texture = m_texSurface[index];
}

/* 0x8138BB2A */
void ActivityDrawObj::SetPos(paf::math::v3 pos)
{
	if (!(m_pos == pos)) {
		m_pos = pos;
		m_activityFlags.layoutDirty = true;
	}
}

/* 0x8138BB8C */
void ActivityDrawObj::SetSize(paf::math::v3 size)
{
	if (!(m_size == size)) {
		m_size = paf::math::v3(sce::Vectormath::Simd::Aos::maxPerElem(
			sce::Vectormath::Simd::Aos::Vector3(0.0f), size.getImpl()));
		m_activityFlags.layoutDirty = true;
		m_activityFlags.verticesDirty = true;
	}
}

/* 0x8138BBF8 */
void ActivityDrawObj::SetMode(int mode)
{
	if (static_cast<uint32_t>(mode) > ACTIVITY_DRAW_MODE_FULL_MASK)
		return;
	m_pendingMode = static_cast<int8_t>(mode);
	m_activityFlags.modeDirty = (m_activeMode != m_pendingMode);
}

/* 0x8138BC26 */
void ActivityDrawObj::SetVariant(int variant)
{
	m_activityFlags.compactVariant = (variant & 1) != 0;
}

/* 0x810D88AC */
void ActivityDrawObj::CalcWorldMatrix(paf::math::matrix &worldMat,
				      const paf::math::matrix &parent, bool unscaled)
{
	(void)unscaled;
	sce::Vectormath::Simd::Aos::Vector3 clampedSize = sce::Vectormath::Simd::Aos::maxPerElem(
		sce::Vectormath::Simd::Aos::Vector3(0.0f), m_planeSize.getImpl());
	sce::Vectormath::Simd::Aos::Matrix4 scaleMat =
		sce::Vectormath::Simd::Aos::Matrix4::scale(clampedSize);
	sce::Vectormath::Simd::Aos::Matrix4 localMat;

	if (m_rot != NULL) {
		paf::math::quaternion rotQuat =
			paf::math::quaternion::FromEulerAnglesYXZ(*m_rot);
		localMat = rotQuat.toMatrix(1.0e-4f).getImpl() * scaleMat;
	} else {
		localMat = scaleMat;
	}
	localMat.setCol3(sce::Vectormath::Simd::Aos::Vector4(m_planePos.getImpl(), 1.0f));
	worldMat = parent * paf::math::matrix(localMat);
}

/* 0x8138BC36 */
void ActivityDrawObj::UpdateLayoutAxis(uint32_t axis, paf::math::v2 texSize, const float *margins)
{
	float texDim = (axis == 0) ? texSize.extract_x() : texSize.extract_y();
	float marginNear = (axis == 0) ? margins[0] : margins[1];
	float marginFar = (axis == 0) ? margins[2] : margins[3];
	float scaleFactor = m_scaleValue.extract(axis);
	float boxSize = m_size.extract(axis);

	float contentDim = texDim - (marginNear + marginFar);
	float effectiveSpan = 0.0f;
	float planeExtent = 0.0f;
	if (contentDim > 0.0f) {
		effectiveSpan = boxSize * scaleFactor;
		planeExtent = scaleFactor * (boxSize / (contentDim / texDim));
	}

	m_planeSize.set(axis, planeExtent);

	float offset = 0.0f;
	if (effectiveSpan != 0.0f) {
		float delta = (marginNear - marginFar) * 0.5f;
		offset = -(planeExtent / texDim) * delta;
	}

	float center = m_pos.extract(axis) + offset;
	float halfSpan = effectiveSpan * 0.5f;
	m_planePos.set(axis, center);
	m_boundsMin.set(axis, center - halfSpan);
	m_boundsMax.set(axis, center + halfSpan);
}

/* 0x8138BFA0 */
void ActivityDrawObj::UpdateLayout()
{
	Surface *surface = m_texSurface[0].get();
	if (surface == NULL)
		return;

	paf::math::v2 texSize(static_cast<float>(surface->GetWidth()),
			      static_cast<float>(surface->GetHeight()));
	float marginLeft = static_cast<float>(surface->GetMarginLeft());
	float marginBottom = static_cast<float>(surface->GetMarginBottom());
	float marginRight = static_cast<float>(surface->GetMarginRight());
	float marginTop = static_cast<float>(surface->GetMarginTop());

	float zeroMargins[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	UpdateLayoutAxis(0, texSize, zeroMargins);

	m_planeSize.set_x(m_planeSize.extract_x() + marginLeft + marginRight);
	float horizontalShift = (marginRight - marginLeft) * 0.5f;
	if (m_rot != NULL)
		horizontalShift *= sce_paf_cosf(m_rot->extract_y());
	m_planePos.set_x(m_planePos.extract_x() + horizontalShift);

	UpdateLayoutAxis(1, texSize, zeroMargins);

	m_planePos.set_y(m_planePos.extract_y() + (marginTop - marginBottom) * 0.5f);
	m_planePos.set_z(m_pos.extract_z());
	m_planeSize.set_y(m_planeSize.extract_y() + marginBottom + marginTop);
	m_planeSize.set_z(1.0f);
}

/* 0x8138C14A */
void ActivityDrawObj::UpdateMeshVerticesPosition()
{
	if (static_cast<uint32_t>(m_activeMode) >= 4 || m_meshVertices == NULL)
		return;

	Surface *frameSurface = m_texSurface[0].get();
	if (frameSurface == NULL)
		return;

	paf::math::v2 ratioH(0.0f, 0.0f);
	paf::math::v2 ratioV(0.0f, 0.0f);
	ScePafGraphics_AFAE7807(frameSurface, &ratioH, &ratioV);

	float planeWidth = m_planeSize.extract_x();
	float planeHeight = m_planeSize.extract_y();
	paf::math::v2 normH = computeNormalizedBorder(ratioH, frameSurface->GetWidth(), planeWidth);
	paf::math::v2 normV = computeNormalizedBorder(ratioV, frameSurface->GetHeight(), planeHeight);

	if (m_activeMode == ACTIVITY_DRAW_MODE_COLOR_ONLY) {
		float cols[4] = {
			-0.5f,
			normH.extract_x() - 0.5f,
			0.5f - normH.extract_y(),
			0.5f
		};
		float rows[4] = {
			0.5f,
			0.5f - normV.extract_x(),
			normV.extract_y() - 0.5f,
			-0.5f
		};
		ActivityColorOnlyVertex *verts = static_cast<ActivityColorOnlyVertex *>(
			m_meshVertices->m_buffer->m_memory->m_addr);
		for (uint32_t r = 0; r < 4; ++r) {
			for (uint32_t c = 0; c < 4; ++c) {
				verts[r * 4 + c].posX = cols[c];
				verts[r * 4 + c].posY = rows[r];
			}
		}
	} else if (m_activeMode == ACTIVITY_DRAW_MODE_PARTIAL_MASK) {
		float cols[5] = {
			-0.5f,
			normH.extract_x() - 0.5f,
			0.0f,
			0.5f - normH.extract_y(),
			0.5f
		};
		float rows[5] = {
			0.5f,
			0.5f - normV.extract_x(),
			0.0f,
			normV.extract_y() - 0.5f,
			-0.5f
		};

		float halfWidth = planeWidth * 0.5f;
		float halfHeight = planeHeight * 0.5f;
		float splitX = 0.0f;
		if (halfWidth > 0.0f) {
			float padRight = m_activityFlags.compactVariant ? 6.0f : 10.0f;
			splitX = (halfWidth - (padRight + 400.0f)) / planeWidth;
			if (splitX < cols[1])
				splitX = cols[1];
			else if (splitX > cols[3])
				splitX = cols[3];
		}
		cols[2] = splitX;

		if (halfHeight > 0.0f) {
			float padTop = m_activityFlags.compactVariant ? 5.0f : 10.0f;
			m_topEdgeClamp = 0;
			m_bottomEdgeClamp = 0;
			float splitY = (halfHeight - (padTop + 80.0f)) / planeHeight;
			if (splitY > rows[1]) {
				rows[2] = rows[1];
				rows[1] = splitY;
				m_topEdgeClamp = 1;
			} else if (splitY < rows[3]) {
				rows[2] = rows[3];
				rows[3] = splitY;
				m_bottomEdgeClamp = 1;
			} else {
				rows[2] = splitY;
			}
		}

		ActivityPartialMaskVertex *verts = static_cast<ActivityPartialMaskVertex *>(
			m_meshVertices->m_buffer->m_memory->m_addr);
		for (uint32_t r = 0; r < 5; ++r) {
			for (uint32_t c = 0; c < 5; ++c) {
				verts[r * 5 + c].posX = cols[c];
				verts[r * 5 + c].posY = rows[r];
			}
		}
	} else if (m_activeMode == ACTIVITY_DRAW_MODE_FULL_MASK) {
		Surface *imageSurface = m_texSurface[1].get();
		if (imageSurface == NULL)
			return;

		paf::math::v2 panelRatioH(0.0f, 0.0f);
		paf::math::v2 panelRatioV(0.0f, 0.0f);
		ScePafGraphics_AFAE7807(imageSurface, &panelRatioH, &panelRatioV);

		paf::math::v2 panelNormH =
			computeNormalizedBorder(panelRatioH, imageSurface->GetWidth(), 812.0f);
		paf::math::v2 panelNormV =
			computeNormalizedBorder(panelRatioV, imageSurface->GetHeight(), 124.0f);

		float cols[8];
		float rows[8];
		cols[0] = -0.5f;
		cols[7] = 0.5f;
		rows[0] = 0.5f;
		rows[7] = -0.5f;

		float frameLeft = normH.extract_x() - 0.5f;
		float frameRight = 0.5f - normH.extract_y();
		float frameTop = 0.5f - normV.extract_x();
		float frameBottom = normV.extract_y() - 0.5f;

		float imgLeft = 0.0f;
		float imgRight = 0.0f;
		float imgInnerLeft = 0.0f;
		float imgInnerRight = 0.0f;
		if (planeWidth > 0.0f) {
			imgRight = 0.5f - (10.0f / planeWidth);
			imgLeft = imgRight - (812.0f / planeWidth);
			imgInnerRight = imgRight - ((panelNormH.extract_y() * 812.0f) / planeWidth);
			imgInnerLeft = imgLeft + ((panelNormH.extract_x() * 812.0f) / planeWidth);
		}

		if (frameLeft < imgLeft) {
			m_leftEdgeClamp = 0;
			cols[1] = frameLeft;
			cols[2] = imgLeft;
			cols[3] = imgInnerLeft;
		} else if (frameLeft <= imgInnerLeft) {
			m_leftEdgeClamp = 1;
			cols[1] = imgLeft;
			cols[2] = frameLeft;
			cols[3] = imgInnerLeft;
		} else {
			m_leftEdgeClamp = 2;
			cols[1] = imgLeft;
			cols[2] = imgInnerLeft;
			cols[3] = frameLeft;
		}

		if (frameRight > imgRight) {
			m_rightEdgeClamp = 0;
			cols[6] = frameRight;
			cols[5] = imgRight;
			cols[4] = imgInnerRight;
		} else if (frameRight >= imgInnerRight) {
			m_rightEdgeClamp = 1;
			cols[6] = imgRight;
			cols[5] = frameRight;
			cols[4] = imgInnerRight;
		} else {
			m_rightEdgeClamp = 2;
			cols[6] = imgRight;
			cols[5] = imgInnerRight;
			cols[4] = frameRight;
		}

		float imgTop = 0.0f;
		float imgBottom = 0.0f;
		float imgInnerTop = 0.0f;
		float imgInnerBottom = 0.0f;
		if (planeHeight > 0.0f) {
			imgTop = 0.5f - (10.0f / planeHeight);
			imgBottom = imgTop - (124.0f / planeHeight);
			imgInnerTop = imgTop - ((panelNormV.extract_x() * 124.0f) / planeHeight);
			imgInnerBottom = imgBottom + ((panelNormV.extract_y() * 124.0f) / planeHeight);
		}

		if (frameTop > imgTop) {
			m_topEdgeClamp = 0;
			rows[1] = frameTop;
			rows[2] = imgTop;
			rows[3] = imgInnerTop;
		} else if (frameTop >= imgInnerTop) {
			m_topEdgeClamp = 1;
			rows[1] = imgTop;
			rows[2] = frameTop;
			rows[3] = imgInnerTop;
		} else {
			m_topEdgeClamp = 2;
			rows[1] = imgTop;
			rows[2] = imgInnerTop;
			rows[3] = frameTop;
		}

		if (frameBottom < imgBottom) {
			m_bottomEdgeClamp = 0;
			rows[6] = frameBottom;
			rows[5] = imgBottom;
			rows[4] = imgInnerBottom;
		} else if (frameBottom <= imgInnerBottom) {
			m_bottomEdgeClamp = 1;
			rows[6] = imgBottom;
			rows[5] = frameBottom;
			rows[4] = imgInnerBottom;
		} else {
			m_bottomEdgeClamp = 2;
			rows[6] = imgBottom;
			rows[5] = imgInnerBottom;
			rows[4] = frameBottom;
		}

		ActivityFullMaskVertex *verts = static_cast<ActivityFullMaskVertex *>(
			m_meshVertices->m_buffer->m_memory->m_addr);
		for (uint32_t r = 0; r < 8; ++r) {
			for (uint32_t c = 0; c < 8; ++c) {
				verts[r * 8 + c].posX = cols[c];
				verts[r * 8 + c].posY = rows[r];
			}
		}
	}
}

/* 0x8138C860 */
void ActivityDrawObj::UpdateMeshVerticesTexcoord()
{
	if (static_cast<uint32_t>(m_activeMode) >= 4 || m_meshVertices == NULL)
		return;

	Surface *frameSurface = m_texSurface[0].get();
	if (frameSurface == NULL)
		return;

	paf::math::v2 ratioH(0.0f, 0.0f);
	paf::math::v2 ratioV(0.0f, 0.0f);
	ScePafGraphics_AFAE7807(frameSurface, &ratioH, &ratioV);

	float planeWidth = m_planeSize.extract_x();
	float planeHeight = m_planeSize.extract_y();

	if (m_activeMode == ACTIVITY_DRAW_MODE_COLOR_ONLY) {
		float texU[4] = { 0.0f, ratioH.extract_x(), 1.0f - ratioH.extract_y(), 1.0f };
		float texV[4] = { 0.0f, ratioV.extract_x(), 1.0f - ratioV.extract_y(), 1.0f };
		ActivityColorOnlyVertex *verts = static_cast<ActivityColorOnlyVertex *>(
			m_meshVertices->m_buffer->m_memory->m_addr);
		for (uint32_t r = 0; r < 4; ++r) {
			for (uint32_t c = 0; c < 4; ++c) {
				verts[r * 4 + c].frameU = texU[c];
				verts[r * 4 + c].frameV = texV[r];
			}
		}
	} else if (m_activeMode == ACTIVITY_DRAW_MODE_PARTIAL_MASK) {
		ActivityPartialMaskVertex *verts = static_cast<ActivityPartialMaskVertex *>(
			m_meshVertices->m_buffer->m_memory->m_addr);

		float frameU[5] = { 0.0f, ratioH.extract_x(), 0.5f, 1.0f - ratioH.extract_y(), 1.0f };
		float frameV[5] = { 0.0f, ratioV.extract_x(), 0.5f, 1.0f - ratioV.extract_y(), 1.0f };

		float posLeft = verts[1].posX;
		float posMidX = verts[2].posX;
		float posRight = verts[3].posX;
		if ((posRight - posLeft) > 0.0f) {
			frameU[2] = ratioH.extract_x() + ((1.0f - ratioH.extract_y() - ratioH.extract_x()) *
				    ((posMidX - posLeft) / (posRight - posLeft)));
		}

		float posRowMid = verts[2 * 5].posY;
		float posRowLower = verts[3 * 5].posY;
		float posRowBottom = verts[4 * 5].posY;
		if (m_topEdgeClamp != 1 && m_bottomEdgeClamp == 1) {
			frameV[2] = 1.0f - ratioV.extract_y();
			frameV[3] = frameV[2] +
				    (((1.0f - frameV[2]) * (posRowLower - posRowMid)) /
				     (posRowBottom - posRowMid));
		}

		float imageU[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		float imageV[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

		if (planeWidth > 0.0f) {
			float padRight = m_activityFlags.compactVariant ? 6.0f : 10.0f;
			float scaledX[5];
			for (uint32_t c = 0; c < 5; ++c)
				scaledX[c] = (verts[c].posX + 0.5f) * planeWidth;

			imageU[4] = (padRight / 400.0f) + 1.0f;
			imageU[3] = imageU[4] - ((scaledX[4] - scaledX[3]) / 400.0f);
			imageU[2] = imageU[3] - ((scaledX[3] - scaledX[2]) / 400.0f);
			imageU[1] = imageU[2] - ((scaledX[2] - scaledX[1]) / 400.0f);
			imageU[0] = imageU[1] - ((scaledX[1] - scaledX[0]) / 400.0f);
		}

		if (planeHeight > 0.0f) {
			float padTop = m_activityFlags.compactVariant ? 5.0f : 10.0f;
			float scaledY[5];
			for (uint32_t r = 0; r < 5; ++r)
				scaledY[r] = (0.5f - verts[r * 5].posY) * planeHeight;

			imageV[0] = -padTop / 80.0f;
			imageV[1] = imageV[0] + ((scaledY[1] - scaledY[0]) / 80.0f);
			imageV[2] = imageV[1] + ((scaledY[2] - scaledY[1]) / 80.0f);
			imageV[3] = imageV[2] + ((scaledY[3] - scaledY[2]) / 80.0f);
			imageV[4] = imageV[3] + ((scaledY[4] - scaledY[3]) / 80.0f);
		}

		for (uint32_t r = 0; r < 5; ++r) {
			for (uint32_t c = 0; c < 5; ++c) {
				verts[r * 5 + c].frameU = frameU[c];
				verts[r * 5 + c].frameV = frameV[r];
				verts[r * 5 + c].imageU = imageU[c];
				verts[r * 5 + c].imageV = imageV[r];
			}
		}
	} else if (m_activeMode == ACTIVITY_DRAW_MODE_FULL_MASK) {
		Surface *imageSurface = m_texSurface[1].get();
		if (imageSurface == NULL)
			return;

		ActivityFullMaskVertex *verts = static_cast<ActivityFullMaskVertex *>(
			m_meshVertices->m_buffer->m_memory->m_addr);

		float colPos[8];
		float rowPos[8];
		for (uint32_t i = 0; i < 8; ++i) {
			colPos[i] = verts[i].posX;
			rowPos[i] = verts[i * 8].posY;
		}

		/* Frame U coordinates across the 8 columns */
		float frameU[8] = { 0.0f, ratioH.extract_x(), 0.5f, 0.5f, 0.5f, 0.5f, 1.0f - ratioH.extract_y(), 1.0f };
		int leftAnchor = (m_leftEdgeClamp == 2) ? 3 : ((m_leftEdgeClamp == 1) ? 2 : 1);
		int rightAnchor = (m_rightEdgeClamp == 2) ? 4 : ((m_rightEdgeClamp == 1) ? 5 : 6);
		frameU[leftAnchor] = ratioH.extract_x();
		frameU[rightAnchor] = 1.0f - ratioH.extract_y();

		float frameWidthPx = static_cast<float>(frameSurface->GetWidth());
		float uPerPosX = planeWidth / frameWidthPx;
		float midUSlope = (frameU[rightAnchor] - frameU[leftAnchor]) /
				  (colPos[rightAnchor] - colPos[leftAnchor]);

		if (m_leftEdgeClamp == 2) {
			frameU[1] = frameU[0] + (colPos[1] - colPos[0]) * uPerPosX;
			frameU[2] = frameU[0] + (colPos[2] - colPos[0]) * uPerPosX;
		} else if (m_leftEdgeClamp == 1) {
			frameU[1] = frameU[0] + (colPos[1] - colPos[0]) * uPerPosX;
			frameU[3] = frameU[leftAnchor] + (colPos[3] - colPos[leftAnchor]) * midUSlope;
		} else {
			frameU[2] = frameU[leftAnchor] + (colPos[2] - colPos[leftAnchor]) * midUSlope;
			frameU[3] = frameU[leftAnchor] + (colPos[3] - colPos[leftAnchor]) * midUSlope;
		}

		if (m_rightEdgeClamp == 2) {
			frameU[6] = frameU[7] + (colPos[6] - colPos[7]) * uPerPosX;
			frameU[5] = frameU[7] + (colPos[5] - colPos[7]) * uPerPosX;
		} else if (m_rightEdgeClamp == 1) {
			frameU[6] = frameU[7] + (colPos[6] - colPos[7]) * uPerPosX;
			frameU[4] = frameU[rightAnchor] + (colPos[4] - colPos[rightAnchor]) * midUSlope;
		} else {
			frameU[5] = frameU[rightAnchor] + (colPos[5] - colPos[rightAnchor]) * midUSlope;
			frameU[4] = frameU[rightAnchor] + (colPos[4] - colPos[rightAnchor]) * midUSlope;
		}

		/* Frame V coordinates across the 8 rows */
		float frameV[8] = { 0.0f, ratioV.extract_x(), 0.5f, 0.5f, 0.5f, 0.5f, 1.0f - ratioV.extract_y(), 1.0f };
		int topAnchor = (m_topEdgeClamp == 2) ? 3 : ((m_topEdgeClamp == 1) ? 2 : 1);
		int bottomAnchor = (m_bottomEdgeClamp == 2) ? 4 : ((m_bottomEdgeClamp == 1) ? 5 : 6);
		frameV[topAnchor] = ratioV.extract_x();
		frameV[bottomAnchor] = 1.0f - ratioV.extract_y();

		float frameHeightPx = static_cast<float>(frameSurface->GetHeight());
		float vPerPosY = planeHeight / frameHeightPx;
		float midVSlope = (frameV[bottomAnchor] - frameV[topAnchor]) /
				  (rowPos[bottomAnchor] - rowPos[topAnchor]);

		if (m_topEdgeClamp == 2) {
			frameV[1] = frameV[0] - (rowPos[1] - rowPos[0]) * vPerPosY;
			frameV[2] = frameV[0] - (rowPos[2] - rowPos[0]) * vPerPosY;
		} else if (m_topEdgeClamp == 1) {
			frameV[1] = frameV[0] - (rowPos[1] - rowPos[0]) * vPerPosY;
			frameV[3] = frameV[topAnchor] + (rowPos[3] - rowPos[topAnchor]) * midVSlope;
		} else {
			frameV[2] = frameV[topAnchor] + (rowPos[2] - rowPos[topAnchor]) * midVSlope;
			frameV[3] = frameV[topAnchor] + (rowPos[3] - rowPos[topAnchor]) * midVSlope;
		}

		if (m_bottomEdgeClamp == 2) {
			frameV[6] = frameV[7] - (rowPos[6] - rowPos[7]) * vPerPosY;
			frameV[5] = frameV[7] - (rowPos[5] - rowPos[7]) * vPerPosY;
		} else if (m_bottomEdgeClamp == 1) {
			frameV[6] = frameV[7] - (rowPos[6] - rowPos[7]) * vPerPosY;
			frameV[4] = frameV[bottomAnchor] + (rowPos[4] - rowPos[bottomAnchor]) * midVSlope;
		} else {
			frameV[5] = frameV[bottomAnchor] + (rowPos[5] - rowPos[bottomAnchor]) * midVSlope;
			frameV[4] = frameV[bottomAnchor] + (rowPos[4] - rowPos[bottomAnchor]) * midVSlope;
		}

		/* Mask U/V coordinates (812x124 inset by 10px from top-right) */
		float maskU[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		float maskV[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

		if (planeWidth > 0.0f) {
			float scaledX[8];
			for (uint32_t c = 0; c < 8; ++c)
				scaledX[c] = (colPos[c] + 0.5f) * planeWidth;

			maskU[7] = (10.0f / 812.0f) + 1.0f;
			for (int c = 6; c >= 0; --c)
				maskU[c] = maskU[c + 1] - ((scaledX[c + 1] - scaledX[c]) / 812.0f);
		}

		if (planeHeight > 0.0f) {
			float scaledY[8];
			for (uint32_t r = 0; r < 8; ++r)
				scaledY[r] = (0.5f - rowPos[r]) * planeHeight;

			maskV[0] = -10.0f / 124.0f;
			for (uint32_t r = 1; r < 8; ++r)
				maskV[r] = maskV[r - 1] + ((scaledY[r] - scaledY[r - 1]) / 124.0f);
		}

		/* Panel image U/V coordinates (9-sliced using imageSurface ratios) */
		paf::math::v2 imgRatioH(0.0f, 0.0f);
		paf::math::v2 imgRatioV(0.0f, 0.0f);
		ScePafGraphics_AFAE7807(imageSurface, &imgRatioH, &imgRatioV);

		float imageU[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
		int imgLeftOuter = (m_leftEdgeClamp == 0) ? 2 : 1;
		int imgLeftInner = (m_leftEdgeClamp == 2) ? 2 : 3;
		int imgRightOuter = (m_rightEdgeClamp == 0) ? 5 : 6;
		int imgRightInner = (m_rightEdgeClamp == 2) ? 5 : 4;

		imageU[imgLeftOuter] = 0.0f;
		imageU[imgLeftInner] = imgRatioH.extract_x();
		imageU[imgRightOuter] = 1.0f;
		imageU[imgRightInner] = 1.0f - imgRatioH.extract_y();

		float imgMidUSlope = (imageU[imgRightInner] - imageU[imgLeftInner]) /
				     (colPos[imgRightInner] - colPos[imgLeftInner]);
		if (m_leftEdgeClamp == 2) {
			imageU[3] = imageU[imgLeftInner] +
				    (colPos[3] - colPos[imgLeftInner]) * imgMidUSlope;
		} else if (m_leftEdgeClamp == 1) {
			imageU[2] = imageU[imgLeftOuter] +
				    ((imageU[imgLeftInner] - imageU[imgLeftOuter]) *
				     (colPos[2] - colPos[imgLeftOuter])) /
				    (colPos[imgLeftInner] - colPos[imgLeftOuter]);
		} else {
			imageU[1] = 0.0f;
		}

		if (m_rightEdgeClamp == 2) {
			imageU[4] = imageU[imgRightInner] +
				    (colPos[4] - colPos[imgRightInner]) * imgMidUSlope;
		} else if (m_rightEdgeClamp == 1) {
			imageU[5] = imageU[imgRightOuter] +
				    ((imageU[imgRightInner] - imageU[imgRightOuter]) *
				     (colPos[5] - colPos[imgRightOuter])) /
				    (colPos[imgRightInner] - colPos[imgRightOuter]);
		} else {
			imageU[6] = 1.0f;
		}

		float imageV[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
		int imgTopOuter = (m_topEdgeClamp == 0) ? 2 : 1;
		int imgTopInner = (m_topEdgeClamp == 2) ? 2 : 3;
		int imgBottomOuter = (m_bottomEdgeClamp == 0) ? 5 : 6;
		int imgBottomInner = (m_bottomEdgeClamp == 2) ? 5 : 4;

		imageV[imgTopOuter] = 0.0f;
		imageV[imgTopInner] = imgRatioV.extract_x();
		imageV[imgBottomOuter] = 1.0f;
		imageV[imgBottomInner] = 1.0f - imgRatioV.extract_y();

		float imgMidVSlope = (imageV[imgBottomInner] - imageV[imgTopInner]) /
				     (rowPos[imgBottomInner] - rowPos[imgTopInner]);
		if (m_topEdgeClamp == 2) {
			imageV[3] = imageV[imgTopInner] +
				    (rowPos[3] - rowPos[imgTopInner]) * imgMidVSlope;
		} else if (m_topEdgeClamp == 1) {
			imageV[2] = imageV[imgTopOuter] +
				    ((imageV[imgTopInner] - imageV[imgTopOuter]) *
				     (rowPos[2] - rowPos[imgTopOuter])) /
				    (rowPos[imgTopInner] - rowPos[imgTopOuter]);
		} else {
			imageV[1] = 0.0f;
		}

		if (m_bottomEdgeClamp == 2) {
			imageV[4] = imageV[imgBottomInner] +
				    (rowPos[4] - rowPos[imgBottomInner]) * imgMidVSlope;
		} else if (m_bottomEdgeClamp == 1) {
			imageV[5] = imageV[imgBottomOuter] +
				    ((imageV[imgBottomInner] - imageV[imgBottomOuter]) *
				     (rowPos[5] - rowPos[imgBottomOuter])) /
				    (rowPos[imgBottomInner] - rowPos[imgBottomOuter]);
		} else {
			imageV[6] = 1.0f;
		}

		for (uint32_t r = 0; r < 8; ++r) {
			for (uint32_t c = 0; c < 8; ++c) {
				ActivityFullMaskVertex &vertex = verts[r * 8 + c];
				vertex.frameU = frameU[c];
				vertex.frameV = frameV[r];
				vertex.imageU = imageU[c];
				vertex.imageV = imageV[r];
				vertex.maskU = maskU[c];
				vertex.maskV = maskV[r];
			}
		}
	}
}

/* 0x8138D570 */
void arlGraphicsInit(int option)
{
	(void)option;
	ArlInstance *gl = paf::graph::arl::Gl();
	activityShaderInit();

	for (uint32_t i = 0; i < ACTIVITY_MESH_SLOT_COUNT; ++i) {
		const ActivityMeshDescriptor &desc = s_activityMeshDescriptors[i];
		s_activityIndexBuffers[i].indices =
			new IndexArray(gl->m_default_allocator, desc.indices, desc.indexCount);
		s_activityIndexBuffers[i].count = desc.indexCount;
	}
}

/* 0x8138D77E */
void arlGraphicsTerm(void)
{
	for (uint32_t i = 0; i < ACTIVITY_MESH_SLOT_COUNT; ++i) {
		if (s_activityIndexBuffers[i].indices != NULL) {
			delete s_activityIndexBuffers[i].indices;
			s_activityIndexBuffers[i].indices = NULL;
		}
	}
	activityShaderTerm();
}

/* 0x8138D7FC */
ActivityMeshIndexBuffer *activityDrawObjGetIndexBuffer(uint32_t slot)
{
	if (slot < ACTIVITY_MESH_SLOT_COUNT)
		return &s_activityIndexBuffers[slot];
	return NULL;
}
