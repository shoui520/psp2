/* paf::common_internal: the failure hook every paf component calls on a kernel error. */
#ifndef PAF_COMMON_COMMON_INTERNAL_H
#define PAF_COMMON_COMMON_INTERNAL_H

namespace paf {
namespace common_internal {

#ifdef PAF_BUILD_2014
inline void on_error()
{
}
#else
void on_error();
#endif

}
}

#endif
