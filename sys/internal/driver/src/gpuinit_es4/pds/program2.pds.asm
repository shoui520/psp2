data dword DMA0;
data dword DMA1;
data dword DMA2;
data dword DMA3;
data dword USE0;
data dword USE1;
data dword USE2;
movs douta, ir0, DMA0
movs douta, ir1, DMA1
movs douta, DMA2, DMA3
movs douta, DMA2, DMA3
movs douta, DMA2, DMA3
movs doutu, USE0, USE1, USE2
halt
