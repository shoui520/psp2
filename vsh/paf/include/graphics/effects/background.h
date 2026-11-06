/*
 * The backgrounds the effects are drawn over: what each draws, the textures its reflections and
 * refractions sample, and the list the renderer keeps them in, which hands every call to the
 * background chosen. EnvAuto is Sony's name (its "EnvAuto::captureReflections" marker); the
 * interface, the other classes and every member name are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_BACKGROUND_H
#define PAF_GRAPHICS_EFFECTS_BACKGROUND_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/camera.h>
#include <graphics/effects/page_turn_quad.h>
#include <graphics/effects/post_effects.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include "../../../src/graphics/arl/gxm/gxm_framebuffer_object.h"
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {

/* a texture the backgrounds share, set by whoever draws it (the renderer makes the holder) */
typedef common::SharedPtr<arl::Texture2D> SharedTexture;

class Background {
public:
	virtual ~Background()
	{
	}

	/* the background's resources loaded from the folder */
	virtual void Load(const string &dir) = 0;
	virtual void Draw() = 0;
	/* what reflections and refractions sample */
	virtual arl::TextureSource GetReflectionTexture() = 0;
	virtual arl::TextureSource GetRefractionTexture() = 0;
	/* the shared texture, else the front end's default */
	virtual common::SharedPtr<arl::Texture2D> GetSharedTexture() = 0;
	/* the camera it is seen by, and the texture the backgrounds share */
	virtual void Init(common::SharedPtr<Camera> camera,
			  common::SharedPtr<SharedTexture> texture) = 0;

	virtual void Update()
	{
	}

	virtual void Reset(bool force)
	{
	}

	/* how the background's texture coordinates are moved */
	virtual math::matrix GetUVTransform() = 0;
	/* a value the list hands on to the background chosen (no caller in libpaf) */
	virtual void SetState(int32_t state) = 0;
};

/* the backgrounds, of which the one at the index is drawn */
class BackgroundList : public Background {
public:
	BackgroundList() : m_index(-1)
	{
	}

	/* the background chosen, else none */
	common::SharedPtr<Background> GetCurrent()
	{
		if (m_index >= 0 && m_index < (int32_t)m_backgrounds.size())
			return m_backgrounds[m_index];
		return common::SharedPtr<Background>();
	}

	/* the last background added is chosen */
	void Add(common::SharedPtr<Background> background)
	{
		m_backgrounds.push_back(background);
		m_index = m_backgrounds.size() - 1;
	}

	/* the background at the index chosen; the test is the wrong way round, so an index in
	   the list chooses none and only a negative one is kept */
	void Select(int32_t index)
	{
		m_index = ((uint32_t)index < m_backgrounds.size() && index < 0) ? index : -1;
	}

	virtual void Load(const string &dir)
	{
		for (uint32_t i = 0; i != m_backgrounds.size(); i++)
			m_backgrounds[i]->Load(dir);
	}

	virtual void Draw()
	{
		if (GetCurrent().get() != NULL)
			GetCurrent()->Draw();
	}

	virtual arl::TextureSource GetReflectionTexture()
	{
		if (GetCurrent().get() != NULL)
			return GetCurrent()->GetReflectionTexture();

		arl::DefaultTextures *textures = arl::Gl();
		common::SharedPtr<arl::TextureObject> texture = textures->m_white;

		return arl::TextureSource(texture);
	}

	virtual arl::TextureSource GetRefractionTexture()
	{
		if (GetCurrent().get() != NULL)
			return GetCurrent()->GetRefractionTexture();

		arl::DefaultTextures *textures = arl::Gl();
		common::SharedPtr<arl::TextureObject> texture = textures->m_white;

		return arl::TextureSource(texture);
	}

