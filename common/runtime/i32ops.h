#ifndef _COMMON_RUNTIME_I32OPS_H
#define _COMMON_RUNTIME_I32OPS_H

/* SNC runtime 32-bit division (libsnc i32ops.o, aeabi_div0.o) */
int __aeabi_idiv0(int return_value);
long long __aeabi_ldiv0(long long return_value);
unsigned int __udivsi3(unsigned int n, unsigned int d);
int __divsi3(int n, int d);
unsigned int __umodsi3(unsigned int n, unsigned int d);
int __modsi3(int n, int d);

#endif /* _COMMON_RUNTIME_I32OPS_H */
