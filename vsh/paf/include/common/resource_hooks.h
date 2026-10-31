/*
 * The framework's hooks for common resources, C++ view. The hooks are still C
 * (src/common/resource_hooks.c), defined under these Sony names.
 */
#ifndef PAF_COMMON_RESOURCE_HOOKS_VIEW_H
#define PAF_COMMON_RESOURCE_HOOKS_VIEW_H

#include <versatileparam/idparam.h>

namespace paf {
namespace common {

/* The string the framework's hook gives for id, or an empty one. */
const wchar_t *GetCommonString(const IDParam &id);

}
}

#endif
