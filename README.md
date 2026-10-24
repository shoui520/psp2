# psp2
Reimplementation of system software for psp2.

This project aims to fully reverse engineer and reimplement these psp2 firmwares -   
Names like the "TOOL" or "DEBUGGER" suffixes are mine/whatever: 
* 4.00 (from PDEL internal system debugger backup bank. Only `os0` and `slb2` available)
* 3.650.011-TOOL (PDEL devkit, `os0`, `slb2`, `pup`, `vs0`,`vs0_tarpatch`)
* 3.630.011-DEBUGGER (PDEL internal system debugger, `os0`, `slb2`, `pup`, `vs0`,`vs0_tarpatch`)
* 3.65-CEX (retail, `os0`, `slb2`, `pup`, `vs0`,`vs0_tarpatch`)

Extra type names, strings and paths not included in the above firmwares, were mined from Programmer Tool Runtime Library 3.570 and a collection of PUPs:

<details>
<summary>Click to view PUPs mined</summary>

**type2 E-tool (32)** - from Princess-of-Sleeping

```
PSP2UPDAT-type2-E-tool-0.931.010.PUP
PSP2UPDAT-type2-E-tool-0.931.010-2.PUP
PSP2UPDAT-type2-E-tool-0.940.050.PUP
PSP2UPDAT-type2-E-tool-0.945.040.PUP
PSP2UPDAT-type2-E-tool-0.945.040-2.PUP
PSP2UPDAT-type2-E-tool-0.990.080.PUP
PSP2UPDAT-type2-E-tool-0.990.140.PUP
PSP2UPDAT-type2-E-tool-0.995.070.PUP
PSP2UPDAT-type2-E-tool-0.996.070.PUP
PSP2UPDAT-type2-E-tool-0.996.090.PUP
PSP2UPDAT-type2-E-tool-0.996.090-2.PUP
PSP2UPDAT-type2-E-tool-1.000.041.PUP
PSP2UPDAT-type2-E-tool-1.000.071.PUP
PSP2UPDAT-type2-E-tool-1.000.071-2.PUP
PSP2UPDAT-type2-E-tool-1.030.071.PUP
PSP2UPDAT-type2-E-tool-1.030.071-2.PUP
PSP2UPDAT-type2-E-tool-1.040.011.PUP
PSP2UPDAT-type2-E-tool-1.050.021.PUP
PSP2UPDAT-type2-E-tool-1.060.031.PUP
PSP2UPDAT-type2-E-tool-1.500.151.PUP
PSP2UPDAT-type2-E-tool-1.500.151-2.PUP
PSP2UPDAT-type2-E-tool-1.510.021.PUP
PSP2UPDAT-type2-E-tool-1.510.021-2.PUP
PSP2UPDAT-type2-E-tool-1.520.011.PUP
PSP2UPDAT-type2-E-tool-1.520.011-2.PUP
PSP2UPDAT-type2-E-tool-1.600.061.PUP
PSP2UPDAT-type2-E-tool-1.600.061-2.PUP
PSP2UPDAT-type2-E-tool-1.610.061.PUP
PSP2UPDAT-type2-E-tool-1.660.011.PUP
PSP2UPDAT-type2-E-tool-1.670.011.PUP
PSP2UPDAT-type2-E-tool-1.690.011.PUP
PSP2UPDAT-type2-E-tool-1.691.011.PUP
```

**Other internal / devtool builds (6)** - from Princess-of-Sleeping

```
PSP2UPDAT-type2-I-0.995.000-devtool-release-qaf.PUP
PSP2UPDAT-type2-I-cex_for_tool-release-qaf-0.990.030.PUP
PSP2UPDAT.PUP-1_800._000.I.devtool.prerelease.type3.bricks.qaf
PSP2UPDAT.PUP-3_200._010.I.cex_for_tool.release.type3.qaf
PSP2UPDAT.PUP-type3-I-cex_for_tool-3.650.000.PUP
PSP2UPDAT.PUP-type3-I-tool-1.692.000-downver.PUP
```

**Internal System Debugger firmwares (4)** - downloaded from Darthsernie

```
0.940.040-I/PSP2UPDAT.PUP
3.600.011-I/PSP2UPDAT.PUP
3.630.011-I/PSP2UPDAT.PUP
3.720.011-I/PSP2UPDAT.PUP
```

</details>


## Layout
The source tree mirrors Sony's own, as far as Sony's binaries show it: folder and file names
come from the paths in their debug records, asserts and SDK libraries.
* `sys/`: the platform. `sys/internal/kernel/src/` (kernel core and managers),
  `sys/internal/driver/src/` (drivers), `sys/internal/usermode/src/` (user libraries),
  `sys/internal/deci/` (debug modules), `sys/internal/secure/` (the TrustZone-only modules;
  our folder name).
