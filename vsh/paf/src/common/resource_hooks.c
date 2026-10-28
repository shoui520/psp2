#include "resource_hooks.h"

static CommonTemplateOpenFunction s_template_open;
static CommonStringFunction s_get_string;
static CommonTextureFunction s_get_texture;
static const uint16_t s_empty_string = 0;

CommonTemplateOpenFunction SetCommonTemplateOpenFunction(CommonTemplateOpenFunction func)
{
	s_template_open = func;
	return func;
}

CommonStringFunction SetCommonStringFunction(CommonStringFunction func)
{
	s_get_string = func;
	return func;
}

CommonTextureFunction SetCommonTextureFunction(CommonTextureFunction func)
{
	s_get_texture = func;
	return func;
}

int CommonTemplateOpen(Widget *parent, const IDParam *id)
{
	if (s_template_open)
		return s_template_open(parent, id);
	return -1;
}

const uint16_t *CommonGetString(const IDParam *id)
{
	if (s_get_string)
		return s_get_string(id);
	return &s_empty_string;
}

SurfaceRef *CommonGetTexture(SurfaceRef *result, const IDParam *id)
{
	if (s_get_texture)
		return s_get_texture(result, id);
	result->m_ptr = NULL;
	return result;
}
