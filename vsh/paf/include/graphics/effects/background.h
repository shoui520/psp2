/*
 * The backgrounds the effects are drawn over: what each draws, the textures its reflections and
 * refractions sample, and the list the renderer keeps them in, which hands every call to the
 * background chosen. EnvAuto is Sony's name (its "EnvAuto::captureReflections" marker); the
 * interface, the other classes and every member name are ours. Only the list's code is
 * converted; the backgrounds' own is in 0x810cd0a8.. (EnvAuto), 0x810cf5e0.. and 0x810d151c..
 */
#ifndef PAF_GRAPHICS_EFFECTS_BACKGROUND_H
#define PAF_GRAPHICS_EFFECTS_BACKGROUND_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/camera.h>
#include <graphics/effects/post_effects.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <math/ngp/matrix.h>
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

/* a background lit from light probes (its Load reads grace_probe, beach_probe, sky_03 and
   ls01.tga); 0x210 bytes */
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

	uint8_t reserved[0x20c];
};

/* the background that captures the scene into its reflections and copies the screen for the
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

	/* 0x810cdc3a */
	void SetTextureOps(common::SharedPtr<TextureOps> texture_ops);
	/* 0x810cdd1e */
	void captureReflections();
	/* 0x810ce9e8 */
	void CopyScreenForRefractions();

	uint8_t reserved[0x264];
};

}
}

#endif
