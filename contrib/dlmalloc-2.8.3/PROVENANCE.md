# dlmalloc 2.8.3

Pristine upstream source: https://gee.cs.oswego.edu/pub/misc/malloc-2.8.3.c

Upstream identifies this file as version 2.8.3, dated September 22, 2005.
The source and its public-domain notice are preserved unchanged.

This is a comparison baseline for the modified allocator in SceLibKernel.
It is not established as Sony's unique historical source release. Native
segment insertion follows the 2.8.3 algorithm, while native top padding and
some segment flags retain behavior found in 2.8.2. Runtime footer selection,
accounting, kernel memory allocation and lightweight mutexes also differ.
LibKernel builds selected reviewed routines from a configured copy with its
marked `heap_sce.patch` adaptation. Sony changes remain separate from this
pristine source; the other allocator APIs remain unselected until reviewed.
