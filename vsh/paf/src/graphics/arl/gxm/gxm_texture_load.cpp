/*
 * A 2D texture loaded from an image file, and a texture source loaded from one as a surface's
 * texture or a texture of its own (the file's name ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <file/localfile.h>
#include <file/openurl.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/arl_util.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/color.h>
#include <graphics/gutil.h>
#include <image/image.h>
#include <std/string>
#include <std/vector>

using namespace paf;
using namespace paf::graph::arl;

/* the block-compressed formats are not loaded, their texture left as it was */
bool ScePafGraphics_7C53C58A(common::SharedPtr<Texture2D> texture, const string &filename,
			     SurfaceFlags flags)
{
	vector<uint8_t> buffer;

	if (!load(buffer, filename, true, true))
		return false;

	common::SharedPtr<image::Image> image =
		image::Image::Open(buffer.ptr, buffer.count, NULL, image::ImageFormat_None);

	if (image.get() == NULL)
		return false;

	image->GetExtent();

	int32_t width = image->GetExtent().w;
	int32_t height = image->GetExtent().h;
	uint32_t levels = image->GetArraySize().x;
	ImageMode mode = image->GetMode();

	if (image->GetMode() != ImageMode_DXT1 && image->GetMode() != ImageMode_DXT3 &&
	    image->GetMode() != ImageMode_DXT5) {
		ScePafGraphics_C4BD3558(texture.get(), width, height, mode, levels, flags);
		for (uint32_t level = 0; level < levels; ++level) {
			image::ImagePoint pos(level, 0);

			image->SetArrayPos(pos);

			image::ImageBuffer data = image->ToBuffer(true);

			ScePafGraphics_309D2902(texture.get(), level, width, height, mode, levels,
						data.buff);
			width >>= 1;
			height >>= 1;
		}
	}
	texture->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, levels > 1);
	texture->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	texture->setWrapS(TEXTURE_WRAP_CLAMP_FULL_BORDER);
	texture->setWrapT(TEXTURE_WRAP_CLAMP_FULL_BORDER);
	return true;
}

void TextureSource::Load(const string &filename, bool as_surface)
{
	ArlInstance *gl = Gl();

	m_texture.reset();
	m_surface.clear();
	if (!as_surface) {
		m_texture = common::SharedPtr<Texture2D>(new Texture2D(gl->m_default_allocator));
		ScePafGraphics_7C53C58A(reinterpret_cast<common::SharedPtr<Texture2D> &>(m_texture),
					filename, SurfaceFlags());
	} else {
		OpenUrl opener;

		opener.RegisterOpenFunc(LocalFile::OpenUrl);

		graph::Surface::LoadOption option;
		int32_t error;
		common::SharedPtr<File> file;

		file = opener.Open(filename.c_str(), 1, 0, &error);
		m_surface = graph::Surface::Load(gutil::GetDefaultSurfacePool(), file, &option);
		m_surface->Texturize();
	}
}
