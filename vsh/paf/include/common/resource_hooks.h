/*
 * The framework's hooks for common resources, C++ view. The hooks are still C
 * (src/common/resource_hooks.c), defined under these Sony names.
 */
#ifndef PAF_COMMON_RESOURCE_HOOKS_VIEW_H
#define PAF_COMMON_RESOURCE_HOOKS_VIEW_H

#include <stdint.h>
#include <std/memory>
#include <versatileparam/idparam.h>

namespace paf {
namespace graph {
class Surface;
}
namespace ui {
class Widget;
}
}

/* The hooks the framework sets once the common resource is loaded (C names). */
extern "C" {
typedef int32_t (*CommonTemplateOpenFunction)(paf::ui::Widget *parent, const paf::IDParam &id);
typedef const wchar_t *(*CommonStringFunction)(const paf::IDParam &id);
typedef paf::intrusive_ptr<paf::graph::Surface> (*CommonTextureFunction)(const paf::IDParam &id);

CommonTemplateOpenFunction SetCommonTemplateOpenFunction(CommonTemplateOpenFunction func);
CommonStringFunction SetCommonStringFunction(CommonStringFunction func);
CommonTextureFunction SetCommonTextureFunction(CommonTextureFunction func);
/* CommonTemplateOpen: the common resource's template with the ID opened under the parent by
   the hook; -1 without one */
int32_t ScePafCommon_CAAB7E61(paf::ui::Widget *parent, const paf::IDParam &id);
/* CommonGetTexture: the common resource's texture with the ID, by the hook */
paf::intrusive_ptr<paf::graph::Surface> ScePafCommon_F820A54B(const paf::IDParam &id);
}

namespace paf {
namespace common {

/* The string the framework's hook gives for id, or an empty one. */
const wchar_t *GetCommonString(const IDParam &id);

}
}

#endif
