/* Network debugger callbacks registered with SceSysrootForDriver. */
#pragma once

/* Sysroot forwards this interface without interpreting its contents. */
typedef int (*SysrootNetHook)(void *interface);

void SceSysrootForDriver_ECADF8C3(SysrootNetHook callback);
void SceSysrootForDriver_A799F22A(SysrootNetHook callback);
void SceSysrootForDriver_27D30DB2(SysrootNetHook callback);
void SceSysrootForDriver_D4457D4F(SysrootNetHook callback);

int SceSysrootForDriver_6C036B53(void *interface);
int SceSysrootForDriver_A9510EFE(void *interface);
int SceSysrootForDriver_CCB8BB5E(void *interface);
int SceSysrootForDriver_40F28DC6(void *interface);
