# dlmalloc 2.8.4

Pristine upstream source: https://gee.cs.oswego.edu/pub/misc/malloc-2.8.4.c

Upstream identifies this file as version 2.8.4, dated May 27, 2009.
The source and its public-domain notice are preserved unchanged.

This is the base of the kernel heap in os0:kd/sysmem.skprx (Sony's
dlmalloc/malloc.c). The version follows from Sony's debug records: their line
numbers fit 2.8.4 plus Sony's insertions, and 2.8.5 already places
destroy_mspace after Sony's line for it. Sony's changes are kept separate in
src/os0/kd/sysmem/source/dlmalloc_sce.patch, applied at configure time.
