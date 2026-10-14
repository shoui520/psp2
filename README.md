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
For the sake of simplicity, the project layout mirrors the PUP layout.  
For example, the following path contains the reimplementation of `os0:kd/acmgr.skprx`:  
```
src/os0/kd/acmgr/source/
```
This can be built back into a .skprx so it can (*hopefully*) be used in place of `os0:kd/acmgr.skprx` on PSP2.
## Current status
To summarize:
* `os0`: substantial C reimplementation exists for nearly all modules. Not complete and not exact yet. No progress so far on MeP modules.
* `slb2`: WIP.
* `pup`: Not started. this is for `psp2swu`. `pup/cui_setupper` is identical to `os0:ue/cui_setupper`.
* `vs0`: WIP. A little progress made. `os0:ue/safemode.elf` will be deferred until `vs0` becomes the priority.
* `pd0:app/NPXS10007`: Not started
* `first_loader`: Not started
* `ernie`: Not started
* `pup/devkit_cp`: Not started

Contributions welcome!
## AI disclosure
This was done using Claude and Codex.  
## Special Thanks
Thanks to all the people who made this possible:  
Mathieulh, CreepNT, CelesteBlue, Princess-of-Sleeping, SKGleba, isage, SonicMastr, bythos14, dots-tb, cuevavirus, xyzz, DaveeFTW, ProximaV, GrapheneCt, zecoxao, xerpi, LiEnby, Electry, TheOfficialFloW and yifanlu