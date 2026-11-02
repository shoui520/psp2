data dword DMA0;
data dword DMA1;
data dword USE1;
data dword USE2;
data dword USE0_TA;
data dword USE0_3D;
data dword USE0_2D;
data dword USE0_SPM;
data dword USE0_EVENT;
temp dword t0 = ds0[48];
temp dword t1 = ds1[48];
temp dword t2 = ds1[49];
movs douta, ir0, DMA0
movs douta, ir1, DMA1
and t0, ir1, 0xff00ffff
and t1, t0, 0x40
tstz p0, t1
p0 bra ta
bra event
ta:
and t1, ir0, 0x1000
tstz p0, t1
p0 bra threed
tstz p0, t0
p0 xor t1, ir0, t1
p0 tstz p0, t1
p0 bra ta_kick
bra event
ta_kick:
movs doutu, USE0_TA, USE1, USE2
halt
threed:
and t1, t0, 0x20
tstz p0, t1
p0 and t2, ir0, 0xc
p0 tstz p0, t2
p0 bra twod
xor t1, t0, t1
tstz p0, t1
p0 xor t2, ir0, t2
p0 tstz p0, t2
p0 bra threed_kick
bra event
threed_kick:
movs doutu, USE0_3D, USE1, USE2
halt
twod:
and t1, ir0, 0x10
tstz p0, t1
p0 bra spm
tstz p0, t0
p0 xor t1, ir0, t1
p0 tstz p0, t1
p0 bra twod_kick
bra event
twod_kick:
movs doutu, USE0_2D, USE1, USE2
halt
spm:
and t1, ir0, 0x320
tstz p0, t1
p0 bra event
tstz p0, t0
p0 xor t1, ir0, t1
p0 tstz p0, t1
p0 bra spm_kick
bra event
spm_kick:
movs doutu, USE0_SPM, USE1, USE2
halt
event:
movs doutu, USE0_EVENT, USE1, USE2
halt
