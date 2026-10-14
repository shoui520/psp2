#ifndef _COMMON_RUNTIME_I64OPS_H
#define _COMMON_RUNTIME_I64OPS_H

/* SNC runtime 64-bit helpers (libsnc) */
long long __ashldi3(long long value, long long count);
unsigned long long __lshrdi3(unsigned long long value, long long count);
unsigned long long __u64_shift_right_1bit(unsigned long long value);
long long __i64_shift_right_1bit(long long value);
unsigned long long __udivdi3(unsigned long long n, unsigned long long d);
long long __divdi3(long long n, long long d);
unsigned long long __umoddi3(unsigned long long n, unsigned long long d);
long long __moddi3(long long n, long long d);

#endif /* _COMMON_RUNTIME_I64OPS_H */
