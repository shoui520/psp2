#ifndef PAF_COMMON_HALT_ASSERT_H
#define PAF_COMMON_HALT_ASSERT_H

#define printf_enabled _ZN3paf15common_internal14printf_enabledEv
#define on_error _ZN3paf15common_internal8on_errorEv
#define CommonDebugBreak ScePafCommon_E19DFE47

int printf_enabled(void);
void on_error(void);
void CommonDebugBreak(void);

#endif