	virtual common::SharedPtr<arl::Texture2D> GetSharedTexture()
	{
		if (GetCurrent().get() != NULL)
			return GetCurrent()->GetSharedTexture();

		arl::DefaultTextures *textures = arl::Gl();

		return textures->m_white;
	}

	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<SharedTexture> texture)
	{
		for (uint32_t i = 0; i != m_backgrounds.size(); i++)
			m_backgrounds[i]->Init(camera, texture);
	}

	virtual void Update()
	{
		if (GetCurrent().get() != NULL)
			GetCurrent()->Update();
	}

	virtual void Reset(bool force)
	{
		if (GetCurrent().get() != NULL)
			GetCurrent()->Reset(false);
	}

	virtual math::matrix GetUVTransform()
	{
		if (GetCurrent().get() != NULL)
			return GetCurrent()->GetUVTransform();
		return math::matrix::Identity();
	}

	virtual void SetState(int32_t state)
	{
		if (GetCurrent().get() != NULL)
			GetCurrent()->SetState(state);
	}

	vector<common::SharedPtr<Background> > m_backgrounds;
	int32_t m_index;
};

/* a background of one colour, drawn with the EmptyBackground program; 0xc8 bytes */
class EmptyBackground : public Background {
public:
	/* 0x810d151c */
	EmptyBackground(int32_t mode);

	virtual void Load(const string &dir);
	virtual void Draw();
	virtual arl::TextureSource GetReflectionTexture();
	virtual arl::TextureSource GetRefractionTexture();
	virtual common::SharedPtr<arl::Texture2D> GetSharedTexture();
	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<SharedTexture> texture);
	virtual math::matrix GetUVTransform();
	virtual void SetState(int32_t state);

	uint8_t reserved[0xc4];
};

/* background_image.vp: the quad placed by MVP, its texture coordinates by UVTransform (the
   program's path and parameters Sony's, the class name ours) */
