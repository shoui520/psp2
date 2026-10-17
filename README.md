# psp2
Reimplementation of system software for psp2.

This project aims to fully reverse engineer and reimplement these psp2 firmwares -   
Names like the "TOOL" or "DEBUGGER" suffixes are mine/whatever: 
* 4.00 (from PDEL internal system debugger backup bank. Only `os0` and `slb2` available)
* 3.650.011-TOOL (PDEL devkit, `os0`, `slb2`, `pup`, `vs0`,`vs0_tarpatch`)
* 3.630.011-DEBUGGER (PDEL internal system debugger, `os0`, `slb2`, `pup`, `vs0`,`vs0_tarpatch`)
* 3.65-CEX (retail, `os0`, `slb2`, `pup`, `vs0`,`vs0_tarpatch`)

Extra type names and strings have been taken from the `0.990.030-I` internal system debugger firmware. 

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
  `sys/sdk/target/include/` the SDK headers we write, under Sony's SDK names (`<libhmac.h>`,
  `<kernel/error.h>`); a header only one component uses sits in that component.
* `sys/sdk/target/src/`: SDK libraries modules link (`libsnc`, SNC's runtime).
* `cmake/`, `tools/`, `contrib/` (pristine upstream code), `toolchains/` (the compilers'
  recipes and `toolchains/libgcc`, the GCC runtime routines LibKernel links).

For example, `os0:kd/acmgr.skprx` is built from `sys/internal/kernel/src/acmgr/`:
```
cmake -S . -B build -DFW=4.00-DEBUGGER && cmake --build build
```
builds every module of that firmware; each lands in `build/<partition>/<path>/`
(`build/os0/kd/acmgr/`), so it can (*hopefully*) be used in place of the original on PSP2.
## Current status
To summarize:
* `os0`: substantial C reimplementation exists for nearly all modules. Not complete and not exact yet. No progress so far on MeP modules.
* `slb2`: WIP.
* `pup`: Not started. this is for `psp2swu`. `pup/cui_setupper` is identical to `os0:ue/cui_setupper`.
* `vs0`: WIP. A little progress made. `os0:ue/safemode.elf` will be deferred until `vs0` becomes the priority.
* `pd0:app/NPXS10007`: Not started
* `security/first_loader`: Not started
* `ernie`: Not started
* `cp/` (devkit communication processor): Not started

Contributions welcome!
## AI disclosure
This was done using Claude and Codex.  
## Special Thanks
Thanks to all the people who made this possible:  
Mathieulh, CreepNT, CelesteBlue, Princess-of-Sleeping, SKGleba, isage, SonicMastr, bythos14, dots-tb, cuevavirus, xyzz, DaveeFTW, ProximaV, GrapheneCt, zecoxao, xerpi, LiEnby, Electry, TheOfficialFloW and yifanlu