* `security/`: the secure processor's code (`security/sm`, the secure modules;
  `security/first_loader`). Sony keeps a `security` repository beside `sys` and `vsh` (the
  firmware's version file names all three, each with its own revision); what it holds is
  inferred.
* `vsh/`: the system software UI: `vsh/paf`, `vsh/cdlg/src/` (common dialogs),
  `vsh/lowlayer/src/`, `vsh/app/` (applications and plugins), `vsh/app/common/src/` (NP and
  other shared libraries).
* `compat/`: the PSP emulator (`vs0:app/NPXS10028`), which prints the branch of Sony's `compat`
  repository.
* `wp/` (Welcome Park, its own project in Sony's tree), `cp/` (devkit communication
  processor), `barkley/` (motion controller), `sys661/` (the PSP system tree: the ePSP boot
  loader in os0:kd/pcbc's payload), `third-party-libs-psp2/` (webcore), `ernie/` (syscon; our
  folder name).
* `release/`: what each firmware contains. `release/<FW>.list` lists its modules;
  `release/modules.tsv` gives the folder each module is built from and why it is there.
* Headers: `sys/internal/include/<component>/` holds the interfaces several modules share,
  under the component that owns them (`<threadmgr/threadcb.h>`, `<sysmem/sysroot.h>`);
  `sys/sdk/target/include/` the SDK headers we write, under Sony's SDK names (`<scetypes.h>`,
  `<kernel/types.h>`, `<libhmac.h>`, `<kernel/error.h>`); a header only one component uses sits
  in that component. What Sony's compilers build reads no vitasdk header: the C library headers
  are the SDK's (`<string.h>`, `<stdint.h>`, ...) in `sys/sdk/target/include`, and net_ps's
  NetBSD code reads NetBSD's own kernel headers (`net_ps/include`: the psp2 port's `machine/`
  and Sony's changes, the rest from `contrib/`). The user code of later roadmap stages, built
  with vitasdk's toolchain until its SDK headers are written, still reads vitasdk's user
  headers; `cmake/compat/vitasdk/` holds the shims that complete them.
* `sys/sdk/target/src/`: SDK libraries modules link (`libsnc`, SNC's runtime).
* `cmake/`, `tools/`, `contrib/` (pristine upstream code), `toolchains/` (the compilers'
  recipes and `toolchains/libgcc`, the GCC runtime routines LibKernel links).
* Exports are called by Sony's names where Sony's binaries give them, otherwise
  `<Library>_<NID>` (`SceSysmemForDriver_C94850C9`). `tools/nids/vitasdk.tsv` lists vitasdk's
  name for each of them (`grep C94850C9 tools/nids/vitasdk.tsv`), and each such declaration
  carries it in a comment.

For example, `os0:kd/acmgr.skprx` is built from `sys/internal/kernel/src/acmgr/`:
```
cmake -S . -B build -DFW=4.00-DEBUGGER && cmake --build build
```
builds every module of that firmware; each lands in `build/<partition>/<path>/`
(`build/os0/kd/acmgr/`) as the firmware ships it, `acmgr.skprx`: an fSELF (unencrypted) in
Sony's SELF layout with the original's auth ID (`tools/mkfself.py`), so it can (*hopefully*) be
used in place of the original on PSP2.
## Current status

The priority is: base kernel (`sys/internal/kernel`) -> full kernel (`sys/internal/drivers` and `os0:us` of sys/internal/usermode) -> ScePaf -> SceShell -> ?? anything else.  

To summarize:
* Kernel: substantial C reimplementation exists for nearly all `os0:kd` modules. Not complete and not exact yet. No progress so far on `os0:sm`/MeP/security modules.
* ScePaf: Not started
* SceShell: Not started
* `wp`: Not started
* `first_loader`: Not started
* `ernie`: Not started
* `cp` (devkit communication processor): Not started

Contributions welcome!

## Tiers
* T0: Inventoried
* T1: mapped
* T2: skeleton generated
* T3: C implementation in progress
* T4: Functionally identical
* T5: Byte identical

## AI disclosure
This was done using Claude and Codex.  
## Special Thanks
Thanks to all the people who made this possible:  
Mathieulh, CreepNT, CelesteBlue, Princess-of-Sleeping, SKGleba, isage, SonicMastr, bythos14, dots-tb, cuevavirus, xyzz, DaveeFTW, ProximaV, GrapheneCt, zecoxao, xerpi, LiEnby, Electry, TheOfficialFloW and yifanlu