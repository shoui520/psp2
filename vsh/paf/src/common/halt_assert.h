#ifndef PAF_COMMON_HALT_ASSERT_H
#define PAF_COMMON_HALT_ASSERT_H

#define printf_enabled ScePafCommon_7DD63E0E
#define on_error _ZN3paf15common_internal8on_errorEv
#define CommonDebugBreak ScePafCommon_E19DFE47

int printf_enabled(void);
void on_error(void);
void CommonDebugBreak(void);

#endif
