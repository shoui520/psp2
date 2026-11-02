data dword DMA0;
data dword MASK0;
data dword MASK1;
data dword MASK2;
data dword USE0_0;
data dword USE0_1;
data dword USE1_0;
data dword USE1_1;
data dword USE2_0;
data dword USE2_1;
data dword USE3_0;
data dword USE3_1;
data dword USE2;
temp dword t = ds0[48];
movs douta, ir0, DMA0
and t, ir0, MASK0
tstz p0, t
p0 bra next1
movs doutu, USE0_0, USE0_1, USE2
halt
next1:
and t, ir0, MASK1
tstz p0, t
p0 bra next2
movs doutu, USE1_0, USE1_1, USE2
halt
next2:
and t, ir0, MASK2
tstz p0, t
p0 bra next3
movs doutu, USE2_0, USE2_1, USE2
halt
next3:
movs doutu, USE3_0, USE3_1, USE2
halt
