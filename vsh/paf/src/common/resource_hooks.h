/* Framework callbacks used while opening common resource templates. */
#ifndef PAF_COMMON_RESOURCE_HOOKS_H
#define PAF_COMMON_RESOURCE_HOOKS_H

#include "../versatileparam/idparam.h"

typedef struct Widget Widget;
typedef struct Surface Surface;

/* The returned intrusive_ptr<graph::Surface> occupies one word. */
typedef struct SurfaceRef {
	Surface *m_ptr;
} SurfaceRef;

typedef int (*CommonTemplateOpenFunction)(Widget *parent, const IDParam *id);
typedef const uint16_t *(*CommonStringFunction)(const IDParam *id);
typedef SurfaceRef *(*CommonTextureFunction)(SurfaceRef *result, const IDParam *id);

CommonTemplateOpenFunction SetCommonTemplateOpenFunction(CommonTemplateOpenFunction func);
CommonStringFunction SetCommonStringFunction(CommonStringFunction func);
CommonTextureFunction SetCommonTextureFunction(CommonTextureFunction func);

#define CommonTemplateOpen ScePafCommon_CAAB7E61
#define CommonGetString ScePafCommon_275A7FD9
#define CommonGetTexture ScePafCommon_F820A54B

int CommonTemplateOpen(Widget *parent, const IDParam *id);
const uint16_t *CommonGetString(const IDParam *id);
SurfaceRef *CommonGetTexture(SurfaceRef *result, const IDParam *id);

#endif
