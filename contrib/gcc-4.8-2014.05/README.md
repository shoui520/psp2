CodeSourcery GCC 4.8-2014.05 runtime
====================================

The GCC Sony's compiler was built from. Sony's GCC names itself
`4.8.3 20140320 (SCEI rev. 5.3.3)`, and its libgcc was built in Sony's tree under
`sys/binary/current/psp2-gcc/src/release/arm-csl-2014q1-scei-5.3.3`, from a "combined"
CodeSourcery source tree, for `armv7a-scei-eabi`, multilib `thumb` (the DWARF of the SDK's
libraries). This release, Sourcery CodeBench Lite 2014.05-28, is GCC 4.8.3 20140320
(`gcc/BASE-VER`, `gcc/DATESTAMP`; `gcc/DEV-PHASE` is `prerelease`, which Sony's build
replaced with its own revision).

Source archive: `gcc-2014.05-28.tar.bz2`, the GCC part of Mentor's
`arm-2014.05-28-arm-none-eabi.src.tar.bz2`, kept in Fedora's lookaside cache since
Mentor's download server is gone:
`https://src.fedoraproject.org/repo/pkgs/arm-none-eabi-gcc-cs/gcc-2014.05-28.tar.bz2/f17815dccade0fea6e37ebcc9aca6e2f/gcc-2014.05-28.tar.bz2`

MD5 `f17815dccade0fea6e37ebcc9aca6e2f`, SHA-256
`40a4a23215c77a0bb2032f8862f43b40a34d8ed261b6f6a551ce88c7a1a470c7`; the tree is
`gcc-4.8-2014.05/` in the archive. `toolchains/build-csl.sh` builds the compiler from the
same archive.

Only the files the project's runtime builds read are kept, unchanged (the set the
compilers' dependency output lists for every consumer): from `libgcc/`, `libgcc2.c`,
`libgcc2.h`, `longlong.h`, `unwind-arm-common.inc` and, in `config/arm/`, `lib1funcs.S`
with the `bpabi.S`, `bpabi-v6m.S`, `ieee754-df.S` and `ieee754-sf.S` it includes,
`libunwind.S`, `bpabi.c`, `bpabi-lib.h`, `unwind-arm.c`, `unwind-arm.h` and
`pr-support.c`; `gcc/coretypes.h`, `gcc/tsystem.h` and `gcc/ginclude/unwind-arm-common.h`;
`include/ansidecl.h`, `filenames.h` and `hashtab.h`; and the upstream license files (root
and `include/`). Every other file of the archive is left out. A routine that needs another
file adds it from the same archive. Generated configuration headers and compiled artifacts
belong in build directories.

Runtime licensing is described by `COPYING3` and `COPYING.RUNTIME`; individual
files retain their upstream notices. Other imported files have their own
notices, with the corresponding root copying documents preserved.
