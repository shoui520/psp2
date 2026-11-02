data dword USE0;
data dword USE1;
data dword DMA0_0;
data dword DMA0_1;
data dword DMA1_0;
data dword DMA1_1;
tstz p0, DMA0_0
p0 bra kick
movs doutd, DMA0_0, DMA0_1
tstz p0, DMA1_0
p0 bra kick
movs doutd, DMA1_0, DMA1_1
kick:
if0 movs doutu, USE0, USE1, DMA0_1
halt
