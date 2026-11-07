/*
 * The painter behind ui::Canvas: strokes of round points drawn with the GPU into a 960x544
 * picture, through a GXM context and shader patcher of its own. The canvas keeps a pointer to a
 * CanvasPainter, which holds the state (CanvasPainter::Impl). The memory block names ("common",
 * "basic") and the program parameter names are Sony's (their strings); every class, method and
 * member name is ours.
 */
#ifndef PAF_WIDGET_CANVAS_PAINTER_H
#define PAF_WIDGET_CANVAS_PAINTER_H

#include <stdint.h>
#include <gxm.h>
#include <kernel/sysmem/memblock.h>

namespace paf {
namespace ui {

class CanvasPainter {
public:
	class Impl;

	/* 0x811aa506 (TOOL): the GXM context made by the first painter */
	CanvasPainter();
	/* 0x811aa58e */
	~CanvasPainter();

	/* 0x811aaa1c */
	void GetColor(float *r, float *g, float *b, float *a);
	/* 0x811aaa44 */
	void SetColor(float r, float g, float b, float a);
	/* 0x811aaa5a: the diameter of the points */
	float GetSize();
	/* 0x811aaa64 */
	void SetSize(float size);
	/* 0x811aaa6c: the points' size scaled by the pressure given with them */
	void SetPressureSensitive(bool sensitive);
	/* 0x811aaa76: does nothing (ScePafWidget FD3E7E8A calls it) */
	void Empty1();
	/* 0x811aaa7a: does nothing (ScePafWidget 710ECEC2 calls it) */
	void Empty2();
	/* 0x811aabde: a new stroke from its first point */
	void BeginStroke(float x, float y, float pressure);
	/* 0x811aabf6 */
	void ContinueStroke(float x, float y, float pressure);
	/* 0x811aac08: the stroke kept in the picture (the position the touch ended at is not
	   used) */
	void EndStroke(float x, float y);
	/* 0x811aac22: the picture as a texture, its size and its pitch */
	void GetTexture(SceGxmTexture **texture, int32_t *width, int32_t *height, int32_t *pitch);

	Impl *m_impl;
};

/* the drawn area of a stroke, in half pixels of the picture (names ours) */
struct CanvasRect {
	int32_t left;
	int32_t right;
	int32_t top;
	int32_t bottom;
};

/* a point of a stroke */
struct CanvasPoint {
	float x;
	float y;
	float size;
};

/* the clear program's vertex: a position */
struct CanvasClearVertex {
	float x;
	float y;
};

/* the copy program's vertex */
struct CanvasCopyVertex {
	float x;
	float y;
	float z;
	uint32_t color;
	/* half floats */
	uint16_t u;
	uint16_t v;
};

/* 0x298 bytes */
class CanvasPainter::Impl {
public:
	enum {
		WIDTH = 960,
		HEIGHT = 544,
		/* points kept before the stroke is drawn into the picture */
		MAX_POINTS = 20,
		/* points a stroke is filled out with */
		MAX_STROKE_VERTICES = 30000
	};

	static void InitGxm();

	Impl()
	{
		if (!s_initialized) {
			s_initialized = true;
			InitGxm();
		}
		m_point_count = 0;
		m_color[0] = 0.0f;
		m_size = 5.0f;
		m_color[1] = 0.0f;
		m_color[2] = 0.0f;
		m_color[3] = 1.0f;
		InitClearProgram();
		InitCopyProgram();
		InitQuad();
		InitBrushProgram();
		InitPointProgram();
		InitBuffers();
		InitPictureTarget();
		InitDisplayTarget();
	}