class BackgroundImageVertShader : public arl::ShaderInterfaceBase {
public:
	BackgroundImageVertShader() : m_mvp(m_setup), m_uv_transform(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_transform.init("UVTransform", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_uv_transform.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/drawobj/background_image.vp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* background_image.vp built by psp2cgc */
	static const uint8_t s_binary[344];

	arl::ShaderParameter<math::matrix> m_mvp;
	/* the texture coordinates' origin (x, y) and size (z, w) */
	arl::ShaderParameter<math::v4> m_uv_transform;
};

/* background_image.fp: the texture (ColorMap) as it is */
class BackgroundImageFragShader : public arl::ShaderInterfaceBase {
public:
	BackgroundImageFragShader() : m_color_map(m_setup)
	{
	}

	virtual void Init()
	{
		m_color_map.init("ColorMap", 0, 0);
	}

	virtual void Apply()
	{
		m_color_map.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/drawobj/background_image.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* background_image.fp built by psp2cgc */
	static const uint8_t s_binary[264];

	arl::ShaderParameter<arl::TextureSource> m_color_map;
};

/* a background that draws one image, G_PixelJunk.dds from the background folder, over the
   screen, and whose reflections and refractions sample it; everything is made the first time
   it is used. 0x210 bytes */
class ProbeBackground : public Background {
public:
	/* 0x810cf5e0 */
	ProbeBackground();

	virtual void Load(const string &dir);
	virtual void Draw();
	virtual arl::TextureSource GetReflectionTexture();
	virtual arl::TextureSource GetRefractionTexture();
	virtual common::SharedPtr<arl::Texture2D> GetSharedTexture();
	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<SharedTexture> texture);
	virtual math::matrix GetUVTransform();
	virtual void SetState(int32_t state);

	/* 0x810cf154: the quad, the shaders and the image, made once (name ours) */
	void Prepare();

	arl::TextureSource m_image;
	PageTurnQuad m_quad;
	arl::ShaderInstance<BackgroundImageVertShader, arl::VertShaderObject> m_vert;
	arl::ShaderInstance<BackgroundImageFragShader, arl::FragShaderObject> m_frag;
	/* where the image is drawn, from 0 to 1 over the screen, and the part of it drawn */
	math::v2 m_position_min;
	math::v2 m_position_max;
	math::v2 m_uv_min;
	math::v2 m_uv_max;
	common::SharedPtr<SharedTexture> m_shared_texture;
	bool m_loaded;
};

/* rect2env.vp: the quad's corners and texture coordinates, no parameters (the program's
   path Sony's, the class name ours) */
class Rect2EnvVertShader : public arl::ShaderInterfaceBase {
public:
	virtual void Init()
	{
	}

	virtual void Apply()
	{
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/drawobj/rect2env.vp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* rect2env.vp built by psp2cgc */
	static const uint8_t s_binary[296];
};

/* rect2env.fp: the screen (Source) mapped onto the sphere through Rect2SphereMap and blended
   with the previous capture (Previous, read PreviousOffset away, by PreviousBlend) */
class Rect2EnvFragShader : public arl::ShaderInterfaceBase {
public:
	Rect2EnvFragShader()
		: m_source(m_setup), m_rect2sphere_map(m_setup), m_previous(m_setup),
		  m_previous_offset(m_setup), m_previous_blend(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 2, 0);
		m_rect2sphere_map.init("Rect2SphereMap", 3, 0);
		m_previous.init("Previous", 4, 0);
		m_previous_offset.init("PreviousOffset", arl::ShaderParameterBase::UNIFORM, 0);
		m_previous_blend.init("PreviousBlend", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_rect2sphere_map.Apply();
		m_previous.Apply();
		m_previous_offset.Apply();
		m_previous_blend.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/drawobj/rect2env.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* rect2env.fp built by psp2cgc */
	static const uint8_t s_binary[504];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<arl::TextureSource> m_rect2sphere_map;
	arl::ShaderParameter<arl::TextureSource> m_previous;
	arl::ShaderParameter<math::v2> m_previous_offset;
	arl::ShaderParameter<math::v4> m_previous_blend;
};

/* the background that captures the screen into a sphere map each frame, blended with the
   frame before, for the reflections, and copies the screen into the shared texture for the
   refractions; 0x268 bytes */
class EnvAuto : public Background {
public:
	/* 0x810cd0a8 */
	EnvAuto();

	virtual void Load(const string &dir);
	virtual void Draw();
	virtual arl::TextureSource GetReflectionTexture();
	virtual arl::TextureSource GetRefractionTexture();
	virtual common::SharedPtr<arl::Texture2D> GetSharedTexture();
	virtual void Init(common::SharedPtr<Camera> camera, common::SharedPtr<SharedTexture> texture);
	virtual math::matrix GetUVTransform();
	virtual void SetState(int32_t state);

	/* 0x810cc918: the map from the screen to the sphere, made again when the angle changed
	   (name ours) */
	void UpdateSphereMap();
	/* 0x810cce66: a texture of the size the capture is drawn into (name ours) */
	void CreateCaptureTexture(common::SharedPtr<arl::Texture2D> &texture, arl::SurfaceSize size);
	/* 0x810cdc3a */
	void SetTextureOps(common::SharedPtr<TextureOps> texture_ops);
	/* 0x810cdd1e */
	void captureReflections();
	/* 0x810ce9e8 */
	void CopyScreenForRefractions();

	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<SharedTexture> m_shared_texture;
	/* the capture of this frame, and of the frame before, which the next is blended with */
	common::SharedPtr<arl::Texture2D> m_reflections;
	common::SharedPtr<arl::Texture2D> m_previous;
	/* what refractions sample: the first capture texture */
	common::SharedPtr<arl::Texture2D> m_refractions;
	arl::FrameBufferObject m_fbo;
	common::SharedPtr<TextureOps> m_texture_ops;
	/* 8-byte aligned in Sony's layout (0x90, after the texture ops at 0x84) */
	arl::ShaderInstance<Rect2EnvVertShader, arl::VertShaderObject> m_vert
		__attribute__((aligned(8)));
	arl::ShaderInstance<Rect2EnvFragShader, arl::FragShaderObject> m_frag;
	/* a texture nothing sets or reads */
	common::SharedPtr<arl::Texture2D> m_spare_texture;
	/* how much of the sphere the screen covers, and the angle the sphere map was made for */
	float m_angle;
	common::SharedPtr<arl::Texture2D> m_sphere_map;
	float m_sphere_angle;
	/* how much of the previous capture each capture keeps */
	float m_blend;
	/* no capture is made while set; nothing sets it */
	bool m_disabled;
};

}
}

#endif
