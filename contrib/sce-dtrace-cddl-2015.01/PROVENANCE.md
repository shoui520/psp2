Sony's DTrace source under the CDDL, 2015/01
============================================

Upstream: the libdtrace source package of the PlayStation Vita SDK,
`dtrace-cddl-src.zip` (SHA-256
`226ef0b7077078f982a3f13d5f84c69951e73320184538e0b119919d77abb08c`), the CDDL
sources Sony released for its DTrace tools, at their 2015/01 revision (the
package's own change log). Sony based them on Apple's dtrace-78 and
xnu-1456.1.26, OpenSolaris ON build 96 and the OpenSolaris ARM gate's
`elf_ARM.h`; `dtracep_syscalls_cddl.c` and `.h` are Sony's own, released under
the CDDL.

The same sources are the DTrace core of `os0:kd/deci4p_dtracep`: the module's
debug records name `dtrace/cddl/dtrace.c`, `dtrace/cddl/fasttrap_isa.c`,
`dtrace/cddl/sdt.c`, `dtrace/cddl/taskq.c`. The driver files keep the path
of Sony's driver folder (`driver/src/dfmgr/dtracep`), and the headers sit in
an include root of their own (`include/dtrace/cddl`, included as
`<sys/dtrace.h>`).

Kept, unchanged (the package path on the right):

| Here | Package |
|---|---|
| `cddl.txt` | `cddl.txt` |
| `driver/src/dfmgr/dtracep/dtrace/cddl/*.c` (10 files) | `dtrace/cddl/*.c` |
| `driver/src/dfmgr/dtracep/dtracep_syscalls_cddl.c`, `.h` | `dtracep_syscalls_cddl.c`, `.h` |
| `include/dtrace/cddl/sys/*.h` (13 files) | `sys/*.h` |

Left out: the host tools' sources (`dtrace/libdtrace`, `libdtrace1`, `libctf`,
`libelf`, `libproc`, `head`, `dtrace/sys`), `scripts/unistd.d`, the package's
readme and change log, and `diff.txt` (Sony's changes against the upstream
releases above, as one diff).

What Sony's firmware build changed beyond this package lives with the module,
in `sys/internal/driver/src/dfmgr/dtracep`.