	/* 0x811a9d40 */
	void InitDisplayTarget();
	/* 0x811a9e04 */
	void InitPictureTarget();
	/* 0x811a9ec8 */
	void InitBuffers();
	/* 0x811a9f52 */
	void InitPointProgram();
	/* 0x811aa08c */
	void InitBrushProgram();
	/* 0x811aa1ea */
	void InitQuad();
	/* 0x811aa288 */
	void InitCopyProgram();
	/* 0x811aa3be */
	void InitClearProgram();
	/* 0x811aa59e: the points filled out into the brush's vertices */
	void BuildStroke(CanvasRect *rect);
	/* 0x811aa878: the picture, the stroke on top, onto the display surface */
	void Draw();
	/* 0x811aaa7c: the points drawn into the picture */
	void Flush();
	/* 0x811aab6e */
	void AddPoint(float x, float y, float pressure);

	static bool s_initialized;

	/* a colour over the whole surface */
	SceGxmShaderPatcherId m_clear_vertex_id;
	SceGxmShaderPatcherId m_clear_fragment_id;
	SceGxmVertexProgram *m_clear_vertex;
	SceGxmFragmentProgram *m_clear_fragment;
	SceUID m_clear_vertices_uid;
	SceUID m_clear_indices_uid;
	CanvasClearVertex *m_clear_vertices;
	uint16_t *m_clear_indices;
	/* "color" */
	const SceGxmProgramParameter *m_clear_color;
	/* the picture copied on a quad */
	SceGxmShaderPatcherId m_copy_vertex_id;
	SceGxmShaderPatcherId m_copy_fragment_id;
	SceGxmVertexProgram *m_copy_vertex;
	SceGxmFragmentProgram *m_copy_fragment;
	/* points of a size and colour */
	SceGxmShaderPatcherId m_point_vertex_id;
	SceGxmShaderPatcherId m_point_fragment_id;
	SceGxmVertexProgram *m_point_vertex;
	SceGxmFragmentProgram *m_point_fragment;
	/* "color" */
	const SceGxmProgramParameter *m_point_color;
	/* "size" */
	const SceGxmProgramParameter *m_point_size;
	uint32_t m_reserved1[4];
	SceUID m_quad_vertices_uid;
	SceUID m_quad_indices_uid;
	CanvasCopyVertex *m_quad_vertices;
	uint16_t *m_quad_indices;
	SceUID m_stroke_vertices_uid;
	SceUID m_stroke_indices_uid;
	CanvasPoint *m_stroke_vertices;
	uint16_t *m_stroke_indices;
	/* round points, their size per vertex */
	SceGxmShaderPatcherId m_brush_vertex_id;
	SceGxmShaderPatcherId m_brush_fragment_id;
	SceGxmVertexProgram *m_brush_vertex;
	SceGxmFragmentProgram *m_brush_fragment;
	/* "fColor" */
	const SceGxmProgramParameter *m_brush_color;
	uint32_t m_reserved2;
	SceUID m_positions_uid;
	SceUID m_position_indices_uid;
	/* the points' positions and their indices 0..19 */
	CanvasClearVertex *m_positions;
	uint16_t *m_position_indices;
	/* the stroke's vertices BuildStroke made */
	uint16_t m_stroke_count;
	/* the picture the strokes are kept in */
	SceUID m_picture_uid;
	void *m_picture_data;
	SceGxmColorSurface m_picture_surface;
	SceGxmTexture m_picture_texture;
	SceUID m_picture_depth_uid;
	void *m_picture_depth_data;
	SceGxmDepthStencilSurface m_picture_depth;
	SceGxmRenderTarget *m_picture_target;
	/* the picture with the stroke on top, what the canvas shows */
	SceUID m_display_uid;
	void *m_display_data;
	SceGxmColorSurface m_display_surface;
	SceGxmTexture m_display_texture;
	SceUID m_display_depth_uid;
	void *m_display_depth_data;
	SceGxmDepthStencilSurface m_display_depth;
	SceGxmRenderTarget *m_display_target;
	/* the brush's texture */
	SceGxmTexture m_brush_texture;
	CanvasPoint m_points[MAX_POINTS];
	uint32_t m_point_count;
	bool m_pressure_sensitive;
	float m_size;
	/* counts the points the strokes were filled out between */
	float m_segments;
	float m_color[4];
};

}
}

#endif
