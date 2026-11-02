data dword DMA0;
data dword DMA1;
data dword USE0;
data dword USE1;
data dword USE2;
tstz p0, ir0
p0 bra dma
bra kick
dma:
movs doutd, DMA0, DMA1
kick:
movs doutu, USE0, USE1, USE2
halt
