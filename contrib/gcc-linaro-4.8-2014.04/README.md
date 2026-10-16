GCC Linaro 4.8-2014.04 runtime candidate
======================================

Source repository: https://gcc.gnu.org/git/gcc.git

Upstream tag: `refs/vendors/linaro/tags/gcc-linaro-4.8-2014.04`

Tag object: `bcd56b91d79cab51d5f70fc614d2b0a094a06105`

Peeled commit: `e0368b47b0bc030fd2fa907bb9c73eabc7b7d3a6`

Only the files the project's runtime builds read are kept, unchanged (the set the
compilers' dependency output lists for every consumer, checked with both the vitasdk
build and Sony's compiler): from `libgcc/`, `libgcc2.c`, `libgcc2.h`, `longlong.h`,
`unwind-arm-common.inc` and, in `config/arm/`, `lib1funcs.S` with the `bpabi.S`,
`bpabi-v6m.S`, `ieee754-df.S` and `ieee754-sf.S` it includes, `libunwind.S`, `bpabi.c`,
`bpabi-lib.h`, `unwind-arm.c`, `unwind-arm.h` and `pr-support.c`; `gcc/coretypes.h`,
`gcc/tsystem.h` and `gcc/ginclude/unwind-arm-common.h`; `include/ansidecl.h`,
`filenames.h` and `hashtab.h`; and the upstream license files (root and `include/`).
Every other file of the tag is left out. A routine that needs another file adds it from
the same tag. Generated configuration headers and compiled artifacts belong in build
directories.

Runtime licensing is described by `COPYING3` and `COPYING.RUNTIME`; individual
files retain their upstream notices. Other imported files have their own
notices, with the corresponding root copying documents preserved.

This is a compiler-runtime candidate matching the project's Linaro compiler
release. It is not a claim that Sony used an identical runtime. Each adopted
routine must first be compared against the relevant original firmware.
